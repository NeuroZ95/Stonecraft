#pragma once
#include <vector>
#include <unordered_map>
#include <map>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include "chunk.h"
#include "perlin.h"

struct RaycastResult {
    bool hit;
    glm::ivec3 blockPos;
    glm::ivec3 adjacentPos;
    int face;
};

struct SimpleIVec3 {
    int x;
    int y;
    int z;
};

struct SimpleIVec3Less {
    bool operator()(const SimpleIVec3& a, const SimpleIVec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

class World {
public:
    World();
    ~World();

    void update(float deltaTime);
    void updateMeshes();
    void render(glm::vec3 cameraPos, unsigned int shaderProgram);
    block_t getBlock(int x, int y, int z);
    void setBlock(glm::ivec3 pos, block_t type);

    void regenerate(unsigned int seed, float spawnX, float spawnZ, bool pregenerate = true);
    void receiveChunk(int cx, int cz, const uint8_t* blocks);

    RaycastResult raycast(glm::vec3 origin, glm::vec3 direction, float maxDist);

    // Функции сериализации сохранения мира на диск
    bool loadWorldData(const std::string& saveName, glm::vec3& outPlayerPos, float& outYaw, float& outPitch, int& outSelectedSlot, uint8_t* outHotbar);
    void saveWorldData(const std::string& saveName, const glm::vec3& playerPos, float yaw, float pitch, int selectedSlot, const uint8_t* hotbar);

    inline const std::unordered_map<uint64_t, Chunk*>& getChunksMap() const { return chunks; }

    bool isMultiplayer;
    std::string activeSaveName; // Имя текущего сохранения
    unsigned int worldSeed;     // Сид текущего мира
    int worldType;              // 0 - Default, 1 - Flat

    static constexpr int MAX_WORLD_WIDTH_BLOCKS = 100000;
    static constexpr int MAX_WORLD_CHUNKS = MAX_WORLD_WIDTH_BLOCKS / CHUNK_SIZE;
    static constexpr int MAX_WORLD_CHUNKS_HALF = MAX_WORLD_CHUNKS / 2;

    static inline uint64_t getChunkKey(int cx, int cz) {
        return (static_cast<uint64_t>(cx) << 32) | (static_cast<uint32_t>(cz) & 0xFFFFFFFF);
    }

    static inline int floorDiv(int a, int b) {
        return a >= 0 ? a / b : (a - b + 1) / b;
    }

    static inline int floorMod(int a, int b) {
        int r = a % b;
        return r >= 0 ? r : r + b;
    }

    std::map<SimpleIVec3, block_t, SimpleIVec3Less> modifiedBlocks;

    // Структуры для 2D карты высот теней
    unsigned int heightmapTexID;
    std::vector<uint8_t> heightmapData;
    glm::vec2 heightmapCenter;

    // Структуры для 3D текстуры вокселей (Честный RTX)
    unsigned int voxels3DTexID;
    std::vector<uint8_t> voxels3DData;

    void updateHeightmap(glm::vec3 playerPos);
    void updateHeightmapBlock(int worldX, int worldZ);
    void checkHeightmapShift(glm::vec3 playerPos);

private:
    std::unordered_map<uint64_t, Chunk*> chunks;
    PerlinNoise noiseGen;
};