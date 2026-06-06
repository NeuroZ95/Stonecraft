#pragma once

#include <map>
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shader.h"

struct Character {
    unsigned int TextureID; // ID текстуры глифа
    glm::ivec2   Size;      // Размеры глифа
    glm::ivec2   Bearing;   // Смещение от базовой линии до левого/верхнего угла
    unsigned int Advance;   // Смещение до следующего глифа
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    bool init(const std::string& fontPath, unsigned int fontSize);
    void renderText(Shader& shader, const std::string& text, float x, float y, float scale, glm::vec3 color);
    float getTextWidth(const std::string& text, float scale);

private:
    std::map<char, Character> characters;
    unsigned int VAO, VBO;
};