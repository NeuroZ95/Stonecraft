#include "generator.h"
#include <filesystem>
#include <fstream>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../libs/stb_image_write.h"

namespace fs = std::filesystem;

void TextureGenerator::initTexturesFolder() {
    // Кроссплатформенное создание директории (работает и на Linux, и на Windows)
    fs::create_directory("textures");

    std::string singlePath = "textures/template_single.png";
    std::string multiPath = "textures/template_multi.png";
    std::string testPath = "textures/test.png";

    if (!fs::exists(singlePath)) {
        generateSingleTemplate(singlePath);
    }
    if (!fs::exists(multiPath)) {
        generateMultiTemplate(multiPath);
    }
    if (!fs::exists(testPath)) {
        std::ifstream src(multiPath, std::ios::binary);
        std::ofstream dst(testPath, std::ios::binary);
        dst << src.rdbuf();
    }
}

void TextureGenerator::generateSingleTemplate(const std::string& path) {
    const int width = 16;
    const int height = 16;
    const int channels = 4;
    std::vector<unsigned char> pixels(width * height * channels);

    for (int i = 0; i < width * height; ++i) {
        pixels[i * channels + 0] = 255; 
        pixels[i * channels + 1] = 0;   
        pixels[i * channels + 2] = 0;   
        pixels[i * channels + 3] = 255; 
    }

    stbi_write_png(path.c_str(), width, height, channels, pixels.data(), width * channels);
}

void TextureGenerator::drawArrow(unsigned char* pixels, int width, int startX, int startY, int direction, unsigned char r, unsigned char g, unsigned char b) {
    int centerX = startX + 8;
    int centerY = startY + 8;

    for (int y = startY; y < startY + 16; ++y) {
        for (int x = startX; x < startX + 16; ++x) {
            int idx = (y * width + x) * 4;
            int dx = x - centerX;
            int dy = y - centerY;

            bool isArrow = false;
            if (direction == 0) {
                if (dy <= 4 && dy >= -4 && dx == 0) isArrow = true;
                if (dy >= -4 && dy <= -1 && std::abs(dx) == std::abs(dy + 4)) isArrow = true;
            } else if (direction == 1) {
                if (dy <= 4 && dy >= -4 && dx == 0) isArrow = true;
                if (dy <= 4 && dy >= 1 && std::abs(dx) == std::abs(dy - 4)) isArrow = true;
            } else if (direction == 2) {
                if (dx <= 4 && dx >= -4 && dy == 0) isArrow = true;
                if (dx >= -4 && dx <= -1 && std::abs(dy) == std::abs(dx + 4)) isArrow = true;
            } else if (direction == 3) {
                if (dx <= 4 && dx >= -4 && dy == 0) isArrow = true;
                if (dx <= 4 && dx >= 1 && std::abs(dy) == std::abs(dx - 4)) isArrow = true;
            } else if (direction == 4) {
                if (std::abs(dx) <= 1 && std::abs(dy) <= 1) isArrow = true;
            } else if (direction == 5) {
                int dist = dx*dx + dy*dy;
                if (dist >= 4 && dist <= 9) isArrow = true;
            }

            if (isArrow) {
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = 255; 
            }
        }
    }
}

void TextureGenerator::generateMultiTemplate(const std::string& path) {
    const int width = 96; 
    const int height = 16;
    const int channels = 4;
    std::vector<unsigned char> pixels(width * height * channels, 255);

    unsigned char colors[6][3] = {
        {200, 50, 50},   
        {50, 200, 50},   
        {50, 50, 200},   
        {200, 200, 50},  
        {200, 50, 200},  
        {50, 200, 200}   
    };

    int directions[6] = {0, 1, 2, 3, 4, 5};

    for (int face = 0; face < 6; ++face) {
        int startX = face * 16;
        for (int y = 0; y < 16; ++y) {
            for (int x = startX; x < startX + 16; ++x) {
                int idx = (y * width + x) * channels;
                pixels[idx + 0] = colors[face][0];
                pixels[idx + 1] = colors[face][1];
                pixels[idx + 2] = colors[face][2];
                pixels[idx + 3] = 255; 
            }
        }
        drawArrow(pixels.data(), width, startX, 0, directions[face], 255, 255, 255);
    }

    stbi_write_png(path.c_str(), width, height, channels, pixels.data(), width * channels);
}