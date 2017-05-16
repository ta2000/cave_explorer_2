#ifndef GAME_H_
#define GAME_H_

struct game
{
    void (*setup_renderer)(struct game*);
};

void game_init(struct game* self);
void game_setup_vk_renderer(struct game* self);

#endif
