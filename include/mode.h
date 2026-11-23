#ifndef MODE_H
#define MODE_H

#include <stdbool.h>

typedef enum {
    RESULT_IGNORED,      
    RESULT_CORRECT,      
    RESULT_WRONG,        
    RESULT_PLAYER_OUT,   
    RESULT_GAME_OVER     
} ResultKind;

typedef struct {
    ResultKind kind;
    int player;
    int next_player;
} Result;


typedef struct {
    void (*start)(void *self, int first_player, long long now_ms);

    Result (*on_guess)(void *self, int player, const char *guess, long long now_ms);

    Result (*on_timeout)(void *self, long long now_ms);

    Result (*on_forfeit)(void *self, int player, long long now_ms);

    long long (*ms_until_deadline)(const void *self, long long now_ms);

    bool (*is_over)(const void *self);

    int (*winner)(const void *self);

    void *self;
} Mode;

#endif
