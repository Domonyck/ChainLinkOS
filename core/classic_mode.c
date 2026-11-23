#include "classic_mode.h"
#include <stdlib.h>  

typedef struct {
    Game *game;     
    bool started;      
    bool over;      
    int active;        
    int winner;       
} ClassicState;

static Result make_result(ResultKind kind, int player, int next_player) {
    Result r = { kind, player, next_player };
    return r;
}

static int current_turn(const ClassicState *s) {
    return (s->started && !s->over) ? s->active : -1;
}

static void classic_start(void *self, int first_player, long long now_ms) {
    ClassicState *s = self;
    (void)now_ms;   

    if (s->started) return;                               
    if (first_player != 0 && first_player != 1) return;    

    s->started = true;
    s->active = first_player;
}

static Result classic_on_guess(void *self, int player, const char *guess, long long now_ms) {
    ClassicState *s = self;
    (void)now_ms;

    if (!s->started || s->over || player != s->active)
        return make_result(RESULT_IGNORED, player, current_turn(s));

    if (check_guess(s->game, player, guess)) {
        advance(s->game, player);

        if (is_game_finished(s->game, player)) {
            s->over = true;
            s->winner = player;
            return make_result(RESULT_GAME_OVER, player, -1);
        }
        return make_result(RESULT_CORRECT, player, player);    
    }

    add_hint(s->game, player);

    int other = 1 - player;
    s->active = other;
    return make_result(RESULT_WRONG, player, other);
}

static Result classic_on_timeout(void *self, long long now_ms) {
    ClassicState *s = self;
    (void)now_ms;

    return make_result(RESULT_IGNORED, -1, current_turn(s));
}

static Result classic_on_forfeit(void *self, int player, long long now_ms) {
    ClassicState *s = self;
    (void)now_ms;

    if (!s->started || s->over || (player != 0 && player != 1))
        return make_result(RESULT_IGNORED, player, current_turn(s));

    s->over = true;
    s->winner = 1 - player;                                
    return make_result(RESULT_GAME_OVER, player, -1);
}

static long long classic_ms_until_deadline(const void *self, long long now_ms) {
    (void)self;
    (void)now_ms;

    return -1;                                         
}

static bool classic_is_over(const void *self) {
    const ClassicState *s = self;
    return s->over;
}

static int classic_winner(const void *self) {
    const ClassicState *s = self;

    if (!s->over) return -1;
    return s->winner;
}

/* ======== 4. CREATE / DESTROY ======== */

bool create_classic_mode(Mode *mode, Game *game) {
    if (mode == NULL) return false;
    mode->self = NULL;
    if (game == NULL) return false;

    /* calloc zeroes everything; winner needs -1, set below. */
    ClassicState *s = calloc(1, sizeof *s);
    if (s == NULL) return false;

    s->game = game;
    s->winner = -1;

    mode->start = classic_start;
    mode->on_guess = classic_on_guess;
    mode->on_timeout = classic_on_timeout;
    mode->on_forfeit = classic_on_forfeit;
    mode->ms_until_deadline = classic_ms_until_deadline;
    mode->is_over = classic_is_over;
    mode->winner = classic_winner;
    mode->self = s;

    return true;
}

void destroy_classic_mode(Mode *m) {
    if (m == NULL) return;

    free(m->self);
    m->self = NULL;
}
