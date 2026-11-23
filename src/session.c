// for clock_gettime, strdup
// 200809L means "the 2008 POSIX standard", the version that includes both of those functions
#define _POSIX_C_SOURCE 200809L

#include "session.h"
#include "word_bank.h"
#include "game.h"
#include "blitz_mode.h"
#include "classic_mode.h"
#include "ascii_art.h"
#include "ascii_art_announce.h"
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

// ======== 1. SETTINGS AND PRIVATE DATA ========

// where the word chains are loaded from (run the server from the repo root)
#define CLASSIC_DATA_FILE "data/classicmode.json"
#define BLITZ_DATA_FILE   "data/blitzmode.json"

// the longest line a player can type; anything longer is cut off
#define LINE_MAX_LEN 256

// the countdown line shown on row 1 of the screen in Blitz mode
#define TIME_HEADER_FMT "⏳ Time Left: %llds | Your chain:"
// the big "CHAINLINK" title drawn at the top of every screen
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

// everything the session remembers about the current match (hidden from every other file)
struct Session {
    int fds[2];                   // the two players' sockets (the session never closes them)
    GameMode mode;                // Classic or Blitz
    ListSource source;            // who provides the word list (Classic only)

    WordBank *bank;               // the word chains loaded from the data file
    const char *const *chain;     // player 0's chain (borrowed from bank)
    size_t chain_len;             // how many words are in player 0's chain
    const char *const *chain1;    // player 1's chain (borrowed from bank)
    size_t chain1_len;            // how many words are in player 1's chain

    Game game;                    // the match state (the words, positions, hints, scores)
    Mode referee;                 // the referee: the rules of the chosen mode

    int  chooser;                 // player who picks the mode / list source: player 0 first, then the last winner
    bool can_continue;            // the round ended normally (nobody left), so a rematch can be offered
};

// ======== 2. CREATE / RESET / DESTROY ========


// create a session for two already-connected players; every field starts at zero
Session *create_session(int fd0, int fd1) {
  
    Session *s = calloc(1, sizeof *s);

    // if there is no memory, hand the NULL back to the caller
    if (s == NULL) return s;  
    // remember both players' sockets
    s->fds[0] = fd0;
    s->fds[1] = fd1;

    return s;  
}

// throw away everything one round created so the next round starts clean
// safe on a round that never got as far as creating a referee or a game
static void reset_round(Session *s) {
    // free the referee that belongs to the mode that was played
    if (s->mode == MODE_BLITZ) {
        destroy_blitz_mode(&s->referee);  
    } else {
        destroy_classic_mode(&s->referee);
    }
    
    // blank the referee so it cannot be used by mistake
    memset(&s->referee, 0, sizeof s->referee);
    
    // free the players' chains inside the game, and the loaded word bank
    free_game(&s->game);              
    wb_free(s->bank);
    
    // forget the pointers, because the bank they pointed into is gone
    s->bank = NULL;
    s->chain = s->chain1 = NULL;
    s->chain_len = s->chain1_len = 0;
    
    // next round asks for the word list option again
    s->source = LIST_GAME_MASTER;
}

// release the session when the whole thing is over
void destroy_session(Session *s) {
    if (s == NULL) return;

    reset_round(s);
    free(s);
}

// ======== 3. SMALL HELPERS ========

// send a whole piece of text to a player; send() may only send part of it, so keep going until all is sent
static void send_text(int fd, const char *text) {
    size_t left = strlen(text);
    while (left > 0) {
        ssize_t n = send(fd, text, left, 0);
        
        // the player is gone, stop trying
        if (n <= 0) return;
        
        // move past what was sent
        text += n;
        left -= (size_t)n;
    }
}

// like printf, but the text goes to a player's socket (e.g. send_fmt(fd, "Score: %d", 3))
static void send_fmt(int fd, const char *format, ...) {
    char buf[512];
    
    // fill buf with the formatted text, then send it
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof buf, format, args);
    va_end(args);
    send_text(fd, buf);
}

// the server's stopwatch in milliseconds; "monotonic" means it only moves forward, even if the computer's clock is changed
static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// milliseconds to whole seconds, rounded up, so the countdown shows 1 (not 0) until time is truly up
static long long to_seconds(long long ms) {
    return (ms + 999) / 1000;
}

// clean what a player typed: remove spaces and the end-of-line from both sides, and make it lowercase
static void trim_lower(char *s) {
    size_t len = strlen(s);
    // cut the spaces, tabs and end-of-line characters off the end
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';

    // skip the spaces and tabs at the start, then slide the text to the front
    size_t start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;
    memmove(s, s + start, len - start + 1);

    // turn capital letters into small ones
    for (size_t i = 0; s[i] != '\0'; i++)
        if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] - 'A' + 'a');
}

// wait until a player presses Enter and give back the line they typed; -1 if they disconnected
// "blocking" means the whole server waits here, so it is only used while setting up, never during a match
static int read_line_blocking(int fd, char *line, size_t size) {
    size_t len = 0;
    for (;;) {
        // read one character at a time
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        
        // nothing came in: the player left
        if (n <= 0) return -1;
        
        // Enter was pressed, the line is complete
        if (c == '\n') break;
        
        // keep the character, but never write past the end of the buffer (extra characters are dropped)
        if (len + 1 < size) line[len++] = c;   
    }
    line[len] = '\0';
    return 0;
}

// ======== 4. SETUP ========

// checks if a word has a space or tab inside it
static bool has_inner_space(const char *s) {
    for (; *s != '\0'; s++)
        if (*s == ' ' || *s == '\t') return true;
    return false;
}

// checks if a word has only the letters a to z
static bool is_letters_only(const char *s) {
    for (; *s != '\0'; s++)
        if (*s < 'a' || *s > 'z') return false;
    return true;
}

// ask one player for their own 5-word list (Classic, "players generate own list" option)
// returns 0 when all 5 words are valid, -1 if the player disconnected
static int collect_player_chain(int fd, char *words[5]) {
    send_text(fd, "\nEnter your 5 connected words, one per line (one word each):\n");
    for (int i = 0; i < 5; i++) {
        send_fmt(fd, "Word %d: ", i + 1);
        char line[LINE_MAX_LEN];
        // the player left: free the words already collected and give up
        if (read_line_blocking(fd, line, sizeof line) != 0) {
            for (int j = 0; j < i; j++) free(words[j]); 
            return -1;
        }
        trim_lower(line);   

        // each check below rejects the word and asks again for the same slot (i--)
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
        // the word is good: keep its own copy
        words[i] = strdup(line);
    }
    return 0;
}

// pick the word chains for this match from the data file; 0 on success, -1 on failure
static int load_chain(Session *s) {
    // players write their own lists in that option, so there is nothing to load
    if (s->mode == MODE_CLASSIC && s->source == LIST_PLAYER) {
        return 0;   
    }

    // each mode has its own data file
    const char *path = (s->mode == MODE_BLITZ) ? BLITZ_DATA_FILE : CLASSIC_DATA_FILE;

    // load the file (dropping any bank left from an earlier round)
    wb_free(s->bank);
    s->bank = wb_load(path);
    if (s->bank == NULL) {
        fprintf(stderr, "Could not load word list from %s (run the server from the repo root).\n", path);
        return -1;
    }

    // a file with no chains is useless
    size_t count = wb_chain_count(s->bank);
    if (count == 0) {
        fprintf(stderr, "%s has no chains.\n", path);
        return -1;
    }

    if (s->mode == MODE_BLITZ) {
        // Blitz: each player gets a different random chain (i1 is never the same as i0 when there are two or more)
        size_t i0 = (size_t)rand() % count;
        size_t i1 = (count >= 2) ? (i0 + 1 + (size_t)rand() % (count - 1)) % count : i0;
        s->chain  = wb_get_chain(s->bank, i0, &s->chain_len);
        s->chain1 = wb_get_chain(s->bank, i1, &s->chain1_len);
    } else {
        // Classic: both players get the same random chain
        s->chain = wb_get_random_chain(s->bank, &s->chain_len);
        s->chain1 = s->chain;
        s->chain1_len = s->chain_len;
    }

    return 0;
}

// ask the chooser which mode to play; the other player waits. 0 on success, -1 if the chooser left
static int ask_mode(Session *s) {
    // c is the chooser: player 0 the first time, then the previous winner
    int c = s->chooser;
    send_text(s->fds[c], "\nChoose a mode: type 1 for Classic or 2 for Blitz\ninput: ");
    send_fmt(s->fds[1 - c], "\nPlayer %d is choosing the game mode...\n", c);

    // keep asking until the answer is valid
    char line[LINE_MAX_LEN];
    for (;;) {
        if (read_line_blocking(s->fds[c], line, sizeof line) != 0) {
            send_fmt(s->fds[1 - c], "\nPlayer %d left the game.\n", c);
            return -1;
        }
        trim_lower(line);

        if (strcmp(line, "1") == 0 || strcmp(line, "classic") == 0) { s->mode = MODE_CLASSIC; return 0; }
        if (strcmp(line, "2") == 0 || strcmp(line, "blitz") == 0)   { s->mode = MODE_BLITZ;   return 0; }

        send_text(s->fds[c], "Please type 1 or 2.\ninput: ");
    }
}

// ask the chooser who provides the word list (Classic only); 0 on success, -1 if the chooser left
static int ask_source(Session *s) {
    int c = s->chooser;
    send_text(s->fds[c], "\nChoose word list option:\n1. Server Game Master List\n2. Players generate own 5-word list\ninput: ");
    send_fmt(s->fds[1 - c], "\nPlayer %d is choosing the word list option...\n", c);

    // keep asking until the answer is valid
    char line[LINE_MAX_LEN];
    for (;;) {
        if (read_line_blocking(s->fds[c], line, sizeof line) != 0) {
            send_fmt(s->fds[1 - c], "\nPlayer %d left the game.\n", c);
            return -1;
        }
        trim_lower(line);

        if (strcmp(line, "1") == 0) { s->source = LIST_GAME_MASTER; return 0; }
        if (strcmp(line, "2") == 0) { s->source = LIST_PLAYER;      return 0; }

        send_text(s->fds[c], "Please type 1 or 2.\ninput: ");
    }
}

// the coin flip that decides who plays first; returns that player (0 or 1), or -1 if the caller left
static int coin_flip(Session *s) {
    // a random player is picked to call heads or tails
    int caller = pick_caller();
    int other = 1 - caller;

    send_text(s->fds[caller], "\nYou call the coin. Type heads or tails\ninput: ");
    send_fmt(s->fds[other], "\nPlayer %d is calling the coin...\n", caller);

    // keep asking until the caller types heads or tails (h and t also work)
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

        send_text(s->fds[caller], "Please type heads (or h) or tails (or t).\ninput: ");
    }

    // the game master flips the coin once; if the call matches, the caller goes first, otherwise the other player does
    CoinSide flip = flip_coin();
    int first = decide_first_player(caller, call, flip);

    // tell both players what happened
    for (int p = 0; p < 2; p++)
        send_fmt(s->fds[p], "\nPlayer %d called %s. The coin landed on %s. Player %d goes first.\n",
                 caller,
                 call == COIN_HEADS ? "heads" : "tails",
                 flip == COIN_HEADS ? "heads" : "tails",
                 first);
    return first;
}

// ======== 5. DRAWING ========

/*
The big arrow that marks the word the player is guessing now.
It is drawn in the same block style as the letters and is as tall as a
letter (ascii_get_height() rows), pointing right, in bold yellow:
    ██
    ████
    ██████
    ████
    ██
Words that are not being guessed get blank space of the same width, so every
word stays lined up.
*/
#define ARROW_COLOR "\033[1;93m"      // terminal code for bold yellow
#define COLOR_RESET "\033[0m"         // terminal code to go back to normal colors
#define BLOCK       "█"               // one filled block (takes 3 bytes in UTF-8)
#define BLOCK_BYTES 3
#define ARROW_MAX_COLS 16             // widest arrow we ever draw (a font up to 15 rows tall)
#define MARKER_COLS(h) (2 * (((h) - 1) / 2 + 1) + 1)   // arrow width + one gap column

// the most bytes one marker can take, used to size the output buffer
#define MARKER_MAX_BYTES (sizeof ARROW_COLOR + sizeof COLOR_RESET + ARROW_MAX_COLS * BLOCK_BYTES + 2)

// write the marker for one row of a word: a slice of the arrow if it is the current word, blank space if not
// returns how many bytes were written
static size_t write_marker(char *out, int row, int h, bool current) {
    // mid is the middle row, where the arrow is widest
    int mid = (h - 1) / 2;
    int max_width = 2 * (mid + 1);
    size_t k = 0;

    // not the current word: write spaces as wide as the arrow so every word stays lined up
    if (!current) {
        for (int i = 0; i < max_width + 1; i++) out[k++] = ' ';
        return k;
    }

    // the further a row is from the middle, the narrower the arrow gets
    int dist = row > mid ? row - mid : mid - row;
    int width = 2 * (mid + 1 - dist);                // 2, 4, 6, 4, 2 blocks for a 5-row font

    // color on, then the blocks, then color off
    memcpy(out + k, ARROW_COLOR, sizeof ARROW_COLOR - 1);
    k += sizeof ARROW_COLOR - 1;
    for (int i = 0; i < width; i++) {
        memcpy(out + k, BLOCK, BLOCK_BYTES);
        k += BLOCK_BYTES;
    }
    memcpy(out + k, COLOR_RESET, sizeof COLOR_RESET - 1);
    k += sizeof COLOR_RESET - 1;
    // pad with spaces up to the full arrow width + the gap
    for (int i = width; i < max_width + 1; i++) out[k++] = ' ';
    return k;
}

// turn the plain-text view from view_game (e.g. "> w___") into big block letters, with the arrow beside the current word
// returns a new buffer that the caller must free, or NULL on failure
static char *render_ascii_view(const char *view) {
    // h is how many rows tall each letter is
    int h = ascii_get_height();
    if (h < 1 || h > 15) return NULL;

    // count the lines, so the buffer can be sized big enough
    size_t lines = 1;
    for (const char *c = view; *c != '\0'; c++)
        if (*c == '\n') lines++;

    // per row: the marker, up to 26 bytes per letter, and the newline
    size_t cap = (size_t)h * (lines * (MARKER_MAX_BYTES + 1) + strlen(view) * 26) + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t k = 0;

    // go through the view one word (one line) at a time
    const char *line = view;
    while (*line != '\0') {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);

        // a line starting with '>' is the word being guessed; the word itself starts after the 2-character marker
        bool current = (line[0] == '>');
        const char *word = line + 2;
        size_t wlen = end > word ? (size_t)(end - word) : 0;

        // each word is drawn row by row: the marker first, then every letter's slice of that row
        for (int row = 0; row < h; row++) {
            k += write_marker(out + k, row, h, current);

            for (size_t i = 0; i < wlen; i++) {
                const char *piece = ascii_get_line(word[i], row);
                size_t n = strlen(piece);
                memcpy(out + k, piece, n);
                k += n;
                out[k++] = ' ';
            }
            out[k++] = '\n';
        }

        // move on to the next line
        line = (*end == '\n') ? end + 1 : end;
    }
    out[k] = '\0';
    return out;
}

// redraw both players' whole screens: title, their own chain, scores, and whose turn it is
// out[p] is true for a player whose Blitz clock has run out (NULL in Classic, which has no clocks)
static void send_views(Session *s, int turn_player, const bool out[2]) {
	for (int p = 0; p < 2; p++) {
	    // clear the screen and put the cursor at the top-left
	    send_text(s->fds[p], "\033[2J\033[H");

	    if (s->mode == MODE_BLITZ) {
	        // row 1 holds the countdown
	        long long ms_left = get_time_left(s->referee.self, p, now_ms());
	        send_fmt(s->fds[p], TIME_HEADER_FMT, to_seconds(ms_left));
	        // scroll region = row 2 downward, so row 1 stays fixed; cursor to row 2
	        send_text(s->fds[p], "\033[2;r\033[2;1H");
	    }

	    send_text(s->fds[p], BANNER);

	    // each player sees only their own chain, with later words hidden by view_game
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

	        // their own score first, then the opponent's
	        send_fmt(s->fds[p], "\nScore: %d   Opponent: %d\n",
	                 get_score(&s->game, p), get_score(&s->game, 1 - p));
	    }

	    // a player whose clock ran out is told to wait; otherwise show "YOUR TURN" or "waiting"
	    if (out && out[p]) send_text(s->fds[p], "\n(your time is up - waiting for the match to end)\n");
	    else               send_text(s->fds[p], turn_prompt(p, turn_player));
	}
}

// redraw only the countdown on row 1 (called once a second, so the clock visibly counts down)
static void tick_times(Session *s) {
    long long now = now_ms();
    for (int p = 0; p < 2; p++) {
        long long ms_left = get_time_left(s->referee.self, p, now);
        // save the cursor, jump to row 1, clear that row, write the time, then put the cursor back
        // (so whatever the player is typing is not disturbed)
        send_fmt(s->fds[p], "\0337\033[1;1H\033[2K" TIME_HEADER_FMT "\0338",
                 to_seconds(ms_left));
    }
}

// ======== 6. ANNOUNCEMENTS AND HANDLING A GUESS ========

// tell both players what the last result was (correct, wrong, or out of time)
static void announce(Session *s, Result r) {
    // r.player is the one the result is about; other is their opponent
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

// show the game over screen: why it ended, both scores, and win / lose / tie for each player
static void announce_game_over(Session *s, const char *reason) {
    // ask the referee who won (-1 means a tie)
    int winner = s->referee.winner(s->referee.self);

    for (int p = 0; p < 2; p++) {
        send_text(s->fds[p], "\n");
        send_text(s->fds[p], ascii_art_announce_game_over());
        send_text(s->fds[p], "\n");

send_fmt(s->fds[p], "%s\nScores: player 0 = %d, player 1 = %d\n",
         reason, get_score(&s->game, 0), get_score(&s->game, 1));
        // each player gets their own ending: tie, win, or lose
        if (winner == -1)
            send_text(s->fds[p], ascii_art_announce_it_is_a_tie());
        else if (winner == p)
            send_text(s->fds[p], ascii_art_announce_you_win());
        else
            send_text(s->fds[p], ascii_art_announce_you_lose());
    }
}

// handle one full line a player typed: give it to the referee as a guess and show the outcome
// returns true when that guess ended the match
static bool handle_line(Session *s, int player, const char *line, bool out[2]) {
    Mode *m = &s->referee;
    // the referee decides what the guess means; the session only passes it along with the current time
    Result r = m->on_guess(m->self, player, line, now_ms());

    // not this player's turn (or the match is not running): tell them and change nothing
    if (r.kind == RESULT_IGNORED) {
        if (out && out[player]) send_text(s->fds[player], "\nYour time is up - you cannot play anymore.\n");
        else                    send_text(s->fds[player], "\nNot your turn - please wait.\n");
        return false;
    }

    // the match ended
    if (r.kind == RESULT_GAME_OVER) {
        // there are two causes: someone finished their list, or (Blitz) a
        // late guess landed after the last running clock had already expired
        // (that is why the message is chosen by checking who finished)
        if (is_game_finished(&s->game, r.player))
            announce_game_over(s, "A player completed their word list!");
        else
            announce_game_over(s, "Both players ran out of time.");
        // the match ended normally, so a rematch can be offered
        s->can_continue = true;
        return true;
    }

    // the match goes on: remember anyone whose clock ran out, tell both players, and redraw the screens
    if (r.kind == RESULT_PLAYER_OUT && out) out[r.player] = true;
    announce(s, r);
    send_views(s, r.next_player, out);
    return false;
}
static void drain_input(int fd) {
    char buf[256];
    while (recv(fd, buf, sizeof buf, MSG_DONTWAIT) > 0) { }
}
static int ask_rematch(Session *s, int winner) {
    // c is the player who decides: the winner, or a random player on a tie
    int c = (winner == 0 || winner == 1) ? winner : pick_caller();
    s->chooser = c;

    // give the screen back to normal scrolling (Blitz had fixed the top row)
    for (int p = 0; p < 2; p++) send_text(s->fds[p], "\033[r");

    if (winner == -1) {
        send_fmt(s->fds[c], "\nIt was a tie, so you were picked at random to decide.\n");
        send_fmt(s->fds[1 - c], "\nIt was a tie, so Player %d was picked at random to decide.\n", c);
    }
    send_text(s->fds[c], "\nPlay another game?\n1. Yes, choose a mode\n2. Exit\ninput: ");
    send_fmt(s->fds[1 - c], "\nWaiting for Player %d to choose: play again or exit...\n", c);
    // ignore keystrokes typed during the final screen
    drain_input(s->fds[c]);

    // keep asking until the answer is valid
    char line[LINE_MAX_LEN];
    for (;;) {
        if (read_line_blocking(s->fds[c], line, sizeof line) != 0) {
            send_fmt(s->fds[1 - c], "\nPlayer %d left the game.\n", c);
            return -1;
        }
        trim_lower(line);

        if (strcmp(line, "1") == 0 || strcmp(line, "y") == 0 || strcmp(line, "yes") == 0) return 1;
        if (strcmp(line, "2") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "n") == 0 || strcmp(line, "no") == 0) return 0;

        send_text(s->fds[c], "Please type 1 or 2.\ninput: ");
    }
}
// returns true to start another round, or false to end the session
static bool finish_or_rematch(Session *s) {
    // someone left or setup failed: nobody is left to ask
    if (!s->can_continue) return false;

    // ask the winner (found through the referee) whether to play again or exit
    int again = ask_rematch(s, s->referee.winner(s->referee.self));
    if (again < 0) return false;

    // the winner chose to exit: say goodbye to both and stop
    if (again == 0) {
        for (int p = 0; p < 2; p++) {
            send_text(s->fds[p], "\nThanks for playing ChainLink! Goodbye.\n");
        }
        return false;
    }

    // new round: wipe the old game and clear both screens
    reset_round(s);
    for (int p = 0; p < 2; p++) {
        drain_input(s->fds[p]);
        send_text(s->fds[p], "\033[r\033[2J\033[H");
    }
    return true;
}
// ======== 7. PLAYING A MATCH ========

// run a whole Classic match: untimed, a wrong guess passes the turn to the opponent
static void play_classic(Session *s) {
    // step 1: build the game, from the players' own lists or from the server's chain
    if (s->source == LIST_PLAYER) {
        char *words0[5] = {0};
        char *words1[5] = {0};

        // each player enters a list while the other waits
        send_text(s->fds[1], "\nPlayer 0 is entering their 5-word list...\n");
        if (collect_player_chain(s->fds[0], words0) != 0) return;

        send_text(s->fds[0], "\nPlayer 1 is entering their 5-word list...\n");
        if (collect_player_chain(s->fds[1], words1) != 0) {
            for (int i = 0; i < 5; i++) free(words0[i]);
            return;
        }
        // the lists are swapped, so each player guesses the OTHER player's words
		GameStatus st = game_init(&s->game, (const char *const *)words1, 5, (const char *const *)words0, 5);
        // game_init made its own copies, so the typed words can be freed
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

    // step 2: create the Classic referee
    if (!create_classic_mode(&s->referee, &s->game)) {
        fprintf(stderr, "Could not create Classic referee.\n");
        return;
    }

    // step 3: the coin flip decides who starts (-1 means someone left)
    int first = coin_flip(s);
    if (first < 0) return;   

    // step 4: start the referee and show everyone the first screen (Classic has no clocks, so no "out" list)
    Mode *m = &s->referee;
    m->start(m->self, first, 0);
    send_views(s, first, NULL);

    // partly typed lines, one buffer per player (text arrives in pieces, not always a whole line at once)
    char inbuf[2][LINE_MAX_LEN];      
    size_t inlen[2] = { 0, 0 };

    // step 5: the match loop, until the referee says it is over
    while (!m->is_over(m->self)) {
        // poll() sleeps until one of the two players sends something; there is no clock, so it waits as long as needed (-1)
        struct pollfd pfd[2] = { { s->fds[0], POLLIN, 0 }, { s->fds[1], POLLIN, 0 } };
        int ready = poll(pfd, 2, -1);   
        if (ready < 0) {
            // an interruption is harmless, just try again
            if (errno == EINTR) continue;
            perror("poll() failed");
            return;
        }

        for (int p = 0; p < 2; p++) {
            // skip the player who did nothing
            if (!(pfd[p].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            // read whatever the player sent, after what is already in their buffer
            ssize_t n = recv(s->fds[p], inbuf[p] + inlen[p], LINE_MAX_LEN - 1 - inlen[p], 0);
            // nothing came in: the player disconnected, so the other player wins
            if (n <= 0) {   
                m->on_forfeit(m->self, p, 0);
                send_fmt(s->fds[1 - p], "\nPlayer %d left the game.\n", p);
                announce_game_over(s, "A player left the match.");
                return;
            }
            inlen[p] += (size_t)n;

            // take every complete line (ending in Enter) out of the buffer and treat it as a guess
            char *newline;
            while ((newline = memchr(inbuf[p], '\n', inlen[p])) != NULL) {
                *newline = '\0';
                size_t used = (size_t)(newline - inbuf[p]) + 1;

                // true means that guess ended the match
                if (handle_line(s, p, inbuf[p], NULL)) return;

                // remove the handled line, keep the rest
                memmove(inbuf[p], inbuf[p] + used, inlen[p] - used);
                inlen[p] -= used;
            }

            // a line this long is junk: drop it
            if (inlen[p] >= LINE_MAX_LEN - 1) inlen[p] = 0;
        }
    }
    finish_or_rematch(s);
}

// run a whole Blitz match: every player has a clock, and the clock only runs on their own turn
static void play_blitz(Session *s) {
    // step 1: build the game; each player gets their own chain (game_init copies the words)
    if (game_init(&s->game, s->chain, s->chain_len, s->chain1, s->chain1_len) != GAME_OK) {
        fprintf(stderr, "Could not start the game.\n");
        return;
    }
    // step 2: create the Blitz referee
    if (!create_blitz_mode(&s->referee, &s->game)) {
        fprintf(stderr, "Could not create the Blitz referee.\n");
        return;
    }

    // step 3: the coin flip decides who starts
    int first = coin_flip(s);
    if (first < 0) return;   // someone left during the coin flip

    // step 4: start the referee (its clock starts now) and show everyone the first screen
    Mode *m = &s->referee;
    bool out[2] = { false, false };   // players whose clock ran out

    m->start(m->self, first, now_ms());
    send_views(s, first, out);
    long long next_tick = now_ms() + 1000;   // when the time header is next redrawn

    // partly typed lines, one buffer per player
    char inbuf[2][LINE_MAX_LEN];
    size_t inlen[2] = { 0, 0 };

    // step 5: the match loop, until the referee says it is over
    while (!m->is_over(m->self)) {
        long long now = now_ms();

        // once per second, redraw the time header so the clock visibly counts down
        if (now >= next_tick) {
            tick_times(s);
            while (next_tick <= now) next_tick += 1000;
        }

        // decide how long poll() may sleep: until the earlier of the clock deadline and the next tick
        long long wait = m->ms_until_deadline(m->self, now);
        long long to_tick = next_tick - now;
        if (wait < 0 || wait > to_tick) wait = to_tick;
        int timeout_ms = (wait > INT_MAX) ? INT_MAX : (int)wait;

        struct pollfd pfd[2] = { { s->fds[0], POLLIN, 0 }, { s->fds[1], POLLIN, 0 } };
        // poll() sleeps until a player sends something or the timeout passes
        int ready = poll(pfd, 2, timeout_ms);
        if (ready < 0) {
            // an interruption is harmless, just try again
            if (errno == EINTR) continue;
            perror("poll() failed");
            return;
        }

        long long current_time = now_ms();

        // nobody sent anything: either the deadline arrived (the active clock ran out) or it was only the one-second tick
        if (ready == 0) {
            // ask the referee if the clock really ran out
            Result r = m->on_timeout(m->self, current_time);
            // only the tick woke us: the header is redrawn at the top of the loop
            if (r.kind == RESULT_IGNORED) continue;
            // both clocks are out: the match is over
            if (r.kind == RESULT_GAME_OVER) {
                announce_game_over(s, "Both players ran out of time.");
                s->can_continue = true;
                return;
            }
            // one player ran out: tell both and show the screens (the other player plays on alone)
            if (r.kind == RESULT_PLAYER_OUT) out[r.player] = true;
            announce(s, r);
            send_views(s, blitz_active_player(m->self), out);
            continue;
        }

        // a player sent something or disconnected
        for (int p = 0; p < 2; p++) {
            // skip the player who did nothing
            if (!(pfd[p].revents & (POLLIN | POLLHUP | POLLERR))) continue;

            // read whatever the player sent, after what is already in their buffer
            ssize_t n = recv(s->fds[p], inbuf[p] + inlen[p], LINE_MAX_LEN - 1 - inlen[p], 0);
            // nothing came in: the player disconnected, so the other player wins
            if (n <= 0) {
                m->on_forfeit(m->self, p, current_time);
                send_fmt(s->fds[1 - p], "\nPlayer %d left the game.\n", p);
                announce_game_over(s, "A player left the match.");
                return;
            }
            inlen[p] += (size_t)n;

            // take every complete line (ending in Enter) out of the buffer and treat it as a guess
            char *newline;
            while ((newline = memchr(inbuf[p], '\n', inlen[p])) != NULL) {
                *newline = '\0';
                size_t used = (size_t)(newline - inbuf[p]) + 1;

                // true means that guess ended the match
                if (handle_line(s, p, inbuf[p], out)) return;

                // remove the handled line, keep the rest
                memmove(inbuf[p], inbuf[p] + used, inlen[p] - used);
                inlen[p] -= used;
            }

            // a line this long is junk: drop it
            if (inlen[p] >= LINE_MAX_LEN - 1) inlen[p] = 0;
        }
    }
    finish_or_rematch(s);
}

// ======== 8. REMATCH AND RUNNING ========

// run the whole session: setup, a match, then the rematch question, again and again until someone exits
void run_session(Session *s) {
    if (s == NULL) return;

    // one trip around this loop is one match
    for (;;) {
        s->can_continue = false;

        // setup: which mode? (the previous winner chooses; player 0 the first time)
        if (ask_mode(s) != 0) return;

        // setup: which word list source? (Classic mode only)
        if (s->mode == MODE_CLASSIC) {
            if (ask_source(s) != 0) return;
        }

        // pick the word chain for this match
        if (load_chain(s) != 0) {
            send_text(s->fds[0], "\nThe server could not load the word list.\n");
            send_text(s->fds[1], "\nThe server could not load the word list.\n");
            return;
        }

        // play the match in the chosen mode
        if (s->mode == MODE_BLITZ) {
            play_blitz(s);
        } else {
            play_classic(s);
        }

        // someone left or setup failed: nobody is left to ask
        if (!s->can_continue) return;
    }
}
