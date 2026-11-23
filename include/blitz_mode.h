#ifndef BLITZ_MODE_H
#define BLITZ_MODE_H

#include "mode.h"
#include "game.h"

#define BLITZ_TIME_MS 60000LL

bool create_blitz_mode(Mode *mode, Game *game);
int blitz_active_player(const void *self);
long long get_time_left(const void *self, int player, long long now_ms);

void destroy_blitz_mode(Mode *m);



#endif 
