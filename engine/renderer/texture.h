#pragma once
#include <string>
#include <vector>

class Texture {
public:
    Texture(const std::string& blocksFolderPath, const std::vector<std::string>& blockFileNames);
    Texture(const std::string& filePath); // Новый конструктор для одиночных текстур
    ~Texture();
    void bind(unsigned int slot = 0) const;
    unsigned int getID() const;

private:
    unsigned int id;
};