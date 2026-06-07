#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "player.h"
#include "menu.h"
#include "../world/world.h"
#include "../renderer/shader.h"
#include "../renderer/texture.h"
#include "../renderer/crosshair.h"
#include "../renderer/hud.h"
#include "../renderer/selection.h"

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();

    void handleCharacterInput(unsigned int codepoint);
    void handleKeyInput(int key, int scancode, int action, int mods);
    void handleScroll(double yoffset);

private:
    void update(float deltaTime);
    void render(float deltaTime);
    void loadActiveShader(const std::string& shaderName);
    void cleanup();

    World world;
    Player player;
    Menu menu;
    Hud hud;
    Crosshair* crosshair; // Переведено на указатель для безопасной отложенной инициализации
    BlockSelection selection;

    Shader* ourShader;
    Shader* selectionShader;

    // Шейдеры и буферы игрока
    unsigned int pShader;
    unsigned int pVAO, pVBO;

    // Шейдеры и буферы солнца
    unsigned int sunShader;
    unsigned int sunVAO, sunVBO;

    Texture* blockAtlas;

    float deltaTime;
    float lastFrame;

    bool f11PressedLastFrame;
    bool escPressedLastFrame;
    bool uiLeftMousePressed;
    bool leftMousePressed;
    bool rightMousePressed;

    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};