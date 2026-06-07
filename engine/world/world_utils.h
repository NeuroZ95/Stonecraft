#pragma once
#include <string>
#include <glm/glm.hpp>

class Player;
class World;

class WorldUtils {
public:
    static unsigned int parseSeed(const std::string& input);
    static void resetPlayerToSpawn(Player& player, World& world);
};