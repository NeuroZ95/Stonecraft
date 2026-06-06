#include "crosshair.h"
#include <GL/glew.h>

Crosshair::Crosshair() {
    hudShader = new Shader("shaders/hud_vertex.glsl", "shaders/hud_fragment.glsl");
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
}

Crosshair::~Crosshair() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    delete hudShader;
}

void Crosshair::render(float screenWidth, float screenHeight) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

    (void)screenWidth;
    (void)screenHeight;

    float halfLengthX = 18.0f / 1920.0f;
    float halfLengthY = 18.0f / 1080.0f;
    float halfThickX = 2.0f / 1920.0f;
    float halfThickY = 2.0f / 1080.0f;

    float vertices[] = {
        // 1. Горизонтальная сплошная линия (рисует весь центр)
        -halfLengthX, -halfThickY,
         halfLengthX, -halfThickY,
         halfLengthX,  halfThickY,
         halfLengthX,  halfThickY,
        -halfLengthX,  halfThickY,
        -halfLengthX, -halfThickY,

        // 2. Верхний вертикальный ус (начинается ОТ границы центра)
        -halfThickX,  halfThickY,
         halfThickX,  halfThickY,
         halfThickX,  halfLengthY,
         halfThickX,  halfLengthY,
        -halfThickX,  halfLengthY,
        -halfThickX,  halfThickY,

        // 3. Нижний вертикальный ус (заканчивается ДО границы центра)
        -halfThickX, -halfLengthY,
         halfThickX, -halfLengthY,
         halfThickX, -halfThickY,
         halfThickX, -halfThickY,
        -halfThickX, -halfThickY,
        -halfThickX, -halfLengthY
    };

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    hudShader->use();
    glDrawArrays(GL_TRIANGLES, 0, 18);
    glBindVertexArray(0);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Исправление утечки состояния рендеринга OpenGL
}