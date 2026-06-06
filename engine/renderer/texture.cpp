#include "texture.h"
#include <GL/glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../libs/stb_image.h"
#include "../../tools/logger/logger.h"
#include <vector>

Texture::Texture(const std::string& blocksFolderPath, const std::vector<std::string>& blockFileNames) {
    int atlasWidth = 96;
    int blockHeight = 16;
    int channels = 4;
    int totalBlocks = blockFileNames.size();
    int atlasHeight = totalBlocks * blockHeight;

    std::vector<unsigned char> atlasPixels(atlasWidth * atlasHeight * channels, 0);

    for (int i = 0; i < totalBlocks; ++i) {
        if (blockFileNames[i].empty()) {
            continue;
        }

        std::string fullPath = blocksFolderPath + "/" + blockFileNames[i];
        int width, height, comp;
        unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &comp, 4);

        if (!data) {
            Logger::log(Logger::Level::ERROR, "Failed to load texture: " + fullPath);
            continue;
        }

        int rowOffset = i * blockHeight;

        if (width == 16 && height == 16) {
            for (int face = 0; face < 6; ++face) {
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        int srcIdx = (y * 16 + x) * 4;
                        int destX = face * 16 + x;
                        int destY = rowOffset + y;
                        int destIdx = (destY * atlasWidth + destX) * 4;

                        atlasPixels[destIdx + 0] = data[srcIdx + 0];
                        atlasPixels[destIdx + 1] = data[srcIdx + 1];
                        atlasPixels[destIdx + 2] = data[srcIdx + 2];
                        atlasPixels[destIdx + 3] = data[srcIdx + 3];
                    }
                }
            }
        } 
        else if (width == 96 && height == 16) {
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 96; ++x) {
                    int srcIdx = (y * 96 + x) * 4;
                    int destY = rowOffset + y;
                    int destIdx = (destY * atlasWidth + x) * 4;

                    atlasPixels[destIdx + 0] = data[srcIdx + 0];
                    atlasPixels[destIdx + 1] = data[srcIdx + 1];
                    atlasPixels[destIdx + 2] = data[srcIdx + 2];
                    atlasPixels[destIdx + 3] = data[srcIdx + 3];
                }
            }
        } 
        else {
            Logger::log(Logger::Level::WARNING, "Invalid texture dimensions for: " + blockFileNames[i]);
        }

        stbi_image_free(data);
    }

    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(const std::string& filePath) {
    int width, height, comp;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &comp, 4);

    if (!data) {
        Logger::log(Logger::Level::ERROR, "Failed to load single texture: " + filePath);
        // Запасной полупрозрачный пиксель
        unsigned char fallback[4] = { 128, 128, 128, 128 };
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    }
    else {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::~Texture() {
    glDeleteTextures(1, &id);
}

void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, id);
}

unsigned int Texture::getID() const {
    return id;
}