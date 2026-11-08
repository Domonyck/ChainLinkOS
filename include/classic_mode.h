/*
Classic rules: 
untimed, turn-based mode where players take turns guessing a list of 5 connected words.

Rules implemented:
  - Untimed turns: players have no turn clocks or overall time limits.
  - First player turn is determined by a coin flip in the session before starting.
  - A correct guess advances the word position and allows the player to keep 
    their turn to guess the next word.
  - A wrong guess (empty, misspelled, incomplete or NULL) adds a hint letter 
    to the current word and passes the turn to the opponent.
  - The match ends when a player finishes their 5-word list completely, 
    or when a player forfeits.
  - Winner: the player who completes their list first, or the remaining 
    player after a forfeit.

The mode borrows the Game (the session creates and frees it) and ignores
time parameters passed by the server.
*/

#ifndef CLASSIC_MODE_H
#define CLASSIC_MODE_H

#include "mode.h"
#include "game.h"

/*
Build a Classic mode around a Game.

mode: Filled in on success. On failure mode->self is NULL.
game: The game to referee. Borrowed: not owned, never freed here.
return true on success, false if an argument is NULL or memory ran out.

Ownership: pass the Mode to destroy_classic_mode() exactly once. Destroy it
before free_game(), since the mode points at the game.
*/
bool create_classic_mode(Mode *mode, Game *game);

/*
Release a mode created by create_classic_mode(). Safe with NULL and
safe to call twice. Does not free the Game.
*/
void destroy_classic_mode(Mode *m);

#endif
