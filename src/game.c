/*
It holds the state of the match and answers questions about it, and it doesn’t know about networking, clocks or modes.

It only ever says “this is correct” or “this is the current state”. Deciding what that means (who goes next, whether time ran out) belongs to the mode.
*/

#include "game.h"

#include <ctype.h>      // isspace, tolower
#include <stdlib.h>     // malloc, calloc, free
#include <string.h>     // strlen, memcpy, memset, strcmp

// Copy a string onto a heap
// Why: To give Game g its copy of the word to free later on, and the original string is never touched
static char *str_dup(const char *s) {
    size_t n = strlen(s) + 1;           // +1 for null terminator '\0'
    char *copy = malloc(n);             // allocate n bytes of memory
    if (copy) memcpy(copy, s, n);       // copy string to allocated memory, including '\0'
    return copy;
}

// trim whitespaces from both sides and lowercase i.e. normalization
static char *normalize(const char *s) {
    const char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;     /* skip leading space */
 
    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) len--;  /* drop trailing space */
 
    char *out = malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = (char)tolower((unsigned char)start[i]);
    out[len] = '\0';
    return out;
}

// check if player is valid
static bool is_player_valid(int player) {
    return player == 0 || player == 1;
}

// free player's chain and reset it to empty
static void free_chain(Player *p) {
    for (size_t i = 0; i < p->chain_len; i++) free(p->chain[i]);        // loop through the chain and free
    free(p->chain);                                                     // free all leftovers
    
    // reset chain to empty
    p->chain = NULL;
    p->chain_len = 0;
}

// validate a chain and give a player its own copy
static GameStatus copy_chain(Player *p, const char *const *words, size_t n) {
    if (words == NULL || n == 0) return GAME_ERR_EMPTY_CHAIN;   // return error if chain is empty

    for (size_t i = 0; i < n; i++) 
        if (words[i] == NULL || words[i][0] == '\0') return GAME_ERR_EMPTY_WORD;    // return error if a word in the chain is empty

    p->chain = calloc(n, sizeof(char *));       // alloc memory and fill each with 0 bytes
    if (!p->chain) return GAME_ERR_NOMEM;       // return error if no memory can be allocated

    for (size_t i = 0; i < n; i++) {
        p->chain[i] = str_dup(words[i]);
        
        // if a copy fails, free the i words already copied and report no memory
        if (!p->chain[i]) {
            p->chain_len = i;
            free_chain(p);
            return GAME_ERR_NOMEM;
        }
    }
    p->chain_len = n;
    return GAME_OK;
}

// get expected word for validation purposes
static const char *expected_word(const Game *g, int player) {
    const Player *p = &g->players[player];
    return p->chain[p->position];
}

// copies the words in each player chain to the game's internal storage
GameStatus game_init(Game *g, const char *const *words0, size_t n0, const char *const *words1, size_t n1) {
    memset(g, 0, sizeof *g);                                        // reset game to 0

    GameStatus stat = copy_chain(&g->players[0], words0, n0);       // initialize player 0
    if (stat != GAME_OK) return stat;                               // if initialization failed return the error status
    
    stat = copy_chain(&g->players[1], words1, n1);                  // initialize player 1
    if (stat != GAME_OK) {
        free_chain(&g->players[0]);                                 // if initialization failed, free player 0 chain; both must be initialized
        return stat;                                                // return the error status
    }
    return GAME_OK;
}

// free both players' chains
void free_game(Game *g) {
    for (int i = 0; i < 2; i++) free_chain(&g->players[i]);
}

GameStatus view_game(const Game *g, int player, char **out) {
    if (!is_player_valid(player)) return GAME_ERR_BAD_PLAYER;

    const Player *p = &g->players[player];

    size_t total = 1;
    for (size_t i = 0; i < p->chain_len; i++) total += strlen(p->chain[i]);
    if (p->chain_len > 0) total += p->chain_len - 1;

    char *buf = malloc(total);
    if (!buf) return GAME_ERR_NOMEM;

    size_t k = 0;

    size_t last = p->position + 1;
    if (last > p->chain_len - 1) last = p->chain_len - 1;

    for (size_t i = 0; i <= last; i++) {
        const char *word = p->chain[i];
        size_t wlen = strlen(word);
        
        for (size_t j = 0; j < wlen; j++) {
            // Reveal all guessed word
            if (i < p->position) buf[k++] = word[j];
            
            // Reveal the current word at hint level
            else if (i == p->position) buf[k++] = (j <= p->hint_level) ? word[j] : '_';
            
            // Reveal the upcoming word
            else if (i == p->position + 1) buf[k++] = (j == 0) ? word[j] : '_';
            
            // Mask the words not guessed yet
            else buf[k++] = '_';
        }
        if (i < last) buf[k++] = '\n';
    }
    
    buf[k] = '\0';

    *out = buf;
    return GAME_OK;
}

bool is_game_finished(const Game *g, int player) {
    if (!is_player_valid(player)) return false;

    return g->players[player].position >= g->players[player].chain_len;
}

bool check_guess(const Game *g, int player, const char *guess) {
    if (!is_player_valid(player) || guess == NULL) return false;
    if (is_game_finished(g, player)) return false;

    char *norm_guess = normalize(guess);
    char *norm_word = normalize(expected_word(g, player));

    bool match = (norm_guess && norm_word && strcmp(norm_guess, norm_word) == 0);

    free(norm_guess);
    free(norm_word);
    return match;
}

GameStatus advance(Game *g, int player) {
    if (!is_player_valid(player)) return GAME_ERR_BAD_PLAYER;
    
    Player *p = &g->players[player];

    p->score++;
    p->position++;
    p->hint_level = 0;
    return GAME_OK;
}

GameStatus add_hint(Game *g, int player) {
    if (!is_player_valid(player)) return GAME_ERR_BAD_PLAYER;

    g->players[player].hint_level++;
    return GAME_OK;
}

int get_score(const Game *g, int player) {
    if (!is_player_valid(player)) return -1;

    return g->players[player].score;
}

// ========== COIN FLIP ==========
// The only part of this file that uses randomness. It never reads or changes a Game.
 
// randomly pick the player who makes the call
int pick_caller(void) {
    return rand() % 2;
}
 
// flip the coin
CoinSide flip_coin(void) {
    return (rand() % 2 == 0) ? COIN_HEADS : COIN_TAILS;
}
 
// the caller goes first if the call matches the flip, otherwise the other player does
int decide_first_player(int caller, CoinSide call, CoinSide flip) {
    if (!is_player_valid(caller)) return -1;
 
    // an enum can hold any int, so reject values that are not a real side
    // for validation purposes
    if (call != COIN_HEADS && call != COIN_TAILS) return -1;
    if (flip != COIN_HEADS && flip != COIN_TAILS) return -1;
 
    if (call == flip) return caller;
    return 1 - caller;      // 0 -> 1, 1 -> 0
}