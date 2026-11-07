/*
Implementation of the Chain Blitz referee declared in blitz_mode.h.

Layout of this file:
    1. BlitzState: the mode's private data (hidden from every other file)
    2. Small helpers
    3. The seven Mode functions (all static (private class): reached only through the Mode table)
    4. create_blitz_mode / destroy_blitz_mode

Time handling: a clock is stored as a DEADLINE (the moment it runs out), not
a countdown, so it cannot drift. When a clock pauses, the time still left is
saved in time_left[]. When it resumes, a new deadline is made from it.
*/

#include "blitz_mode.h"

#include <stdlib.h>   /* calloc, free */

/* ======== 1. PRIVATE STATE ======== */

typedef struct {
    Game *game;               /**< Borrowed from the session. Never freed here. */
    bool started;             /**< start() has been called with a valid first player. */
    bool over;                /**< The match has ended. */
    int active;               /**< The player whose clock is running. */
    long long deadline;       /**< When the active player's clock runs out. */
    long long time_left[2];   /**< Saved ms per player; refreshed whenever a clock pauses. */
    bool out[2];              /**< This player's clock has run out. */
    int finisher;             /**< The player who completed their list, or -1. */
    int forfeiter;            /**< The player who left, or -1. */
} BlitzState;

/* ======== 2. HELPERS ======== */

static Result make_result(ResultKind kind, int player, int next_player) {
    Result r = { kind, player, next_player };
    return r;
}

/* Whose turn to report in an IGNORED result: the active player, or -1 if nothing is running. */
static int current_turn(const BlitzState *s) {
    return (s->started && !s->over) ? s->active : -1;
}

/*
The active player's clock has run out. Mark them out; if the opponent still
has time, hand over the turn and start the opponent's clock at now_ms
(the moment the server noticed), otherwise the match is over.
*/
static Result expire_active(BlitzState *s, long long now_ms) {
    int expired = s->active;
    int other = 1 - expired;

    s->out[expired] = true;
    s->time_left[expired] = 0;

    if (s->out[other]) {
        s->over = true;
        return make_result(RESULT_GAME_OVER, expired, -1);
    }

    s->active = other;
    s->deadline = now_ms + s->time_left[other];
    return make_result(RESULT_PLAYER_OUT, expired, other);
}

/* ======== 3. THE MODE FUNCTIONS ======== */

static void blitz_start(void *self, int first_player, long long now_ms) {
    BlitzState *s = self;

    if (s->started) return;                                 /* already running */
    if (first_player != 0 && first_player != 1) return;    /* stay unstarted */

    s->started = true;
    s->active = first_player;
    s->deadline = now_ms + s->time_left[first_player];
}

static Result blitz_on_guess(void *self, int player, const char *guess, long long now_ms) {
    BlitzState *s = self;

    /* Not running, or not this player's turn: nothing happens. */
    if (!s->started || s->over || player != s->active)
        return make_result(RESULT_IGNORED, player, current_turn(s));

    /* A guess at or after the deadline is late: the clock ran out first. */
    if (now_ms >= s->deadline) return expire_active(s, now_ms);

    if (check_guess(s->game, player, guess)) {
        advance(s->game, player);

        if (is_game_finished(s->game, player)) {
            s->over = true;
            s->finisher = player;
            return make_result(RESULT_GAME_OVER, player, -1);
        }
        return make_result(RESULT_CORRECT, player, player);     /* keeps the turn */
    }

    /* Wrong (including empty, misspelled, incomplete or NULL): add a hint. */
    add_hint(s->game, player);

    int other = 1 - player;
    if (s->out[other])                                          /* opponent is out: turn stays */
        return make_result(RESULT_WRONG, player, player);

    s->time_left[player] = s->deadline - now_ms;                /* pause this clock, saving what is left */
    s->active = other;
    s->deadline = now_ms + s->time_left[other];                 /* resume the other clock */
    return make_result(RESULT_WRONG, player, other);
}

static Result blitz_on_timeout(void *self, long long now_ms) {
    BlitzState *s = self;

    if (!s->started || s->over || now_ms < s->deadline)         /* nothing is due */
        return make_result(RESULT_IGNORED, -1, current_turn(s));

    return expire_active(s, now_ms);
}

static Result blitz_on_forfeit(void *self, int player, long long now_ms) {
    BlitzState *s = self;
    (void)now_ms;   /* a forfeit does not depend on the time */

    if (!s->started || s->over || (player != 0 && player != 1))
        return make_result(RESULT_IGNORED, player, current_turn(s));

    s->over = true;
    s->forfeiter = player;
    return make_result(RESULT_GAME_OVER, player, -1);
}

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
