#include "world_utils.h"
#include "../gameplay/player.h"
#include "world.h"
#include <GLFW/glfw3.h>
#include <cctype>

unsigned int WorldUtils::parseSeed(const std::string& input) {
    if (input.empty()) {
        return static_cast<unsigned int>(glfwGetTime() * 1000.0f);
    }

    bool isNumeric = true;
    for (char c : input) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            isNumeric = false;
            break;
        }
    }

    if (isNumeric) {
        try {
            unsigned long long parsedValue = std::stoull(input);
            return static_cast<unsigned int>(parsedValue);
        }
        catch (...) {}
    }

    unsigned int calculatedHash = 0;
    for (char c : input) {
        calculatedHash = calculatedHash * 31 + static_cast<unsigned int>(c);
    }
    return calculatedHash;
}

void WorldUtils::resetPlayerToSpawn(Player& player, World& world) {
    float spawnX = 0.0f; // SPAWN_COORD_X
    float spawnZ = 0.0f; // SPAWN_COORD_Z
    float spawnY = 40.0f;

    for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
        if (world.getBlock(static_cast<int>(spawnX), y, static_cast<int>(spawnZ)) != BLOCK_AIR) {
            spawnY = static_cast<float>(y) + 1.0f + 1.62f;
            break;
        }
    }
    player.position = glm::vec3(spawnX, spawnY - 1.62f, spawnZ);
    player.velocity = glm::vec3(0.0f);
    player.isGrounded = false;
    player.camera.position = player.position + glm::vec3(0.0f, player.eyeHeight, 0.0f);
}