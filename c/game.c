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
    for (size_t i = 0; i < p->chain_len; i++) {
        const char *word = p->chain[i];
        size_t wlen = strlen(word);

        for (size_t j = 0; j < wlen; j++) {
            if (i < p->position) buf[k++] = word[j];
            else if (i == p->position) buf[k++] = (j <= p->hint_level) ? word[j] : '_';
            else buf[k++] = '_';
        }
        if (i + 1 < p->chain_len) buf[k++] = '\n';
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

// ========== TESTS (only compiled with -DGAME_RUN_TESTS) ==========
#ifdef GAME_RUN_TESTS

#include <stdio.h>      // printf
#include <assert.h>     // assert

static void expect_view(const Game *g, int player, const char *expected) {
    char *v = NULL;
    assert(view_game(g, player, &v) == GAME_OK);
    assert(strcmp(v, expected) == 0);
    free(v);
}

int main(void) {
    const char *words[] = {"beach", "front", "yard"};
    Game g;
    assert(game_init(&g, words, 3, words, 3) == GAME_OK);

    // starting view: both players at word 0, hint 0
    expect_view(&g, 0, "b____\n_____\n____");
    expect_view(&g, 1, "b____\n_____\n____");

    // check_guess: normalization and wrong answers
    char g1[] = "beach", g2[] = "  BEACH ", g3[] = "front", g4[] = "";
    assert(check_guess(&g, 0, g1));
    assert(check_guess(&g, 0, g2));
    assert(!check_guess(&g, 0, g3));
    assert(!check_guess(&g, 0, g4));

    // add_hint reveals one more letter, only for player 0
    add_hint(&g, 0);
    expect_view(&g, 0, "be___\n_____\n____");
    expect_view(&g, 1, "b____\n_____\n____");

    // advance: guessed word shows in full, hint resets
    advance(&g, 0);
    expect_view(&g, 0, "beach\nf____\n____");
    expect_view(&g, 1, "b____\n_____\n____");
    assert(get_score(&g, 0) == 1);
    assert(get_score(&g, 1) == 0);
    assert(check_guess(&g, 0, g3));
    assert(!check_guess(&g, 0, g1));

    // finishing the chain
    advance(&g, 0);
    advance(&g, 0);
    expect_view(&g, 0, "beach\nfront\nyard");
    assert(is_game_finished(&g, 0));
    char any[] = "anything";
    assert(!check_guess(&g, 0, any));
    assert(!is_game_finished(&g, 1));

    // bad inputs
    char *v = NULL;
    assert(view_game(&g, 2, &v) == GAME_ERR_BAD_PLAYER);
    assert(get_score(&g, -1) == -1);

    Game bad;
    assert(game_init(&bad, NULL, 0, words, 3) == GAME_ERR_EMPTY_CHAIN);
    const char *holes[] = {"cat", ""};
    assert(game_init(&bad, holes, 2, words, 3) == GAME_ERR_EMPTY_WORD);

    free_game(&g);
    printf("all tests passed\n");
    return 0;
}

#endif // GAME_RUN_TESTS