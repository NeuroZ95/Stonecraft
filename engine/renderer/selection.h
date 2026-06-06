#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

class BlockSelection {
public:
    BlockSelection();
    ~BlockSelection();

    void init();
    void render(glm::ivec3 blockPos, glm::vec3 cameraPos, glm::mat4 view, glm::mat4 projection, unsigned int shaderProgram);

private:
    unsigned int VAO, VBO;
};