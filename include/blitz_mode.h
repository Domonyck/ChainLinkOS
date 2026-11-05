/*
Chain Blitz rules: 
each player has their own 30-second clock that runs only on their turn; like a chess clock. the most correct words wins.

Rules implemented:
  - Each player has 30 seconds in total, paused when it is not their turn
    and resumed from where it stopped (not reset) when the turn comes back.
  - A correct guess keeps the turn.
  - A wrong guess (empty, misspelled, incomplete (extra space in between characters) or NULL) adds a hint and
    passes the turn. If the opponent is already out of time, the turn stays.
  - A guess that arrives at or after the deadline is late: the clock runs
    out first and the guess is not counted.
  - When a clock runs out that player is out and the other plays on alone.
  - The match ends when a player finishes their list, when both clocks have
    run out, or when a player forfeits.
  - Winner: the player who finished, otherwise the other player of a
    forfeit, otherwise the higher score; equal scores are a tie.

The mode borrows the Game (the session creates and frees it) and never
reads a clock: the session passes in now_ms, the server's monotonic time.
*/

#ifndef BLITZ_MODE_H
#define BLITZ_MODE_H

#include "mode.h"
#include "game.h"

/* Each player's total thinking time in milliseconds. */
#define BLITZ_TIME_MS 60000LL

/*
Build a Blitz mode around a Game.

mode: Filled in on success. On failure mode->self is NULL.
game: The game to referee. Borrowed: not owned, never freed here.
return true on success, false if an argument is NULL or memory ran out.

Ownership: pass the Mode to destroy_blitz_mode() exactly once. Destroy it
before free_game(), since the mode points at the game.
*/
bool create_blitz_mode(Mode *mode, Game *game);

/*
Release a mode created by create_blitz_mode(). Safe with NULL and
safe to call twice. Does not free the Game.
*/
void destroy_blitz_mode(Mode *m);

#endif 