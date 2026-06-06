#include "world.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <engine/gameplay/menu.h>
#include <engine/gameplay/settings.h>
#include "../../tools/logger/logger.h"

extern Menu* g_MenuInstance;
namespace fs = std::filesystem;

World::World() : noiseGen(1337), isMultiplayer(false), heightmapTexID(0), voxels3DTexID(0), heightmapCenter(0.0f, 0.0f), activeSaveName(""), worldSeed(1337), worldType(0) {}

World::~World() {
    for (auto& [key, chunk] : chunks) {
        delete chunk;
    }
    chunks.clear();
    if (heightmapTexID != 0) {
        glDeleteTextures(1, &heightmapTexID);
    }
    if (voxels3DTexID != 0) {
        glDeleteTextures(1, &voxels3DTexID);
    }
}

void World::update(float deltaTime) {
    (void)deltaTime;
    for (auto& [key, chunk] : chunks) {
        chunk->update();
    }
}

void World::updateMeshes() {
    for (auto& [key, chunk] : chunks) {
        chunk->buildMesh(BLOCK_TYPES_COUNT);
    }
}

void World::checkHeightmapShift(glm::vec3 playerPos) {
    int currentCx = static_cast<int>(std::floor(playerPos.x / 16.0f)) * 16;
    int currentCz = static_cast<int>(std::floor(playerPos.z / 16.0f)) * 16;

    if (heightmapTexID == 0 || voxels3DTexID == 0 ||
        std::abs(currentCx - static_cast<int>(heightmapCenter.x)) >= 32 ||
        std::abs(currentCz - static_cast<int>(heightmapCenter.y)) >= 32) {
        updateHeightmap(playerPos);
    }
}

void World::updateHeightmap(glm::vec3 playerPos) {
    int centerX = static_cast<int>(std::floor(playerPos.x / 16.0f)) * 16;
    int centerZ = static_cast<int>(std::floor(playerPos.z / 16.0f)) * 16;

    int startX = centerX - 256;
    int startZ = centerZ - 256;

    heightmapCenter = glm::vec2(centerX, centerZ);

    if (heightmapTexID == 0) {
        glGenTextures(1, &heightmapTexID);
        glBindTexture(GL_TEXTURE_2D, heightmapTexID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        heightmapData.resize(512 * 512, 0);
    }

    if (voxels3DTexID == 0) {
        glGenTextures(1, &voxels3DTexID);
        glBindTexture(GL_TEXTURE_3D, voxels3DTexID);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        voxels3DData.resize(512 * 64 * 512, 0);
    }

    for (int z = 0; z < 512; ++z) {
        for (int x = 0; x < 512; ++x) {
            int worldX = startX + x;
            int worldZ = startZ + z;

            int h = 0;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                uint8_t block = getBlock(worldX, y, worldZ);
                voxels3DData[x + y * 512 + z * 512 * 64] = block;

                if (!isBlockTransparent(block) && h == 0) {
                    h = y + 1;
                }
            }
            heightmapData[z * 512 + x] = static_cast<uint8_t>(h);
        }
    }

    glBindTexture(GL_TEXTURE_2D, heightmapTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, heightmapData.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindTexture(GL_TEXTURE_3D, voxels3DTexID);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, 512, 64, 512, 0, GL_RED, GL_UNSIGNED_BYTE, voxels3DData.data());
    glBindTexture(GL_TEXTURE_3D, 0);
}

void World::updateHeightmapBlock(int worldX, int worldZ) {
    if (heightmapTexID == 0 || voxels3DTexID == 0) return;

    int startX = static_cast<int>(heightmapCenter.x) - 256;
    int startZ = static_cast<int>(heightmapCenter.y) - 256;

    int localX = worldX - startX;
    int localZ = worldZ - startZ;

    if (localX >= 0 && localX < 512 && localZ >= 0 && localZ < 512) {
        int h = 0;
        std::vector<uint8_t> column(64);

        for (int y = 0; y < 64; ++y) {
            uint8_t block = getBlock(worldX, y, worldZ);
            column[y] = block;
            voxels3DData[localX + y * 512 + localZ * 512 * 64] = block;

            if (!isBlockTransparent(block)) {
                h = y + 1;
            }
        }
        heightmapData[localZ * 512 + localX] = static_cast<uint8_t>(h);

        glBindTexture(GL_TEXTURE_3D, voxels3DTexID);
        glTexSubImage3D(GL_TEXTURE_3D, 0, localX, 0, localZ, 1, 64, 1, GL_RED, GL_UNSIGNED_BYTE, column.data());
        glBindTexture(GL_TEXTURE_3D, 0);

        glBindTexture(GL_TEXTURE_2D, heightmapTexID);
        uint8_t val = static_cast<uint8_t>(h);
        glTexSubImage2D(GL_TEXTURE_2D, 0, localX, localZ, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &val);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void World::render(glm::vec3 cameraPos, unsigned int shaderProgram) {
    checkHeightmapShift(cameraPos);

    int playerCx = floorDiv(static_cast<int>(std::floor(cameraPos.x)), CHUNK_SIZE);
    int playerCz = floorDiv(static_cast<int>(std::floor(cameraPos.z)), CHUNK_SIZE);
    int viewDistance = g_Settings.renderDistance;

    unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");

    for (int dx = -viewDistance; dx <= viewDistance; ++dx) {
        for (int dz = -viewDistance; dz <= viewDistance; ++dz) {
            int cx = playerCx + dx;
            int cz = playerCz + dz;

            if (cx < -MAX_WORLD_CHUNKS_HALF || cz < -MAX_WORLD_CHUNKS_HALF ||
                cx >= MAX_WORLD_CHUNKS_HALF || cz >= MAX_WORLD_CHUNKS_HALF) {
                continue;
            }

            uint64_t key = getChunkKey(cx, cz);
            auto it = chunks.find(key);

            Chunk* chunk = nullptr;
            if (it == chunks.end()) {
                if (isMultiplayer) {
                    continue;
                }

                chunk = new Chunk(cx, 0, cz);

                std::string chunkPath = "saves/" + activeSaveName + "/chunks/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
                if (!activeSaveName.empty() && fs::exists(chunkPath)) {
                    chunk->loadFromFile(chunkPath);
                }
                else {
                    chunk->generate(noiseGen, worldSeed, worldType);

                    for (const auto& [pos, mType] : modifiedBlocks) {
                        int ccx = floorDiv(pos.x, CHUNK_SIZE);
                        int ccz = floorDiv(pos.z, CHUNK_SIZE);
                        if (ccx == cx && ccz == cz) {
                            int localX = floorMod(pos.x, CHUNK_SIZE);
                            int localZ = floorMod(pos.z, CHUNK_SIZE);
                            chunk->setBlock(localX, pos.y, localZ, mType);
                        }
                    }

                    if (!activeSaveName.empty()) {
                        fs::create_directories("saves/" + activeSaveName + "/chunks");
                        chunk->saveToFile(chunkPath);
                    }
                }

                chunk->buildMesh(BLOCK_TYPES_COUNT);
                chunks[key] = chunk;
            }
            else {
                chunk = it->second;
            }

            glm::vec3 chunkWorldPos(cx * CHUNK_SIZE, 0.0f, cz * CHUNK_SIZE);
            glm::vec3 relativePos = chunkWorldPos - cameraPos;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), relativePos);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            chunk->draw();
        }
    }
}

void World::receiveChunk(int cx, int cz, const uint8_t* blocks) {
    uint64_t key = getChunkKey(cx, cz);
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        delete it->second;
        chunks.erase(it);
    }

    Chunk* chunk = new Chunk(cx, 0, cz);
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                chunk->setBlock(x, y, z, blocks[(x * CHUNK_HEIGHT + y) * CHUNK_SIZE + z]);
            }
        }
    }

    // Применяем все модификации блоков для данного чанка, чтобы клиентская физика и меш были в гармонии
    for (const auto& [pos, blockType] : modifiedBlocks) {
        int ccx = floorDiv(pos.x, CHUNK_SIZE);
        int ccz = floorDiv(pos.z, CHUNK_SIZE);
        if (ccx == cx && ccz == cz) {
            int localX = floorMod(pos.x, CHUNK_SIZE);
            int localZ = floorMod(pos.z, CHUNK_SIZE);
            chunk->setBlock(localX, pos.y, localZ, blockType);
        }
    }

    chunk->buildMesh(BLOCK_TYPES_COUNT);
    chunks[key] = chunk;

    if (voxels3DTexID != 0) {
        int startX = static_cast<int>(heightmapCenter.x) - 256;
        int startZ = static_cast<int>(heightmapCenter.y) - 256;

        int chunkStartX = cx * 16;
        int chunkStartZ = cz * 16;

        if (chunkStartX + 16 > startX && chunkStartX < startX + 512 &&
            chunkStartZ + 16 > startZ && chunkStartZ < startZ + 512) {

            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    int worldX = chunkStartX + x;
                    int worldZ = chunkStartZ + z;

                    int localX = worldX - startX;
                    int localZ = worldZ - startZ;

                    if (localX >= 0 && localX < 512 && localZ >= 0 && localZ < 512) {
                        int h = 0;
                        for (int y = 0; y < 64; ++y) {
                            uint8_t block = chunk->getBlock(x, y, z); // Читаем обновленный блок
                            voxels3DData[localX + y * 512 + localZ * 512 * 64] = block;
                            if (!isBlockTransparent(block)) {
                                h = y + 1;
                            }
                        }
                        heightmapData[localZ * 512 + localX] = static_cast<uint8_t>(h);
                    }
                }
            }

            glBindTexture(GL_TEXTURE_2D, heightmapTexID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, heightmapData.data());
            glBindTexture(GL_TEXTURE_2D, 0);

            glBindTexture(GL_TEXTURE_3D, voxels3DTexID);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, 512, 64, 512, 0, GL_RED, GL_UNSIGNED_BYTE, voxels3DData.data());
            glBindTexture(GL_TEXTURE_3D, 0);
        }
    }
}

block_t World::getBlock(int x, int y, int z) {
    if (y < 0 || y >= CHUNK_HEIGHT) {
        return BLOCK_AIR;
    }

    SimpleIVec3 sPos = { x, y, z };
    auto mIt = modifiedBlocks.find(sPos);
    if (mIt != modifiedBlocks.end()) {
        return mIt->second;
    }

    int cx = floorDiv(x, CHUNK_SIZE);
    int cz = floorDiv(z, CHUNK_SIZE);

    int localX = floorMod(x, CHUNK_SIZE);
    int localZ = floorMod(z, CHUNK_SIZE);

    if (cx < -MAX_WORLD_CHUNKS_HALF || cz < -MAX_WORLD_CHUNKS_HALF ||
        cx >= MAX_WORLD_CHUNKS_HALF || cz >= MAX_WORLD_CHUNKS_HALF) {
        return BLOCK_AIR;
    }

    uint64_t key = getChunkKey(cx, cz);
    auto it = chunks.find(key);
    if (it == chunks.end()) {
        return BLOCK_AIR;
    }

    return it->second->getBlock(localX, y, localZ);
}

void World::setBlock(glm::ivec3 pos, block_t type) {
    if (pos.y < 0 || pos.y >= CHUNK_HEIGHT) {
        return;
    }

    SimpleIVec3 sPos = { pos.x, pos.y, pos.z };
    modifiedBlocks[sPos] = type;

    int cx = floorDiv(pos.x, CHUNK_SIZE);
    int cz = floorDiv(pos.z, CHUNK_SIZE);

    int localX = floorMod(pos.x, CHUNK_SIZE);
    int localZ = floorMod(pos.z, CHUNK_SIZE);

    if (cx < -MAX_WORLD_CHUNKS_HALF || cz < -MAX_WORLD_CHUNKS_HALF ||
        cx >= MAX_WORLD_CHUNKS_HALF || cz >= MAX_WORLD_CHUNKS_HALF) {
        return;
    }

    uint64_t key = getChunkKey(cx, cz);
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        Chunk* chunk = it->second;
        if (chunk->getBlock(localX, pos.y, localZ) != type) {
            chunk->setBlock(localX, pos.y, localZ, type);
            chunk->buildMesh(BLOCK_TYPES_COUNT);

            // Сохраняем измененный чанк сразу же, гарантируя персистентность
            if (!isMultiplayer && !activeSaveName.empty()) {
                std::string chunkPath = "saves/" + activeSaveName + "/chunks/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
                chunk->saveToFile(chunkPath);
            }

            if (localX == 0 && cx > -MAX_WORLD_CHUNKS_HALF) {
                auto nIt = chunks.find(getChunkKey(cx - 1, cz));
                if (nIt != chunks.end()) nIt->second->buildMesh(BLOCK_TYPES_COUNT);
            }
            if (localX == CHUNK_SIZE - 1 && cx < MAX_WORLD_CHUNKS_HALF - 1) {
                auto nIt = chunks.find(getChunkKey(cx + 1, cz));
                if (nIt != chunks.end()) nIt->second->buildMesh(BLOCK_TYPES_COUNT);
            }
            if (localZ == 0 && cz > -MAX_WORLD_CHUNKS_HALF) {
                auto nIt = chunks.find(getChunkKey(cx, cz - 1));
                if (nIt != chunks.end()) nIt->second->buildMesh(BLOCK_TYPES_COUNT);
            }
            if (localZ == CHUNK_SIZE - 1 && cz < MAX_WORLD_CHUNKS_HALF - 1) {
                auto nIt = chunks.find(getChunkKey(cx, cz + 1));
                if (nIt != chunks.end()) nIt->second->buildMesh(BLOCK_TYPES_COUNT);
            }
        }
    }

    updateHeightmapBlock(pos.x, pos.z);
}

bool World::loadWorldData(const std::string& saveName, glm::vec3& outPlayerPos, float& outYaw, float& outPitch, int& outSelectedSlot, uint8_t* outHotbar) {
    std::string path = "saves/" + saveName + "/world.dat";
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.read(reinterpret_cast<char*>(&worldSeed), sizeof(worldSeed));
    file.read(reinterpret_cast<char*>(&worldType), sizeof(worldType));
    file.read(reinterpret_cast<char*>(&outPlayerPos), sizeof(outPlayerPos));
    file.read(reinterpret_cast<char*>(&outYaw), sizeof(outYaw));
    file.read(reinterpret_cast<char*>(&outPitch), sizeof(outPitch));
    file.read(reinterpret_cast<char*>(&outSelectedSlot), sizeof(outSelectedSlot));
    file.read(reinterpret_cast<char*>(outHotbar), 9);

    modifiedBlocks.clear();
    size_t mapSize = 0;
    if (file.read(reinterpret_cast<char*>(&mapSize), sizeof(mapSize))) {
        for (size_t i = 0; i < mapSize; ++i) {
            SimpleIVec3 pos;
            block_t type;
            file.read(reinterpret_cast<char*>(&pos), sizeof(pos));
            file.read(reinterpret_cast<char*>(&type), sizeof(type));
            modifiedBlocks[pos] = type;
        }
    }
    file.close();

    noiseGen = PerlinNoise(worldSeed);
    return true;
}

void World::saveWorldData(const std::string& saveName, const glm::vec3& playerPos, float yaw, float pitch, int selectedSlot, const uint8_t* hotbar) {
    if (saveName.empty() || isMultiplayer) return;

    fs::create_directories("saves/" + saveName + "/chunks");

    std::string path = "saves/" + saveName + "/world.dat";
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;

    file.write(reinterpret_cast<const char*>(&worldSeed), sizeof(worldSeed));
    file.write(reinterpret_cast<const char*>(&worldType), sizeof(worldType));
    file.write(reinterpret_cast<const char*>(&playerPos), sizeof(playerPos));
    file.write(reinterpret_cast<const char*>(&yaw), sizeof(yaw));
    file.write(reinterpret_cast<const char*>(&pitch), sizeof(pitch));
    file.write(reinterpret_cast<const char*>(&selectedSlot), sizeof(selectedSlot));
    file.write(reinterpret_cast<const char*>(hotbar), 9);

    size_t mapSize = modifiedBlocks.size();
    file.write(reinterpret_cast<const char*>(&mapSize), sizeof(mapSize));
    for (const auto& [pos, type] : modifiedBlocks) {
        file.write(reinterpret_cast<const char*>(&pos), sizeof(pos));
        file.write(reinterpret_cast<const char*>(&type), sizeof(type));
    }
    file.close();

    // Запись всех прогруженных чанков из памяти на накопитель
    for (const auto& [key, chunk] : chunks) {
        int cx = chunk->getChunkX();
        int cz = chunk->getChunkZ();
        std::string chunkPath = "saves/" + saveName + "/chunks/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
        chunk->saveToFile(chunkPath);
    }
}

RaycastResult World::raycast(glm::vec3 origin, glm::vec3 direction, float maxDist) {
    RaycastResult result = { false, glm::ivec3(0), glm::ivec3(0), -1 };
    glm::vec3 currentPos = origin;
    float stepSize = 0.01f;
    glm::vec3 step = direction * stepSize;
    float traveled = 0.0f;

    glm::ivec3 lastPos(
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z))
    );

    while (traveled < maxDist) {
        currentPos += step;
        traveled += stepSize;

        glm::ivec3 bPos(
            static_cast<int>(std::floor(currentPos.x)),
            static_cast<int>(std::floor(currentPos.y)),
            static_cast<int>(std::floor(currentPos.z))
        );

        if (getBlock(bPos.x, bPos.y, bPos.z) != BLOCK_AIR) {
            result.hit = true;
            result.blockPos = bPos;
            result.adjacentPos = lastPos;

            glm::ivec3 diff = lastPos - bPos;
            if (diff.z == 1) result.face = 0;
            else if (diff.z == -1) result.face = 1;
            else if (diff.x == -1) result.face = 2;
            else if (diff.x == 1) result.face = 3;
            else if (diff.y == 1) result.face = 4;
            else if (diff.y == -1) result.face = 5;

            return result;
        }
        lastPos = bPos;
    }
    return result;
}

void World::regenerate(unsigned int seed, float spawnX, float spawnZ, bool pregenerate) {
    for (auto& [key, chunk] : chunks) {
        delete chunk;
    }
    chunks.clear();
    modifiedBlocks.clear();

    worldSeed = seed;
    noiseGen = PerlinNoise(seed);

    if (!pregenerate) return;

    int spawnCx = floorDiv(static_cast<int>(std::floor(spawnX)), CHUNK_SIZE);
    int spawnCz = floorDiv(static_cast<int>(std::floor(spawnZ)), CHUNK_SIZE);

    const int pregenerateDistance = 14;
    int totalChunks = (2 * pregenerateDistance + 1) * (2 * pregenerateDistance + 1);
    int generatedCount = 0;

    for (int dx = -pregenerateDistance; dx <= pregenerateDistance; ++dx) {
        for (int dz = -pregenerateDistance; dz <= pregenerateDistance; ++dz) {
            int cx = spawnCx + dx;
            int cz = spawnCz + dz;
            if (cx >= -MAX_WORLD_CHUNKS_HALF && cz >= -MAX_WORLD_CHUNKS_HALF &&
                cx < MAX_WORLD_CHUNKS_HALF && cz < MAX_WORLD_CHUNKS_HALF) {

                Chunk* chunk = new Chunk(cx, 0, cz);
                std::string chunkPath = "saves/" + activeSaveName + "/chunks/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";

                if (!activeSaveName.empty() && fs::exists(chunkPath)) {
                    chunk->loadFromFile(chunkPath);
                }
                else {
                    chunk->generate(noiseGen, worldSeed, worldType);
                    if (!activeSaveName.empty()) {
                        fs::create_directories("saves/" + activeSaveName + "/chunks");
                        chunk->saveToFile(chunkPath);
                    }
                }

                chunk->buildMesh(BLOCK_TYPES_COUNT);
                chunks[getChunkKey(cx, cz)] = chunk;
            }

            generatedCount++;
            if (generatedCount % 5 == 0 || generatedCount == totalChunks) {
                if (g_MenuInstance) {
                    float progress = (float)generatedCount / totalChunks;

                    // Детализация шагов генерации
                    std::string stageText = "Generating World...";
                    if (progress < 0.15f) {
                        stageText = "Initializing continental tectonic plates & seeds (" + std::to_string(int(progress * 100)) + "%)...";
                    }
                    else if (progress < 0.40f) {
                        stageText = "Sculpting mountains & depth valleys (" + std::to_string(int(progress * 100)) + "%)...";
                    }
                    else if (progress < 0.65f) {
                        stageText = "Carving underground caves & rock layers (" + std::to_string(int(progress * 100)) + "%)...";
                    }
                    else if (progress < 0.85f) {
                        stageText = "Spreading organic soil & grass covers (" + std::to_string(int(progress * 100)) + "%)...";
                    }
                    else {
                        stageText = "Assembling chunk structures & spawn zone (" + std::to_string(int(progress * 100)) + "%)...";
                    }

                    g_MenuInstance->renderLoadingScreen(progress, false, stageText, true);
                }
            }
        }
    }
}