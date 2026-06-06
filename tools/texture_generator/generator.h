#pragma once
#include <string>

class TextureGenerator {
public:
    static void initTexturesFolder();
private:
    static void generateSingleTemplate(const std::string& path);
    static void generateMultiTemplate(const std::string& path);
    static void drawArrow(unsigned char* pixels, int width, int startX, int startY, int direction, unsigned char r, unsigned char g, unsigned char b);
};