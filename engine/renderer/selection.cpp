#include "selection.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

BlockSelection::BlockSelection() : VAO(0), VBO(0) {}

BlockSelection::~BlockSelection() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
}

void BlockSelection::init() {
    float vertices[] = {
        0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,  0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 1.0f,  0.0f, 1.0f, 0.0f,

        0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void BlockSelection::render(glm::ivec3 blockPos, glm::vec3 cameraPos, glm::mat4 view, glm::mat4 projection, unsigned int shaderProgram) {
    glUseProgram(shaderProgram);

    // Вычисляем координату рамки выделения относительно камеры на CPU для исключения потери точности
    glm::vec3 relativeBlockPos = glm::vec3(blockPos) - cameraPos;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), relativeBlockPos - glm::vec3(0.001f));
    model = glm::scale(model, glm::vec3(1.002f));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glEnable(GL_COLOR_LOGIC_OP);
    glLogicOp(GL_INVERT);
    glLineWidth(2.0f);

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    glDisable(GL_COLOR_LOGIC_OP);
}