#ifndef GAME_H
#define GAME_H

#include <stddef.h>     // size_t
#include <stdbool.h>    // bool, true, false

// ========== SETUP ==========

// Everything one player needs during a match
typedef struct {
    char **chain;           // an array of heap-allocated words
    size_t chain_len;       // number of words in chain
    size_t position;        // index of the word being guessed
    size_t hint_level;      // letters revealed on the current word after incorrect guesses
    int score;              // number of correct guesses so far
} Player;

// The whole game state: two players, indexed 0 and 1
typedef struct {
    Player players[2];
} Game;

// stores all possible status (types of error and ok status)
typedef enum {
    GAME_OK = 0,
    GAME_ERR_EMPTY_CHAIN,
    GAME_ERR_EMPTY_WORD,
    GAME_ERR_BAD_PLAYER,
    GAME_ERR_NOMEM,
} GameStatus;

// initialize Game g with the chain of player0 and its size, and the chain of player1 and its size.
// Copies the words, so the caller keeps ownership of its own arrays.
/*
    const char -> read-only char
    const char * -> modifiable pointer pointing to read-only characters
    const char *const -> read-only pointer to read-only characters
    const char *const *words0 -> modifiable pointer to read-only pointers to read-only characters
*/
GameStatus game_init(Game *g, const char *const *words0, size_t n0, const char *const *words1, size_t n1);

// free all allocated memory of game_init
void free_game(Game *g);


// ========== READ-ONLY ==========

// builds the masked chain a player is allowed to see, words separated by '\n'.
// On success *out is a malloc'd null-terminated string: the caller must free() it.
GameStatus view_game(const Game *g, int player, char **out);

// asks if game is finished
bool is_game_finished(const Game *g, int player);

// checks player guess if it matches expected word
bool check_guess(const Game *g, int player, const char *guess);


// ========== WRITE ==========

// advance player to next word in chain
GameStatus advance(Game *g, int player);

// add one to player hint after an incorrect guess
GameStatus add_hint(Game *g, int player);


// ========== RESULT =========

// gets player score
int get_score(const Game *g, int player);


// ========== COIN FLIP ==========

// the two sides of the coin
typedef enum { COIN_HEADS, COIN_TAILS } CoinSide;

// randomly picks which player (0 or 1) makes the call.
// Uses rand(): call srand() once at program start, or every run picks the same player.
int pick_caller(void);

// flips the coin: heads or tails.
// Uses rand(): call srand() once at program start.
CoinSide flip_coin(void);

// decides who goes first: the caller if the call matches the flip, otherwise the other player.
// Returns 0 or 1, or -1 if caller is not 0 or 1, or call/flip is not a valid side.
// Pure function: no randomness inside, so it is easy to test.
int decide_first_player(int caller, CoinSide call, CoinSide flip);

/* ======== TURN PROMPT ======== */

/**
 * @brief The message to show a player about whose turn it is.
 *
 * Send it right after each view_game() output.
 *   - viewer is the one playing  : ">>> YOUR TURN <<<" and an "input: " prompt
 *   - viewer is the other player : "... waiting for the other player ..."
 *   - nobody's turn (turn_player -1) or an invalid viewer: an empty string
 *
 * @param viewer       The player who will receive the message (0 or 1).
 * @param turn_player  Whose turn it is (0 or 1), or -1 if none. Take it from
 *                     Result.next_player.
 * @return A string literal. Do not free or modify it.
 */
const char *turn_prompt(int viewer, int turn_player);

#endif