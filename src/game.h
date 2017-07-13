#ifndef GAME_H_
#define GAME_H_

struct game
{
    int no_warning;
};

void game_init(struct game* self);
void game_setup_renderer(struct game* self);

#endif
