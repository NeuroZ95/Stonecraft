#include "player.h"
#include <cmath>
#include <algorithm>

Player::Player(glm::vec3 startPosition) : camera(startPosition) {
    position = startPosition - glm::vec3(0.0f, 1.62f, 0.0f);
    eyeHeight = 1.62f;
    isSneaking = false;

    velocity = glm::vec3(0.0f);
    speed = 4.0f;
    airSpeed = 2.5f;
    sensitivity = 0.1f;
    isGrounded = false;
    lastX = 640.0f;
    lastY = 360.0f;
    firstMouse = true;

    selectedSlot = 0;
    std::fill(std::begin(hotbar), std::end(hotbar), BLOCK_AIR);
    hotbar[0] = BLOCK_DIRT;
    hotbar[1] = BLOCK_GRASS;
    hotbar[2] = BLOCK_STONE;
    hotbar[3] = BLOCK_GLASS;
    hotbar[4] = BLOCK_OAK_LOG;     // Заняли пятый слот дубовым бревном
    hotbar[5] = BLOCK_OAK_LEAVES;  // Добавили листья дуба в шестой слот
}

void Player::update(float deltaTime, World& world) {
    // 1. Определение намерения присесть
    bool shiftPressed = (glfwGetKey(Window::handle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(Window::handle, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    // Если игрок хочет встать (отпустил Shift), но над головой блок, не разрешаем вставать
    bool canUncrouch = true;
    if (isSneaking && !shiftPressed) {
        isSneaking = false; // Временный тест коллизии в полный рост
        if (checkAABBCollision(position, world)) {
            canUncrouch = false;
        }
        isSneaking = true; // Возвращаем состояние обратно
    }

    if (canUncrouch) {
        isSneaking = shiftPressed && isGrounded;
    }
    else {
        isSneaking = isGrounded; // Вынужденно крадёмся, если над головой препятствие
    }

    if (Window::isCursorLocked) {
        handleMouse();
        handleKeyboard(deltaTime);
    }
    else {
        firstMouse = true;
    }

    // Сила тяжести
    if (!isGrounded) {
        velocity.y -= 9.81f * deltaTime;
    }

    if (velocity.y < -50.0f) {
        velocity.y = -50.0f;
    }

    glm::vec3 displacement = velocity * deltaTime;
    float distance = glm::length(displacement);

    if (distance > 0.001f) {
        float maxStep = 0.1f;
        int steps = std::max(1, (int)std::ceil(distance / maxStep));
        glm::vec3 stepDisplacement = displacement / (float)steps;

        for (int i = 0; i < steps; ++i) {
            glm::vec3 oldPos = position;

            // Движение по оси X
            position.x += stepDisplacement.x;
            if (checkAABBCollision(position, world)) {
                position.x = oldPos.x;
                velocity.x = 0.0f;
            }
            else if (isSneaking && isGrounded) {
                // Если сдвинулись в пустоту, отменяем шаг по X
                if (!isPositionSupported(position, world)) {
                    position.x = oldPos.x;
                    velocity.x = 0.0f;
                }
            }

            // Движение по оси Z
            position.z += stepDisplacement.z;
            if (checkAABBCollision(position, world)) {
                position.z = oldPos.z;
                velocity.z = 0.0f;
            }
            else if (isSneaking && isGrounded) {
                // Если сдвинулись в пустоту, отменяем шаг по Z
                if (!isPositionSupported(position, world)) {
                    position.z = oldPos.z;
                    velocity.z = 0.0f;
                }
            }

            // Движение по оси Y (вертикальная коллизия)
            position.y += stepDisplacement.y;
            if (checkAABBCollision(position, world)) {
                if (velocity.y < 0.0f) {
                    isGrounded = true;
                }
                else if (velocity.y > 0.0f) {
                    velocity.y = 0.0f;
                }
                position.y = oldPos.y;
                stepDisplacement.y = 0.0f;
            }
            else {
                if (stepDisplacement.y != 0.0f) {
                    isGrounded = false;
                }
            }
        }
    }
    else {
        // Проверка приземления на микро-расстоянии
        if (checkAABBCollision(position + glm::vec3(0.0f, -0.01f, 0.0f), world)) {
            isGrounded = true;
            velocity.y = 0.0f;
        }
        else {
            isGrounded = false;
        }
    }

    // 2. Плавный переход высоты камеры (теперь высота присяда выше)
    float targetEyeHeight = isSneaking ? 1.45f : 1.62f; // Глаза опускаются на комфортные 0.17 блока
    float transitionSpeed = 5.0f;
    if (eyeHeight < targetEyeHeight) {
        eyeHeight = std::min(targetEyeHeight, eyeHeight + transitionSpeed * deltaTime);
    }
    else if (eyeHeight > targetEyeHeight) {
        eyeHeight = std::max(targetEyeHeight, eyeHeight - transitionSpeed * deltaTime);
    }

    // 3. Синхронизируем положение рендеринга камеры с физическим телом
    camera.position = position + glm::vec3(0.0f, eyeHeight, 0.0f);
}

void Player::handleKeyboard(float deltaTime) {
    glm::vec3 forward = glm::normalize(glm::vec3(camera.front.x, 0.0f, camera.front.z));
    glm::vec3 right = camera.right;

    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(Window::handle, GLFW_KEY_W) == GLFW_PRESS) moveDir += forward;
    if (glfwGetKey(Window::handle, GLFW_KEY_S) == GLFW_PRESS) moveDir -= forward;
    if (glfwGetKey(Window::handle, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
    if (glfwGetKey(Window::handle, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;

    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
    }

    if (isGrounded) {
        // Снижение скорости при приседании (в Minecraft скорость падает примерно до 30% от обычной)
        float currentSpeed = isSneaking ? (speed * 0.3f) : speed;
        velocity.x = moveDir.x * currentSpeed;
        velocity.z = moveDir.z * currentSpeed;

        if (glfwGetKey(Window::handle, GLFW_KEY_SPACE) == GLFW_PRESS) {
            velocity.y = 5.0f;
            isGrounded = false;
        }
    }
    else {
        velocity.x += moveDir.x * airSpeed * deltaTime * 8.0f;
        velocity.z += moveDir.z * airSpeed * deltaTime * 8.0f;

        float currentHorizontalSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        if (currentHorizontalSpeed > speed && glm::length(moveDir) > 0.0f) {
            glm::vec3 horizVel = glm::normalize(glm::vec3(velocity.x, 0.0f, velocity.z)) * speed;
            velocity.x = horizVel.x;
            velocity.z = horizVel.z;
        }

        if (glm::length(moveDir) == 0.0f) {
            velocity.x *= std::pow(0.05f, deltaTime);
            velocity.z *= std::pow(0.05f, deltaTime);
            if (std::abs(velocity.x) < 0.05f) velocity.x = 0.0f;
            if (std::abs(velocity.z) < 0.05f) velocity.z = 0.0f;
        }
    }
}

void Player::handleMouse() {
    double xpos, ypos;
    glfwGetCursorPos(Window::handle, &xpos, &ypos);

    if (firstMouse) {
        lastX = xpos; lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos; lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    camera.yaw += xoffset;
    camera.pitch += yoffset;

    if (camera.pitch > 89.0f) camera.pitch = 89.0f;
    if (camera.pitch < -89.0f) camera.pitch = -89.0f;

    camera.updateVectors();
}

// Изменённая функция проверки коллизий: теперь pos указывает на ноги (координата Y)
bool Player::checkAABBCollision(glm::vec3 pos, World& world) {
    float pRadius = 0.3f;
    float pMinX = pos.x - pRadius;
    float pMaxX = pos.x + pRadius;

    // В приседе хитбокс игрока теперь становится 1.65f (вместо слишком низкого 1.5f)
    float pHeight = isSneaking ? 1.65f : 1.8f;
    float pMinY = pos.y;
    float pMaxY = pos.y + pHeight;

    float pMinZ = pos.z - pRadius;
    float pMaxZ = pos.z + pRadius;

    int startX = (int)std::floor(pMinX);
    int endX = (int)std::floor(pMaxX);
    int startY = (int)std::floor(pMinY);
    int endY = (int)std::floor(pMaxY);
    int startZ = (int)std::floor(pMinZ);
    int endZ = (int)std::floor(pMaxZ);

    for (int x = startX; x <= endX; ++x) {
        for (int y = startY; y <= endY; ++y) {
            for (int z = startZ; z <= endZ; ++z) {
                if (world.getBlock(x, y, z) != 0) {
                    float bMinX = (float)x;
                    float bMaxX = (float)x + 1.0f;
                    float bMinY = (float)y;
                    float bMaxY = (float)y + 1.0f;
                    float bMinZ = (float)z;
                    float bMaxZ = (float)z + 1.0f;

                    if (pMaxX > bMinX && pMinX < bMaxX &&
                        pMaxY > bMinY && pMinY < bMaxY &&
                        pMaxZ > bMinZ && pMinZ < bMaxZ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

// Проверка поддержки: есть ли хоть один твёрдый блок под горизонтальной проекцией ног
bool Player::isPositionSupported(glm::vec3 feetPos, World& world) {
    float pRadius = 0.3f;
    float pMinX = feetPos.x - pRadius;
    float pMaxX = feetPos.x + pRadius;
    float pMinZ = feetPos.z - pRadius;
    float pMaxZ = feetPos.z + pRadius;

    int startX = (int)std::floor(pMinX);
    int endX = (int)std::floor(pMaxX);
    int startZ = (int)std::floor(pMinZ);
    int endZ = (int)std::floor(pMaxZ);

    // Изменение: смещение -0.5f гарантирует, что мы всегда опрашиваем блоки строго ПОД ногами,
    // даже если физика держит игрока чуть выше поверхности земли из-за округлений (например, y = 25.02)
    int y = (int)std::floor(feetPos.y - 0.5f);

    for (int x = startX; x <= endX; ++x) {
        for (int z = startZ; z <= endZ; ++z) {
            if (world.getBlock(x, y, z) != BLOCK_AIR) {
                return true; // Персонажу есть на что опираться
            }
        }
    }
    return false; // Под игроком воздух
}

void Player::checkCollisions(World& world, glm::vec3 oldPosition) {
    (void)world;
    (void)oldPosition;
}