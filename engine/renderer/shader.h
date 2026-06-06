#pragma once
#include <string>
#include <GL/glew.h>

class Shader {
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();

    void use();

private:
    void checkCompileErrors(unsigned int shader, std::string type);
};