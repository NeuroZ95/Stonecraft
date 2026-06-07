#include "engine/gameplay/game.h"

int main() {
    Game game;
    if (game.init()) {
        game.run();
    }
    return 0;
}