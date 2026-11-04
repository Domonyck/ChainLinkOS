#define _POSIX_C_SOURCE 199309L
#include "session.h"
#include "game.h"
#include "word_bank.h"
#include "blitz_mode.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define CLASSIC_DATA_FILE "data/classicmode.json"
#define BLITZ_DATA_FILE   "data/blitzmode.json"

struct Session {
    int fds[2];
    GameMode mode;
    ListSource source;
    WordBank *bank;
    Game game;
};

Session *create_session(int fd0, int fd1) {
    Session *s = calloc(1, sizeof *s);
    if (s == NULL) return NULL;
    s->fds[0] = fd0;
    s->fds[1] = fd1;
    return s;
}

void destroy_session(Session *s) {
    if (s == NULL) return;
    free_game(&s->game);
    wb_free(s->bank);
    free(s);
}

// Helper to get current monotonic time in milliseconds
static long long get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Load chains based on chosen mode
static int load_chains_for_players(Session *s) {
    const char *path = (s->mode == MODE_BLITZ) ? BLITZ_DATA_FILE : CLASSIC_DATA_FILE;
    
    s->bank = wb_load(path);
    if (s->bank == NULL) {
        fprintf(stderr, "Could not load word list from %s\n", path);
        return -1;
    }

    size_t n0 = 0, n1 = 0;
    const char *const *chain0 = wb_get_random_chain(s->bank, &n0);
    const char *const *chain1 = wb_get_random_chain(s->bank, &n1);

    if (!chain0 || !chain1 || n0 == 0 || n1 == 0) {
        fprintf(stderr, "Failed to retrieve random chains from %s\n", path);
        return -1;
    }

    if (game_init(&s->game, chain0, n0, chain1, n1) != GAME_OK) {
        fprintf(stderr, "Failed to initialize game state.\n");
        return -1;
    }

    return 0;
}

// Run Classic Mode match loop
static void run_classic_match(Session *s, int first_player) {
    int active_player = first_player;
    char guess_buf[256];
    char info_buf[512];

    while (true) {
        // Send views and turn prompts
        for (int i = 0; i < 2; i++) {
            char *view = NULL;
            if (view_game(&s->game, i, &view) == GAME_OK) {
                char header[1024];
                snprintf(header, sizeof(header), "\n=== CLASSIC MODE ===\n%s\n", view);
                send(s->fds[i], header, strlen(header), 0);
                
                if (i == active_player) {
                    char *turn_msg = "Your turn! Enter your word guess: ";
                    send(s->fds[i], turn_msg, strlen(turn_msg), 0);
                } else {
                    char *wait_msg = "Waiting for opponent's turn...\n";
                    send(s->fds[i], wait_msg, strlen(wait_msg), 0);
                }
                free(view);
            }
        }

        memset(guess_buf, 0, sizeof(guess_buf));
        int n = recv(s->fds[active_player], guess_buf, sizeof(guess_buf) - 1, 0);
        if (n <= 0) break; // Disconnect

        guess_buf[strcspn(guess_buf, "\r\n")] = 0;

        if (check_guess(&s->game, active_player, guess_buf)) {
            advance(&s->game, active_player);
            
            if (is_game_finished(&s->game, active_player)) {
                snprintf(info_buf, sizeof(info_buf), "\nPlayer %d wins Classic Mode by finishing the list first!\n", active_player);
                send(s->fds[0], info_buf, strlen(info_buf), 0);
                send(s->fds[1], info_buf, strlen(info_buf), 0);
                break;
            }
        } else {
            add_hint(&s->game, active_player);
            char *wrong_msg = "Incorrect guess! Hint added. Turn switching...\n";
            send(s->fds[active_player], wrong_msg, strlen(wrong_msg), 0);
            active_player = 1 - active_player;
        }
    }
}

// Run Blitz Mode match loop using blitz_mode.c referee
static void run_blitz_match(Session *s, int first_player) {
    Mode blitz_mode;
    if (!create_blitz_mode(&blitz_mode, &s->game)) {
        fprintf(stderr, "Failed to create blitz mode.\n");
        return;
    }

    long long now_ms = get_time_ms();
    blitz_mode.start(blitz_mode.self, first_player, now_ms);

    char guess_buf[256];
    char info_buf[512];

    while (!blitz_mode.is_over(blitz_mode.self)) {
        now_ms = get_time_ms();
        long long wait_ms = blitz_mode.ms_until_deadline(blitz_mode.self, now_ms);

        // Send current views to both players
        for (int i = 0; i < 2; i++) {
            char *view = NULL;
            if (view_game(&s->game, i, &view) == GAME_OK) {
                char header[1024];
                snprintf(header, sizeof(header), "\n=== BLITZ MODE (Time left checked server-side) ===\n%s\n", view);
                send(s->fds[i], header, strlen(header), 0);
                free(view);
            }
        }

        // Setup select() with timeout matching deadline
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(s->fds[0], &readable);
        FD_SET(s->fds[1], &readable);
        int max_fd = (s->fds[0] > s->fds[1]) ? s->fds[0] : s->fds[1];

        struct timeval *tv_ptr = NULL;
        struct timeval tv;
        if (wait_ms >= 0) {
            tv.tv_sec = wait_ms / 1000;
            tv.tv_usec = (wait_ms % 1000) * 1000;
            tv_ptr = &tv;
        }

        int sel = select(max_fd + 1, &readable, NULL, NULL, tv_ptr);
        now_ms = get_time_ms();

        if (sel < 0) {
            perror("select() failed");
            break;
        } else if (sel == 0) {
            // Timeout reached for active player's clock
            Result r = blitz_mode.on_timeout(blitz_mode.self, now_ms);
            if (r.kind == RESULT_PLAYER_OUT) {
                snprintf(info_buf, sizeof(info_buf), "\nPlayer %d's clock ran out and is out! Player %d continues alone.\n", r.player, r.next_player);
                send(s->fds[0], info_buf, strlen(info_buf), 0);
                send(s->fds[1], info_buf, strlen(info_buf), 0);
            }
        } else {
            // Check which socket has input ready
            for (int i = 0; i < 2; i++) {
                if (FD_ISSET(s->fds[i], &readable)) {
                    memset(guess_buf, 0, sizeof(guess_buf));
                    int n = recv(s->fds[i], guess_buf, sizeof(guess_buf) - 1, 0);
                    if (n <= 0) {
                        // Forfeit / Disconnect
                        Result r = blitz_mode.on_forfeit(blitz_mode.self, i, now_ms);
                        (void)r;
                        break;
                    }
                    guess_buf[strcspn(guess_buf, "\r\n")] = 0;

                    Result r = blitz_mode.on_guess(blitz_mode.self, i, guess_buf, now_ms);
                    if (r.kind == RESULT_WRONG) {
                        char *wrong_msg = "Incorrect guess! Hint added, turn passes.\n";
                        send(s->fds[i], wrong_msg, strlen(wrong_msg), 0);
                    } else if (r.kind == RESULT_PLAYER_OUT) {
                        snprintf(info_buf, sizeof(info_buf), "\nClock expired on guess for Player %d!\n", r.player);
                        send(s->fds[0], info_buf, strlen(info_buf), 0);
                        send(s->fds[1], info_buf, strlen(info_buf), 0);
                    }
                }
            }
        }
    }

    // Announce Blitz Winner
    int winner = blitz_mode.winner(blitz_mode.self);
    if (winner == -1) {
        snprintf(info_buf, sizeof(info_buf), "\nBlitz Match ended in a Tie!\n");
    } else {
        snprintf(info_buf, sizeof(info_buf), "\nPlayer %d wins Blitz Mode!\n", winner);
    }
    send(s->fds[0], info_buf, strlen(info_buf), 0);
    send(s->fds[1], info_buf, strlen(info_buf), 0);

    destroy_blitz_mode(&blitz_mode);
}

void run_session(Session *s) {
    if (s == NULL) return;

    // 1. Prompt Player 0 to choose mode (1 for Classic, 2 for Blitz)
    const char *mode_prompt = "Choose Game Mode:\n1. Classic Mode\n2. Blitz Mode\nEnter choice (1 or 2): ";
    send(s->fds[0], mode_prompt, strlen(mode_prompt), 0);
    send(s->fds[1], "Waiting for Player 0 to choose game mode...\n", 46, 0);

    char choice_buf[16];
    memset(choice_buf, 0, sizeof(choice_buf));
    recv(s->fds[0], choice_buf, sizeof(choice_buf) - 1, 0);
    
    int mode_choice = atoi(choice_buf);
    if (mode_choice == 2) {
        s->mode = MODE_BLITZ;
    } else {
        s->mode = MODE_CLASSIC;
    }

    // 2. Load corresponding word lists
    if (load_chains_for_players(s) != 0) {
        char *err_msg = "Server error loading word bank for selected mode.\n";
        send(s->fds[0], err_msg, strlen(err_msg), 0);
        send(s->fds[1], err_msg, strlen(err_msg), 0);
        return;
    }

    // 3. Coin Toss to decide first player
    int caller = pick_caller();
    CoinSide flip = flip_coin();
    
    char prompt[128];
    snprintf(prompt, sizeof(prompt), "Player %d is calling the coin toss (0 for Heads, 1 for Tails): ", caller);
    send(s->fds[caller], prompt, strlen(prompt), 0);

    char guess_buf[16];
    memset(guess_buf, 0, sizeof(guess_buf));
    recv(s->fds[caller], guess_buf, sizeof(guess_buf) - 1, 0);
    CoinSide call = (atoi(guess_buf) == 1) ? COIN_TAILS : COIN_HEADS;

    int first_player = decide_first_player(caller, call, flip);
    
    char info_buf[256];
    snprintf(info_buf, sizeof(info_buf), "Coin landed on %s! Player %d goes first.\n", 
             (flip == COIN_HEADS ? "Heads" : "Tails"), first_player);
    send(s->fds[0], info_buf, strlen(info_buf), 0);
    send(s->fds[1], info_buf, strlen(info_buf), 0);

    // 4. Branch to appropriate mode loop
    if (s->mode == MODE_BLITZ) {
        run_blitz_match(s, first_player);
    } else {
        run_classic_match(s, first_player);
    }
}
