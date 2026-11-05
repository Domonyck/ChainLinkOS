/**
 * @file session.c
 * @brief Implementation of the Session module declared in session.h.
 *
 * Layout of this file:
 *   1. The Session struct (its fields are hidden from every other file)
 *   2. create_session / destroy_session
 *   3. Small helpers: sending text, reading one line, the server's clock
 *   4. load_chain: picks the JSON file for the mode and a random chain (word_bank.h)
 *   5. Setup questions: choose the mode, then the coin flip
 *   6. play_blitz: the Blitz match loop (poll + the referee in blitz_mode.h)
 *   7. run_session: setup, then the match
 *
 * Why is the struct here and not in session.h? Keeping it out of the header
 * makes Session opaque: server.c can hold a Session * but cannot reach inside,
 * so no outside code can change s->mode or s->fds by accident.
 *
 * Who does what: game.c checks guesses and keeps the scores, blitz_mode.c is
 * the referee (turns and clocks), and this file only coordinates: it reads
 * the sockets, asks the referee, and tells each player what happened.
 */

/* Needed for clock_gettime and poll when compiling with -std=c99. */
#define _POSIX_C_SOURCE 200809L         // macos
// #define _POSIX_C_SOURCE 199309L

#include "session.h"
#include "word_bank.h"
#include "game.h"
#include "blitz_mode.h"

#include <errno.h>      /* errno, EINTR */
#include <limits.h>     /* INT_MAX */
#include <poll.h>       /* poll */
#include <stdarg.h>     /* va_list, for send_fmt */
#include <stdio.h>      /* printf, snprintf */
#include <stdlib.h>     /* calloc, free */
#include <string.h>     /* strlen, memchr, memmove */
#include <time.h>       /* clock_gettime */
#include <sys/socket.h> /* send, recv */
#include <sys/types.h>  /* ssize_t */

/* Data files, relative to the directory the server is started from (the repo root). */
#define CLASSIC_DATA_FILE "data/classicmode.json"
#define BLITZ_DATA_FILE   "data/blitzmode.json"

/* Longest line a player can send. Anything longer is thrown away. */
#define LINE_MAX_LEN 256

/* ======== THE STRUCT ======== */

/**
 * Everything one match needs to remember.
 */
struct Session {
    int fds[2];          /**< Sockets of player 0 and player 1. Not owned: never closed here. */
    GameMode mode;       /**< Classic or Blitz. */
    ListSource source;   /**< Who provides the list (only asked in Classic). */

    WordBank *bank;               /**< Loaded JSON data. Owned: freed in destroy_session. */
    const char *const *chain;     /**< Player 0's chain. Points INTO bank, so never freed on its own. */
    size_t chain_len;             /**< Number of words in chain. */
    const char *const *chain1;    /**< Player 1's chain (Blitz: a different chain from player 0's). Points INTO bank. */
    size_t chain1_len;            /**< Number of words in chain1. */

    Game game;           /**< Chains, hints and scores. Owned: freed in destroy_session. */
    Mode referee;        /**< The Blitz referee (turns and clocks). Owned: destroyed in destroy_session. */
};

/* ======== SETUP / TEARDOWN ======== */

Session *create_session(int fd0, int fd1) {
    /* calloc zeroes every byte, so mode and source start at their first enum value,
       and the Game and the referee start empty (safe to free). */
    Session *s = calloc(1, sizeof *s);

    if (s == NULL) return s;   /* out of memory: the caller checks for NULL */

    /* Remember the sockets (destination = source: the session's field gets the argument). */
    s->fds[0] = fd0;
    s->fds[1] = fd1;

    return s;   /* without this line the caller receives garbage */
}

void destroy_session(Session *s) {
    if (s == NULL) return;

    /* Order matters: the referee points at the game, so it goes first.
       The game copied its words, so the bank can go last. */
    destroy_blitz_mode(&s->referee);   /* safe if the referee was never created */
    free_game(&s->game);               /* safe on a zeroed Game */
    wb_free(s->bank);                  /* safe with NULL; also invalidates s->chain */
    free(s);
}

/* ======== SMALL HELPERS ======== */

/** Send a whole string, even if the network takes it in pieces. Errors are ignored:
    a dropped player is noticed when the loop next reads from that socket. */
static void send_text(int fd, const char *text) {
    size_t left = strlen(text);
    while (left > 0) {
        ssize_t n = send(fd, text, left, 0);
        if (n <= 0) return;
        text += n;
        left -= (size_t)n;
    }
}

/** printf-style send_text, for messages that contain numbers or names. */
static void send_fmt(int fd, const char *format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    send_text(fd, buf);
}

/** The server's own clock in milliseconds. Monotonic: never jumps when the
    computer's time is changed. Client clocks are never used. */
static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/** Lowercase a line in place and trim spaces from both ends. */
static void trim_lower(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';

    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    memmove(s, s + start, len - start + 1);

    for (size_t i = 0; s[i] != '\0'; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
}

/**
 * Read one line from a socket, waiting for it (used for the setup questions,
 * before any clock is running).
 *
 * @return 0 on success (line holds the text, without the newline),
 *         -1 if the player disconnected.
 */
static int read_line_blocking(int fd, char *line, size_t size) {
    size_t len = 0;
    for (;;) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return -1;
        if (c == '\n') break;
        if (len + 1 < size) line[len++] = c;   /* extra characters are dropped */
    }
    line[len] = '\0';
    return 0;
}

/* ======== WORD LIST ======== */

/**
 * Load the JSON file that matches the mode and pick a random chain from it.
 *
 * Blitz always uses the game master's list. Classic uses it unless a player
 * is supposed to write the list (not implemented yet).
 *
 * @return 0 on success, -1 on failure (a message is printed to stderr).
 */
static int load_chain(Session *s) {
    if (s->mode == MODE_CLASSIC && s->source == LIST_PLAYER) {
        fprintf(stderr, "Player-made lists are not implemented yet.\n");
        return -1;
    }

    const char *path = (s->mode == MODE_BLITZ) ? BLITZ_DATA_FILE : CLASSIC_DATA_FILE;

    /* Drop a previously loaded bank so calling this twice does not leak. */
    wb_free(s->bank);
    s->bank = wb_load(path);
    if (s->bank == NULL) {
        fprintf(stderr, "Could not load word list from %s (run the server from the repo root).\n", path);
        s->chain = NULL;
        s->chain_len = 0;
        s->chain1 = NULL;
        s->chain1_len = 0;
        return -1;
    }

    size_t count = wb_chain_count(s->bank);
    if (count == 0) {
        fprintf(stderr, "%s has no chains.\n", path);
        return -1;
    }

    if (s->mode == MODE_BLITZ) {
        /* Each player gets their own chain. The second index is picked from the
           chains that are left, so the two players never share one (unless the
           file has only a single chain). */
        size_t i0 = (size_t)rand() % count;
        size_t i1 = (count >= 2) ? (i0 + 1 + (size_t)rand() % (count - 1)) % count : i0;
        s->chain  = wb_get_chain(s->bank, i0, &s->chain_len);
        s->chain1 = wb_get_chain(s->bank, i1, &s->chain1_len);
    } else {
        s->chain = wb_get_random_chain(s->bank, &s->chain_len);
        s->chain1 = s->chain;
        s->chain1_len = s->chain_len;
    }

    return 0;
}

/* ======== SETUP QUESTIONS ======== */

/**
 * Ask player 0 which mode to play.
 *
 * @return 0 on success (s->mode is set), -1 if player 0 disconnected.
 */
static int ask_mode(Session *s) {
    send_text(s->fds[0], "\nChoose a mode: type 1 for Classic or 2 for Blitz\ninput: ");
    send_text(s->fds[1], "\nPlayer 0 is choosing the game mode...\n");

    char line[LINE_MAX_LEN];
    for (;;) {
        if (read_line_blocking(s->fds[0], line, sizeof line) != 0) {
            send_text(s->fds[1], "\nPlayer 0 left the game.\n");
            return -1;
        }
        trim_lower(line);

        if (strcmp(line, "1") == 0 || strcmp(line, "classic") == 0) { s->mode = MODE_CLASSIC; return 0; }
        if (strcmp(line, "2") == 0 || strcmp(line, "blitz") == 0)   { s->mode = MODE_BLITZ;   return 0; }

        send_text(s->fds[0], "Please type 1 or 2.\ninput: ");
    }
}

/**
 * The coin flip: a random player calls heads or tails, the server flips once,
 * and both players are told the result so they agree on who starts.
 *
 * @return The player who goes first (0 or 1), or -1 if someone disconnected.
 */
static int coin_flip(Session *s) {
    int caller = pick_caller();
    int other = 1 - caller;

    send_text(s->fds[caller], "\nYou call the coin. Type heads or tails\ninput: ");
    send_fmt(s->fds[other], "\nPlayer %d is calling the coin...\n", caller);

    CoinSide call = COIN_HEADS;
    char line[LINE_MAX_LEN];
    for (;;) {
        if (read_line_blocking(s->fds[caller], line, sizeof line) != 0) {
            send_fmt(s->fds[other], "\nPlayer %d left the game.\n", caller);
            return -1;
        }
        trim_lower(line);

        if (strcmp(line, "heads") == 0 || strcmp(line, "h") == 0) { call = COIN_HEADS; break; }
        if (strcmp(line, "tails") == 0 || strcmp(line, "t") == 0) { call = COIN_TAILS; break; }

        send_text(s->fds[caller], "Please type heads or tails.\ninput: ");
    }

    CoinSide flip = flip_coin();               /* flipped once, here, by the game master */
    int first = decide_first_player(caller, call, flip);

    for (int p = 0; p < 2; p++)
        send_fmt(s->fds[p], "\nPlayer %d called %s. The coin landed on %s. Player %d goes first.\n",
                 caller,
                 call == COIN_HEADS ? "heads" : "tails",
                 flip == COIN_HEADS ? "heads" : "tails",
                 first);
    return first;
}

/* ======== BLITZ MATCH ======== */

/**
 * Send each player their own view of their chain, their score, and the turn
 * signal ("YOUR TURN" for the player who plays, "waiting" for the other).
 *
 * @param turn_player  Whose turn it is now (Result.next_player).
 * @param out          Which players have run out of time (they just watch).
 */
static void send_views(Session *s, int turn_player, const bool out[2]) {
    for (int p = 0; p < 2; p++) {
        char *view = NULL;
        if (view_game(&s->game, p, &view) == GAME_OK) {
            send_fmt(s->fds[p], "\nYour chain:\n%s\nScore: %d   Opponent: %d\n",
                     view, get_score(&s->game, p), get_score(&s->game, 1 - p));
            free(view);   /* view_game returns a malloc'd string: the caller frees it */
        }

        if (out[p]) send_text(s->fds[p], "\n(your time is up - waiting for the match to end)\n");
        else        send_text(s->fds[p], turn_prompt(p, turn_player));
    }
}

/** Tell both players what a Result means for them (not used for GAME_OVER). */
static void announce(Session *s, Result r) {
    int other = 1 - r.player;

    switch (r.kind) {
    case RESULT_CORRECT:
        send_text(s->fds[r.player], "\nCorrect!\n");
        send_fmt(s->fds[other], "\nPlayer %d guessed correctly.\n", r.player);
        break;
    case RESULT_WRONG:
        send_text(s->fds[r.player], "\nWrong - a hint was added.\n");
        send_fmt(s->fds[other], "\nPlayer %d guessed wrong.\n", r.player);
        break;
    case RESULT_PLAYER_OUT:
        send_text(s->fds[r.player], "\nYour time ran out. You are out.\n");
        send_fmt(s->fds[other], "\nPlayer %d ran out of time. You play on alone.\n", r.player);
        break;
    default:
        break;
    }
}

/** Announce the end of the match: the reason, both scores, and who won. */
static void announce_game_over(Session *s, const char *reason) {
    int winner = s->referee.winner(s->referee.self);

    for (int p = 0; p < 2; p++) {
        send_fmt(s->fds[p], "\n=== GAME OVER ===\n%s\nScores: player 0 = %d, player 1 = %d\n",
                 reason, get_score(&s->game, 0), get_score(&s->game, 1));

        if (winner == -1)      send_text(s->fds[p], "It's a tie - both players win!\n");
        else if (winner == p)  send_text(s->fds[p], "You win!\n");
        else                   send_text(s->fds[p], "You lose.\n");
    }
}

/** Why a match ended, from the Result that ended it (forfeits are worded where they happen). */
static const char *game_over_reason(const Session *s, Result r) {
    if (is_game_finished(&s->game, r.player)) return "A player finished their whole list!";
    return "Both players ran out of time.";
}

/**
 * Handle one complete line typed by a player: the referee decides what it means.
 *
 * @return true if the match is over after this line.
 */
static bool handle_line(Session *s, int player, const char *line, bool out[2]) {
    Mode *m = &s->referee;
    Result r = m->on_guess(m->self, player, line, now_ms());

    if (r.kind == RESULT_IGNORED) {   /* not this player's turn */
        if (out[player]) send_text(s->fds[player], "\nYour time is up - you cannot play anymore.\n");
        else             send_text(s->fds[player], "\nNot your turn - please wait.\n");
        return false;
    }

    if (r.kind == RESULT_GAME_OVER) {
        announce_game_over(s, game_over_reason(s, r));
        return true;
    }

    if (r.kind == RESULT_PLAYER_OUT) out[r.player] = true;
    announce(s, r);
    send_views(s, r.next_player, out);
    return false;
}

/**
 * The Blitz match, from the coin flip to the final result.
 *
 * Every loop turn: read the clock once, ask the referee how long until the
 * next deadline, and sleep in poll() for at most that long. Wake up for a
 * guess, a disconnect, or the deadline.
 */
static void play_blitz(Session *s) {
    /* Each player gets their own chain. game_init copies the words. */
    if (game_init(&s->game, s->chain, s->chain_len, s->chain1, s->chain1_len) != GAME_OK) {
        fprintf(stderr, "Could not start the game.\n");
        return;
    }
    if (!create_blitz_mode(&s->referee, &s->game)) {
        fprintf(stderr, "Could not create the Blitz referee.\n");
        return;
    }

    int first = coin_flip(s);
    if (first < 0) return;   /* someone left during the coin flip */

    Mode *m = &s->referee;
    bool out[2] = { false, false };   /* players whose clock ran out */

    m->start(m->self, first, now_ms());
    send_views(s, first, out);

    char inbuf[2][LINE_MAX_LEN];      /* partly received lines, one per player */
    size_t inlen[2] = { 0, 0 };

    while (!m->is_over(m->self)) {
        long long wait = m->ms_until_deadline(m->self, now_ms());
        int timeout_ms = (wait > INT_MAX) ? INT_MAX : (int)wait;

        struct pollfd pfd[2] = { { s->fds[0], POLLIN, 0 }, { s->fds[1], POLLIN, 0 } };
        int ready = poll(pfd, 2, timeout_ms);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll() failed");
            return;
        }

        /* Nobody sent anything and the deadline arrived: the active clock ran out. */
        if (ready == 0) {
            Result r = m->on_timeout(m->self, now_ms());
            if (r.kind == RESULT_GAME_OVER) {
                announce_game_over(s, "Both players ran out of time.");
                return;
            }
            if (r.kind == RESULT_PLAYER_OUT) out[r.player] = true;
            announce(s, r);
            send_views(s, r.next_player, out);
            continue;
        }

        for (int p = 0; p < 2; p++) {
            if (!(pfd[p].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            ssize_t n = recv(s->fds[p], inbuf[p] + inlen[p], LINE_MAX_LEN - 1 - inlen[p], 0);
            if (n <= 0) {   /* the player disconnected: the other player wins */
                m->on_forfeit(m->self, p, now_ms());
                send_fmt(s->fds[1 - p], "\nPlayer %d left the game.\n", p);
                announce_game_over(s, "A player left the match.");
                return;
            }
            inlen[p] += (size_t)n;

            /* Take every complete line out of the buffer. */
            char *newline;
            while ((newline = memchr(inbuf[p], '\n', inlen[p])) != NULL) {
                *newline = '\0';
                size_t used = (size_t)(newline - inbuf[p]) + 1;

                if (handle_line(s, p, inbuf[p], out)) return;   /* match over */

                memmove(inbuf[p], inbuf[p] + used, inlen[p] - used);
                inlen[p] -= used;
            }

            if (inlen[p] >= LINE_MAX_LEN - 1) inlen[p] = 0;   /* a line this long is junk: drop it */
        }
    }
}

/* ======== RUNNING ======== */

void run_session(Session *s) {
    if (s == NULL) return;

    printf("fd0: %d\n", s->fds[0]);
    printf("fd1: %d\n", s->fds[1]);

    /* Setup: which mode? */
    if (ask_mode(s) != 0) return;

    /* Pick the word chain for this match. */
    if (load_chain(s) != 0) {
        send_text(s->fds[0], "\nThe server could not load the word list.\n");
        send_text(s->fds[1], "\nThe server could not load the word list.\n");
        return;
    }

    printf("Mode: %s. Player 0 chain (%zu words):", s->mode == MODE_BLITZ ? "Blitz" : "Classic", s->chain_len);
    for (size_t i = 0; i < s->chain_len; i++) printf(" %s", s->chain[i]);
    printf("\n");
    if (s->mode == MODE_BLITZ) {
        printf("Player 1 chain (%zu words):", s->chain1_len);
        for (size_t i = 0; i < s->chain1_len; i++) printf(" %s", s->chain1[i]);
        printf("\n");
    }

    if (s->mode == MODE_BLITZ) {
        play_blitz(s);
    } else {
        /* Classic is not on the referee interface yet. */
        send_text(s->fds[0], "\nClassic mode is not available yet.\n");
        send_text(s->fds[1], "\nClassic mode is not available yet.\n");
    }
}