#include <stdio.h>
#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION

#include "renderer.h"
#include "game.h"

int main(int argc, char* argv[])
{
    printf("Cave Explorer 2\n");

    struct game game;
    game_init(&game);
    assert(game.setup_renderer);
    game.setup_renderer(&game);

    return 0;
}
