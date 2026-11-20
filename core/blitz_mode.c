#include "blitz_mode.h"

#include <stdlib.h>   // calloc, free

typedef struct {
    Game *game;               // *game is borrowed from session.c
    bool started;             // checks if game has started
    bool over;                // checks if match ended
    int active;               // the current player who's clock is running
    long long deadline;       // when will the active player clock run out
    long long time_left[2];   // the time left in the player's clock when it pauses
    bool out[2];              // checks who's clock ran out
    int finisher;             // the player who completes their list, or none
    int forfeiter;            // the player who left due to server connection issues
} BlitzState;

static Result make_result(ResultKind kind, int player, int next_player) {
    Result r = { kind, player, next_player };
    return r;
}

// checks who's turn is it
static int current_turn(const BlitzState *s) {
    // when game started and is not over, the current turn is the active player, otherwise None (-1)
    return (s->started && !s->over) ? s->active : -1;
}

// The active player's clock has run out. Mark them out; if the opponent still
// has time, hand over the turn and start the opponent's clock at now_ms
// (the moment the server noticed), otherwise the match is over.
static Result expire_active(BlitzState *s, long long now_ms) {
    // mark the active's player as expired if their clock has run out
    int expired = s->active;
    // the opponent is the other, which all turns will be handed until their time runs out
    int other = 1 - expired;

    // mark the player as out (clock run out)
    s->out[expired] = true;
    // change expired player's time to 0
    s->time_left[expired] = 0;

    // if the other player's clock run out
    if (s->out[other]) {
        // match ends
        s->over = true;
        return make_result(RESULT_GAME_OVER, expired, -1);
    }

    // otherwise, the opponent continues playing
    s->active = other;
    // start the opponent's clock with now_ms and time_left
    s->deadline = now_ms + s->time_left[other];
    return make_result(RESULT_PLAYER_OUT, expired, other);
}

// start mode session, assign clock for each player, and give current turn to first player
static void blitz_start(void *self, int first_player, long long now_ms) {
    BlitzState *s = self;

    // if session already running, continue
    if (s->started) return;
    // validate player before running
    if (first_player != 0 && first_player != 1) return; 

    // start the session
    s->started = true;
    // the winner of the rock, paper, scissors will play first
    s->active = first_player;
    // set the clock for the first player
    s->deadline = now_ms + s->time_left[first_player];
}

// checks player turn and their clock and guess validation and if correct
static Result blitz_on_guess(void *self, int player, const char *guess, long long now_ms) {
    BlitzState *s = self;

    // session not running, or session is over, or not this player's turn, IGNORED.
    if (!s->started || s->over || player != s->active)
        return make_result(RESULT_IGNORED, player, current_turn(s));

    // if player guessed at or after the deadline, the turn goes to the opponent, guess not accepted
    if (now_ms >= s->deadline) return expire_active(s, now_ms);

    // if guess is valid and correct
    if (check_guess(s->game, player, guess)) {
        // move to next word of the chain
        advance(s->game, player);

        // guess is valid, but game is finished
        if (is_game_finished(s->game, player)) {
            // mark the game as over
            s->over = true;
            // the finisher is active player
            s->finisher = player;
            return make_result(RESULT_GAME_OVER, player, -1);
        }
        keeps the turn of the active player
        return make_result(RESULT_CORRECT, player, player);     
    }

    // Wrong: empty, mispelled, null, then add hint 
    add_hint(s->game, player);

    // turn stays if opponent is out
    int other = 1 - player;
    if (s->out[other])                                          
        return make_result(RESULT_WRONG, player, player);

    // pause clock of player before turning to other player
    s->time_left[player] = s->deadline - now_ms;                
    // make opponent active player
    s->active = other;
    // resume clock of opponent
    s->deadline = now_ms + s->time_left[other];                 
    return make_result(RESULT_WRONG, player, other);
}

// ...
static Result blitz_on_timeout(void *self, long long now_ms) {
    BlitzState *s = self;

    if (!s->started || s->over || now_ms < s->deadline)         /* nothing is due */
        return make_result(RESULT_IGNORED, -1, current_turn(s));

    return expire_active(s, now_ms);
}

// ...
static Result blitz_on_forfeit(void *self, int player, long long now_ms) {
    BlitzState *s = self;
    (void)now_ms;   // a forfeit does not depend on the time

    // if session not running and player not vaid, return INGORE.
    if (!s->started || s->over || (player != 0 && player != 1))
        return make_result(RESULT_IGNORED, player, current_turn(s));

    // mark the match as over
    s->over = true;
    // the player who left is the forfeiter
    s->forfeiter = player;
    // return game over
    return make_result(RESULT_GAME_OVER, player, -1);
}

// ...
static long long blitz_ms_until_deadline(const void *self, long long now_ms) {
    const BlitzState *s = self;

    if (!s->started || s->over) return -1;      /* nothing to wait for */

    long long ms = s->deadline - now_ms;
    return ms > 0 ? ms : 0;                     /* 0 = something is due now */
}

static bool blitz_is_over(const void *self) {
    const BlitzState *s = self;
    return s->over;
}

static int blitz_winner(const void *self) {
    const BlitzState *s = self;

    if (!s->over) return -1;
    if (s->forfeiter != -1) return 1 - s->forfeiter;    /* the player who stayed */
    if (s->finisher != -1) return s->finisher;          /* completed their list */

    int a = get_score(s->game, 0);
    int b = get_score(s->game, 1);
    if (a > b) return 0;
    if (b > a) return 1;
    return -1;                                          /* tie: both players win */
}

/* ======== 4. CREATE / DESTROY ======== */

bool create_blitz_mode(Mode *mode, Game *game) {
    if (mode == NULL) return false;
    mode->self = NULL;
    if (game == NULL) return false;

    /* calloc zeroes everything; finisher and forfeiter need -1, set below. */
    BlitzState *s = calloc(1, sizeof *s);
    if (s == NULL) return false;

    s->game = game;
    s->time_left[0] = BLITZ_TIME_MS;
    s->time_left[1] = BLITZ_TIME_MS;
    s->finisher = -1;
    s->forfeiter = -1;

    mode->start             = blitz_start;
    mode->on_guess          = blitz_on_guess;
    mode->on_timeout        = blitz_on_timeout;
    mode->on_forfeit        = blitz_on_forfeit;
    mode->ms_until_deadline = blitz_ms_until_deadline;
    mode->is_over           = blitz_is_over;
    mode->winner            = blitz_winner;
    mode->self              = s;

    return true;
}
int blitz_active_player(const void *self) {
    const BlitzState *s = self;
    if (s == NULL || !s->started || s->over) return -1;
    return s->active;
}

long long get_time_left(const void *self, int player, long long now_ms) {
    const BlitzState *s = self;
    if (s == NULL || !s->started || s->over) return 0;
    if (s->active == player) {
        long long left = s->deadline - now_ms;
        return left > 0 ? left : 0;
    }
    return s->time_left[player];
}
void destroy_blitz_mode(Mode *m) {
    if (m == NULL) return;

    free(m->self);
    m->self = NULL;
}
