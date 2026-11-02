#include "../include/game.h"
#include <stdbool.h>
#include <stddef.h>

bool classic_mode_process_turn(Game *g, int player, const char *guess) {
    if (g == NULL || guess == NULL) {
        return false;
    }

    if (is_game_finished(g, player)) {
        return false;
    }

    if (check_guess(g, player, guess)) {
        advance(g, player);
        return true;  
    } else {
        add_hint(g, player);
        return false;
    }
}
