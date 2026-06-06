#pragma once
#include "shader.h"

class Crosshair {
private:
    unsigned int VAO, VBO;
    Shader* hudShader;

public:
    Crosshair();
    ~Crosshair();
    void render(float screenWidth, float screenHeight);
};