#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;

    // --- МОДУЛЬ ТРЯСКИ КАМЕРЫ (КРЕН / ROLL ПО 3-Й ОСИ) ---
    float shakeRollOffset;     // Текущий угол наклона камеры влево/вправо (в градусах)
    float shakeTimer;          // Оставшееся время возврата в секундах (затухает от 0.5 до 0)
    float initialShakeRoll;    // Начальный резкий наклон (+/- градусов в случайную сторону)
    float deathRollOffset;     // Текущий угол наклона при смерти (в градусах)

    Camera(glm::vec3 startPosition);

    // Получение матрицы вида (relativeToZero используется для устранения дрожания при рендеринге)
    glm::mat4 getViewMatrix(bool relativeToZero = false);
    void updateVectors();

    // Метод плавного линейного возвращения крена камеры в исходное состояние
    void update(float deltaTime);

    // Метод резкого наклона камеры при получении урона
    void triggerShake(int damage);
};