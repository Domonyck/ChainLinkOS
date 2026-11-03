/**
The contract between the Session (the game master) and a game mode (the referee): Result and Mode.

Every mode (Blitz, and later Classic) fills in a Mode: a table of function pointers plus
a private state pointer. The session calls the table and never needs to know which mode is behind it.

The mode never touches a socket and never reads a clock. The session reads
the server's own monotonic clock and passes the time in as now_ms
(milliseconds). Client clocks and client timestamps are never used.

Every function that can change the match returns a Result by value, so there
is nothing to allocate or free. Result tells the session what to broadcast.

===== IMPORTANT! =====
There is no mode.c because mode.h only defines the common interface 
that every game mode must follow. It declares the Mode structure,
function pointers, and Result types needed for communication.

The actual game logic is implemented separately in blitz_mode.c
and classic_mode.c. Both use mode.h to ensure they follow the
same interface when communicating with the Session, which is talks directly to the server.

This allows the Session to interact with either game mode
without needing to know its internal implementation.
*/

#ifndef MODE_H
#define MODE_H

#include <stdbool.h>

/*
What happened. like http status codes
*/
typedef enum {
    RESULT_IGNORED,      /**< Nothing changed: wrong player, not started, already over, or nothing due. */
    RESULT_CORRECT,      /**< Correct guess. The same player keeps the turn. */
    RESULT_WRONG,        /**< Wrong guess, a hint was added. next_player says who plays now. */
    RESULT_PLAYER_OUT,   /**< A player's clock ran out. next_player says who plays on alone. */
    RESULT_GAME_OVER     /**< The match ended. Call winner(). */
} ResultKind;

/*
The outcome of one call, in the form the session needs to broadcast.

| kind              | player                         | next_player                          |
|-------------------|--------------------------------|--------------------------------------|
| RESULT_IGNORED    | the player passed in           | whose turn it is, -1 if none         |
| RESULT_CORRECT    | who guessed                    | the same player                      |
| RESULT_WRONG      | who guessed                    | who plays now (same player if the    |
|                   |                                | opponent is out)                     |
| RESULT_PLAYER_OUT | whose clock ran out            | who plays now                        |
| RESULT_GAME_OVER  | who ended it (finisher,        | -1                                   |
|                   | forfeiter, or last clock out)  |                                      |
*/
typedef struct {
    ResultKind kind;
    int player;
    int next_player;
} Result;

/*
A game mode: function pointers plus the mode's private state.

Pass `self` as the first argument of every call: m.on_guess(m.self, ...).
*/
typedef struct {
    /* Begin the match. Called once, after the coin flip. Ignored if first_player is not 0 or 1. */
    void (*start)(void *self, int first_player, long long now_ms);

    /* A player submitted a guess. Returns what happened. */
    Result (*on_guess)(void *self, int player, const char *guess, long long now_ms);

    /* Call when ms_until_deadline() reached 0. Returns IGNORED if nothing is due yet. */
    Result (*on_timeout)(void *self, long long now_ms);

    /* A player disconnected. The other player wins. Ignored if the match is not running. */
    Result (*on_forfeit)(void *self, int player, long long now_ms);

    /*
    Milliseconds until the next time-driven event: -1 means nothing to wait
    for (not started, or over), 0 means something is due now.
    */
    long long (*ms_until_deadline)(const void *self, long long now_ms);

    /* Has the match ended? */
    bool (*is_over)(const void *self);

    /* The winner (0 or 1), or -1 for a tie or if the match is not over. Reports; sets nothing. */
    int (*winner)(const void *self);

    /* The mode's private state. Owned by the mode, freed by its destroy function. 
       The only way mode can access its state without knowing what data type is being used in mode. */
    void *self;
} Mode;

#endif