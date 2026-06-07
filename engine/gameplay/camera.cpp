#include "camera.h"
#include <cmath>
#include <cstdlib>

Camera::Camera(glm::vec3 startPosition) {
    position = startPosition;
    worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    yaw = -90.0f;
    pitch = 0.0f;
    front = glm::vec3(0.0f, 0.0f, -1.0f);

    // Инициализируем переменные крена камеры нулевыми значениями
    shakeRollOffset = 0.0f;
    shakeTimer = 0.0f;
    initialShakeRoll = 0.0f;
    deathRollOffset = 0.0f; // Инициализация

    updateVectors();
}

glm::mat4 Camera::getViewMatrix(bool relativeToZero) {
    // 1. Вычисляем стандартные векторы направления на основе Yaw и Pitch
    glm::vec3 tempFront;
    tempFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    tempFront.y = sin(glm::radians(pitch));
    tempFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    tempFront = glm::normalize(tempFront);

    glm::vec3 tempRight = glm::normalize(glm::cross(tempFront, worldUp));
    glm::vec3 tempUp = glm::normalize(glm::cross(tempRight, tempFront));

    // Суммируем стандартный крен от урона и крен от анимации смерти
    float totalRoll = shakeRollOffset + deathRollOffset;

    // 2. Если крен (Roll) активен, поворачиваем вектор "Up" вокруг оси взгляда (tempFront)
    if (std::abs(totalRoll) > 0.001f) {
        // Создаем матрицу вращения на угол totalRoll вокруг вектора tempFront
        glm::mat4 rollRotation = glm::rotate(glm::mat4(1.0f), glm::radians(totalRoll), tempFront);
        // Применяем вращение к вектору tempUp
        tempUp = glm::vec3(rollRotation * glm::vec4(tempUp, 0.0f));
    }

    // Выбираем позицию: (0,0,0) для относительного рендеринга или реальные координаты
    glm::vec3 eyePos = relativeToZero ? glm::vec3(0.0f) : position;

    // Возвращаем готовую матрицу вида с креном по 3-й оси
    return glm::lookAt(eyePos, eyePos + tempFront, tempUp);
}

void Camera::updateVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::update(float deltaTime) {
    // Если таймер тряски активен, плавно уменьшаем крен
    if (shakeTimer > 0.0f) {
        shakeTimer -= deltaTime;
        if (shakeTimer < 0.0f) {
            shakeTimer = 0.0f;
        }

        // Коэффициент линейного затухания (от 1.0 до 0.0 за полсекунды)
        float progress = shakeTimer / 0.5f;
        shakeRollOffset = initialShakeRoll * progress;
    }
    else {
        // Гарантируем полное выравнивание камеры после окончания таймера
        shakeRollOffset = 0.0f;
    }
}

void Camera::triggerShake(int damage) {
    // 1. Рассчитываем величину наклона в градусах
    float magnitude = 10.0f; // Базовый наклон на 10 градусов для 1 сердца урона

    if (damage >= 7) {
        magnitude = 45.0f; // При уроне больше 7 сердец наклон составит 45 градусов
    }
    else if (damage > 1) {
        // Линейная интерполяция угла наклона между 10 и 45 градусами
        float interpolationFactor = static_cast<float>(damage - 1) / 6.0f;
        magnitude = 10.0f + interpolationFactor * 35.0f;
    }

    initialShakeRoll = -magnitude;

    // Резко устанавливаем начальный наклон и запускаем полусекундный таймер плавного возврата
    shakeRollOffset = initialShakeRoll;
    shakeTimer = 0.5f;
}