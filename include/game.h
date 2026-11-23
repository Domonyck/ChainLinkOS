#ifndef GAME_H
#define GAME_H

#include <stddef.h>   
#include <stdbool.h>    

typedef struct {
    char **chain;          
    size_t chain_len;      
    size_t position;        
    size_t hint_level;      
    int score;              
} Player;

typedef struct {
    Player players[2];
} Game;

typedef enum {
    GAME_OK = 0,
    GAME_ERR_EMPTY_CHAIN,
    GAME_ERR_EMPTY_WORD,
    GAME_ERR_BAD_PLAYER,
    GAME_ERR_NOMEM,
} GameStatus;

GameStatus game_init(Game *g, const char *const *words0, size_t n0, const char *const *words1, size_t n1);

void free_game(Game *g);

GameStatus view_game(const Game *g, int player, char **out);

bool is_game_finished(const Game *g, int player);

bool check_guess(const Game *g, int player, const char *guess);

GameStatus advance(Game *g, int player);

GameStatus add_hint(Game *g, int player);

int get_score(const Game *g, int player);

typedef enum { COIN_HEADS, COIN_TAILS } CoinSide;

int pick_caller(void);

CoinSide flip_coin(void);

int decide_first_player(int caller, CoinSide call, CoinSide flip);

const char *turn_prompt(int viewer, int turn_player);

#endif
