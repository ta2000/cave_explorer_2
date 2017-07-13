#include <stdio.h>
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION

#include "renderer.h"
#include "game.h"

int main(int argc, char* argv[])
{
    struct game game;
    game_init(&game);
    game_setup_renderer(&game);

    return 0;
}
