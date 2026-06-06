#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shader.h"
#include "texture.h"
#include "../gameplay/player.h"

class Hud {
public:
    Hud();
    ~Hud();

    bool init();
    void render(const Player& player, const Texture& blockAtlas, float screenWidth, float screenHeight);

private:
    Shader* hudShader;
    Texture* slotDeactTex;
    Texture* slotActTex;
    unsigned int quadVAO, quadVBO;

    void drawQuad(float x, float y, float width, float height, glm::vec4 color, GLuint textureID, glm::vec4 texCoords = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
};