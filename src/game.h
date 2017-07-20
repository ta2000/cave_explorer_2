#ifndef GAME_H_
#define GAME_H_

#include "stdbool.h"

struct game
{
    bool running;
};

void game_init(struct game* self);
void game_setup_renderer(struct game* self);

#endif
