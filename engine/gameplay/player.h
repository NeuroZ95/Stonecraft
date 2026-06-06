#pragma once
#include "camera.h"
#include "../window/window.h"
#include "../world/world.h"

class Player {
public:
    Camera camera;
    glm::vec3 velocity;
    glm::vec3 position; // Физическая позиция ног игрока

    float speed;
    float airSpeed;
    float sensitivity;
    bool isGrounded;
    bool isSneaking;    // Флаг приседания
    float eyeHeight;    // Текущая высота камеры относительно ног

    float lastX, lastY;
    bool firstMouse;

    // Состояние хотбара
    static const int HOTBAR_SIZE = 9;
    block_t hotbar[HOTBAR_SIZE];
    int selectedSlot;

    Player(glm::vec3 startPosition);

    void update(float deltaTime, World& world);
    void handleKeyboard(float deltaTime);
    void handleMouse();
    void checkCollisions(World& world, glm::vec3 oldPosition);
    bool checkAABBCollision(glm::vec3 pos, World& world);
    bool isPositionSupported(glm::vec3 feetPos, World& world); // Проверка блока под ногами
};