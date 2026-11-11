#define _POSIX_C_SOURCE 200809L    // macos
// #define _POSIX_C_SOURCE 199309L

#include "session.h"
#include "word_bank.h"
#include "game.h"
#include "blitz_mode.h"
#include "classic_mode.h"
#include "ascii_art.h"
#include <errno.h>     
#include <limits.h>   
#include <poll.h>      
#include <stdarg.h>    
#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <time.h>      
#include <sys/socket.h> 
#include <sys/types.h>  

#define CLASSIC_DATA_FILE "data/classicmode.json"
#define BLITZ_DATA_FILE   "data/blitzmode.json"

#define LINE_MAX_LEN 256

#define TIME_HEADER_FMT "⏳ Time Left: %llds | Your chain:"
#define BANNER \
    "\033[0;97m  ██▀▀▀▀▀\033[0;37m \033[0;97m  ██   ██\033[0;37m  \033[0;97m  ██▀▀▀██\033[0;37m  \033[0;97m  ▀██▀\033[0;37m  \033[0;97m  ██▀▀▀▄▄\033[0;37m  \033[0;97m  ██      \033[0;97m  ▀██▀\033[0;37m  \033[0;97m  ██▀▀▀▄▄\033[0;37m  \033[0;97m  ██ \033[0;37m \033[0;97m ██\033[0;37m  \033[0m\n" \
    "\033[0;37m█\033[0;97m ██\033[0;37m ██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n" \
    "\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m█\033[0m\n" \
    "\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██▀▀▀██ \033[0;37m██\033[0;97m ███████ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██▀▀▀▄▄ \033[0;37m█\033[0m\n" \
    "\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m▄\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n" \
    "\033[0;37m█\033[0;97m ██\033[0;37m ██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n" \
    "\033[0;37m█\033[0;97m ██ \033[0;37m██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█ \033[0;97m██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█ \033[0;97m██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n" \
    "\033[0;37m█\033[0;97m ██\033[0;37m ██████\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m███████\033[0;97m ██ \033[0;37m███\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m██\033[0;97m ██ \033[0;37m█\033[0;97m ██ \033[0;37m█\033[0m\n" \
    "\033[0;37m▀\033[0;97m ██▄▄▄▄▄\033[0;37m ▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m▀▀\033[0;97m ▄██▄ \033[0;37m▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██\033[0;37m ▀▀\033[0;97m ██▄▄▄▄ \033[0;37m▀▀\033[0;97m ▄██▄ \033[0;37m▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██\033[0;37m ▀▀\033[0;97m ██ \033[0;37m▀\033[0;97m ██ \033[0;37m▀\033[0m\n"

struct Session {
    int fds[2];         
    GameMode mode;       
    ListSource source;   

    WordBank *bank;               
    const char *const *chain;     
    size_t chain_len;             
    const char *const *chain1;   
    size_t chain1_len;           

    Game game;           
    Mode referee;      
};


Session *create_session(int fd0, int fd1) {
  
    Session *s = calloc(1, sizeof *s);

    if (s == NULL) return s;  
    s->fds[0] = fd0;
    s->fds[1] = fd1;

    return s;  
}

void destroy_session(Session *s) {
    if (s == NULL) return;

    if (s->mode == MODE_BLITZ) {
        destroy_blitz_mode(&s->referee);  
    } else {
        destroy_classic_mode(&s->referee);
    }
    free_game(&s->game);              
    wb_free(s->bank);                 
    free(s);
}
static void send_text(int fd, const char *text) {
    size_t left = strlen(text);
    while (left > 0) {
        ssize_t n = send(fd, text, left, 0);
        if (n <= 0) return;
        text += n;
        left -= (size_t)n;
    }
}
static void send_fmt(int fd, const char *format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    send_text(fd, buf);
}
static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static long long to_seconds(long long ms) {
    return (ms + 999) / 1000;
}
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
static int read_line_blocking(int fd, char *line, size_t size) {
    size_t len = 0;
    for (;;) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return -1;
        if (c == '\n') break;
        if (len + 1 < size) line[len++] = c;   
    }
    line[len] = '\0';
    return 0;
}

static bool has_inner_space(const char *s) {
    for (; *s != '\0'; s++)
        if (*s == ' ' || *s == '\t') return true;
    return false;
}
static bool is_letters_only(const char *s) {
    for (; *s != '\0'; s++)
        if (*s < 'a' || *s > 'z') return false;
    return true;
}
static int collect_player_chain(int fd, char *words[5]) {
    send_text(fd, "\nEnter your 5 connected words, one per line (one word each):\n");
    for (int i = 0; i < 5; i++) {
        send_fmt(fd, "Word %d: ", i + 1);
        char line[LINE_MAX_LEN];
        if (read_line_blocking(fd, line, sizeof line) != 0) {
            for (int j = 0; j < i; j++) free(words[j]); 
            return -1;
        }
        trim_lower(line);   

        if (strlen(line) == 0) {
            send_text(fd, "Word cannot be empty. Re-enter word.\n");
            i--;
            continue;
        }
        if (has_inner_space(line)) {
            send_text(fd, "Invalid word: please enter a single word with no spaces. Re-enter word.\n");
            i--;
            continue;
        }
        if (!is_letters_only(line)) {
            send_text(fd, "Invalid word: use letters only (a-z). Re-enter word.\n");
            i--;
            continue;
        }
        words[i] = strdup(line);
    }
    return 0;
}

static int load_chain(Session *s) {
    if (s->mode == MODE_CLASSIC && s->source == LIST_PLAYER) {
        return 0;   
    }

    const char *path = (s->mode == MODE_BLITZ) ? BLITZ_DATA_FILE : CLASSIC_DATA_FILE;

    wb_free(s->bank);
    s->bank = wb_load(path);
    if (s->bank == NULL) {
        fprintf(stderr, "Could not load word list from %s (run the server from the repo root).\n", path);
        return -1;
    }

    size_t count = wb_chain_count(s->bank);
    if (count == 0) {
        fprintf(stderr, "%s has no chains.\n", path);
        return -1;
    }

    if (s->mode == MODE_BLITZ) {
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

static int ask_source(Session *s) {
    send_text(s->fds[0], "\nChoose word list option:\n1. Server Game Master List\n2. Players generate own 5-word list\ninput: ");
    send_text(s->fds[1], "\nPlayer 0 is choosing the word list option...\n");

    char line[LINE_MAX_LEN];
    for (;;) {
        if (read_line_blocking(s->fds[0], line, sizeof line) != 0) {
            send_text(s->fds[1], "\nPlayer 0 left the game.\n");
            return -1;
        }
        trim_lower(line);

        if (strcmp(line, "1") == 0) { s->source = LIST_GAME_MASTER; return 0; }
        if (strcmp(line, "2") == 0) { s->source = LIST_PLAYER;      return 0; }

        send_text(s->fds[0], "Please type 1 or 2.\ninput: ");
    }
}

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

static char *render_ascii_view(const char *view) {
    int h = ascii_get_height();
    size_t cap = strlen(view) * (size_t)h * 26 + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t k = 0;

    const char *line = view;
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);

        bool current = (line[0] == '>');
        const char *word = line + 2;                 
        size_t wlen = (size_t)(end - word);

        for (int row = 0; row < h; row++) {
            const char *marker = (current && row == h / 2) ? "> " : "  ";
            memcpy(out + k, marker, 2);
            k += 2;

            for (size_t i = 0; i < wlen; i++) {
                const char *piece = ascii_get_line(word[i], row);
                size_t n = strlen(piece);
                memcpy(out + k, piece, n);
                k += n;
                out[k++] = ' ';                      
            }
            out[k++] = '\n';
        }

        line = (*end == '\n') ? end + 1 : end;
    }
    out[k] = '\0';
    return out;
}
static void send_views(Session *s, int turn_player, const bool out[2]) {
	for (int p = 0; p < 2; p++) {
	    send_text(s->fds[p], "\033[2J\033[H");

	    if (s->mode == MODE_BLITZ) {
	        long long ms_left = get_time_left(s->referee.self, p, now_ms());
	        send_fmt(s->fds[p], TIME_HEADER_FMT, to_seconds(ms_left));   /* row 1 */
	        send_text(s->fds[p], "\033[2;r\033[2;1H");   /* scroll region = row 2 down; cursor to row 2 */
	    }

	    send_text(s->fds[p], BANNER);

	    char *view = NULL;
	    if (view_game(&s->game, p, &view) == GAME_OK) {
	        char *art = render_ascii_view(view);
	        free(view);

	        if (s->mode != MODE_BLITZ) send_text(s->fds[p], "\nYour chain:\n");
	        else                       send_text(s->fds[p], "\n");

	        if (art) {
	            send_text(s->fds[p], art);
	            free(art);
	        }

	        send_fmt(s->fds[p], "\nScore: %d   Opponent: %d\n",
	                 get_score(&s->game, p), get_score(&s->game, 1 - p));
	    }

	    if (out && out[p]) send_text(s->fds[p], "\n(your time is up - waiting for the match to end)\n");
	    else               send_text(s->fds[p], turn_prompt(p, turn_player));
	}
}

static void tick_times(Session *s) {
    long long now = now_ms();
    for (int p = 0; p < 2; p++) {
        long long ms_left = get_time_left(s->referee.self, p, now);
        send_fmt(s->fds[p], "\0337\033[1;1H\033[2K" TIME_HEADER_FMT "\0338",
                 to_seconds(ms_left));
    }
}

static void announce(Session *s, Result r) {
    int other = 1 - r.player;

    switch (r.kind) {
    case RESULT_CORRECT:
        send_text(s->fds[r.player], "\nCorrect!\n");
        send_fmt(s->fds[other], "\nPlayer %d guessed correctly.\n", r.player);
        break;
    case RESULT_WRONG:
        send_text(s->fds[r.player], "\nWrong - a hint was added.\n");
        if (s->mode == MODE_CLASSIC) {
            send_fmt(s->fds[other], "\nPlayer %d guessed wrong. Turn passes to opponent.\n", r.player);
        } else {
            send_fmt(s->fds[other], "\nPlayer %d guessed wrong.\n", r.player);
        }
        break;
    case RESULT_PLAYER_OUT:
        send_text(s->fds[r.player], "\nYour time ran out. You are out.\n");
        send_fmt(s->fds[other], "\nPlayer %d ran out of time. You play on alone.\n", r.player);
        break;
    default:
        break;
    }
}

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

static bool handle_line(Session *s, int player, const char *line, bool out[2]) {
    Mode *m = &s->referee;
    Result r = m->on_guess(m->self, player, line, now_ms());

    if (r.kind == RESULT_IGNORED) {   /* not this player's turn */
        if (out && out[player]) send_text(s->fds[player], "\nYour time is up - you cannot play anymore.\n");
        else                    send_text(s->fds[player], "\nNot your turn - please wait.\n");
        return false;
    }

    if (r.kind == RESULT_GAME_OVER) {
        announce_game_over(s, "A player completed their word list!");
        return true;
    }

    if (r.kind == RESULT_PLAYER_OUT && out) out[r.player] = true;
    announce(s, r);
    send_views(s, r.next_player, out);
    return false;
}

static void play_classic(Session *s) {
    if (s->source == LIST_PLAYER) {
        char *words0[5] = {0};
        char *words1[5] = {0};

        send_text(s->fds[1], "\nPlayer 0 is entering their 5-word list...\n");
        if (collect_player_chain(s->fds[0], words0) != 0) return;

        send_text(s->fds[0], "\nPlayer 1 is entering their 5-word list...\n");
        if (collect_player_chain(s->fds[1], words1) != 0) {
            for (int i = 0; i < 5; i++) free(words0[i]);
            return;
        }
		GameStatus st = game_init(&s->game, (const char *const *)words1, 5, (const char *const *)words0, 5);
        for (int i = 0; i < 5; i++) {
            free(words0[i]);
            free(words1[i]);
        }

        if (st != GAME_OK) {
            fprintf(stderr, "Could not initialize game with custom chains.\n");
            return;
        }
    } else {
        if (game_init(&s->game, s->chain, s->chain_len, s->chain1, s->chain1_len) != GAME_OK) {
            fprintf(stderr, "Could not start the game.\n");
            return;
        }
    }

    if (!create_classic_mode(&s->referee, &s->game)) {
        fprintf(stderr, "Could not create Classic referee.\n");
        return;
    }

    int first = coin_flip(s);
    if (first < 0) return;   

    Mode *m = &s->referee;
    m->start(m->self, first, 0);
    send_views(s, first, NULL);

    char inbuf[2][LINE_MAX_LEN];      
    size_t inlen[2] = { 0, 0 };

    while (!m->is_over(m->self)) {
        struct pollfd pfd[2] = { { s->fds[0], POLLIN, 0 }, { s->fds[1], POLLIN, 0 } };
        int ready = poll(pfd, 2, -1);   
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll() failed");
            return;
        }

        for (int p = 0; p < 2; p++) {
            if (!(pfd[p].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            ssize_t n = recv(s->fds[p], inbuf[p] + inlen[p], LINE_MAX_LEN - 1 - inlen[p], 0);
            if (n <= 0) {   
                m->on_forfeit(m->self, p, 0);
                send_fmt(s->fds[1 - p], "\nPlayer %d left the game.\n", p);
                announce_game_over(s, "A player left the match.");
                return;
            }
            inlen[p] += (size_t)n;

          
            char *newline;
            while ((newline = memchr(inbuf[p], '\n', inlen[p])) != NULL) {
                *newline = '\0';
                size_t used = (size_t)(newline - inbuf[p]) + 1;

                if (handle_line(s, p, inbuf[p], NULL)) return;   /* match over */

                memmove(inbuf[p], inbuf[p] + used, inlen[p] - used);
                inlen[p] -= used;
            }

            if (inlen[p] >= LINE_MAX_LEN - 1) inlen[p] = 0;   /* a line this long is junk: drop it */
        }
    }
}

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
    long long next_tick = now_ms() + 1000;   /* when the time header is next redrawn */

    char inbuf[2][LINE_MAX_LEN];      /* partly received lines, one per player */
    size_t inlen[2] = { 0, 0 };

    while (!m->is_over(m->self)) {
        long long now = now_ms();

        /* Once per second, redraw the time header so the clock visibly counts down. */
        if (now >= next_tick) {
            tick_times(s);
            while (next_tick <= now) next_tick += 1000;
        }

        /* Sleep until the earlier of: the clock deadline, the next tick. */
        long long wait = m->ms_until_deadline(m->self, now);
        long long to_tick = next_tick - now;
        if (wait < 0 || wait > to_tick) wait = to_tick;
        int timeout_ms = (wait > INT_MAX) ? INT_MAX : (int)wait;

        struct pollfd pfd[2] = { { s->fds[0], POLLIN, 0 }, { s->fds[1], POLLIN, 0 } };
        int ready = poll(pfd, 2, timeout_ms);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll() failed");
            return;
        }

        long long current_time = now_ms();

        /* Nobody sent anything: either the deadline arrived (the active clock ran out)
           or it was only the one-second tick. */
        if (ready == 0) {
            Result r = m->on_timeout(m->self, current_time);
            if (r.kind == RESULT_IGNORED) continue;   /* only the one-second tick woke us: redrawn at the top of the loop */
            if (r.kind == RESULT_GAME_OVER) {
                announce_game_over(s, "Both players ran out of time.");
                return;
            }
            if (r.kind == RESULT_PLAYER_OUT) out[r.player] = true;
            announce(s, r);
            send_views(s, blitz_active_player(m->self), out);
            continue;
        }

        /* Player sent input or disconnected */
        for (int p = 0; p < 2; p++) {
            if (!(pfd[p].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            ssize_t n = recv(s->fds[p], inbuf[p] + inlen[p], LINE_MAX_LEN - 1 - inlen[p], 0);
            if (n <= 0) {   /* the player disconnected: the other player wins */
                m->on_forfeit(m->self, p, current_time);
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

/* ======== 9. RUNNING ======== */

void run_session(Session *s) {
    if (s == NULL) return;

    /* Setup: which mode? */
    if (ask_mode(s) != 0) return;

    /* Setup: which word list source? (Classic mode only) */
    if (s->mode == MODE_CLASSIC) {
        if (ask_source(s) != 0) return;
    }

    /* Pick the word chain for this match. */
    if (load_chain(s) != 0) {
        send_text(s->fds[0], "\nThe server could not load the word list.\n");
        send_text(s->fds[1], "\nThe server could not load the word list.\n");
        return;
    }

    if (s->mode == MODE_BLITZ) {
        play_blitz(s);
    } else {
        play_classic(s);
    }
}
