#pragma once
#include <vector>
#include <string>
#include "perlin.h"

typedef unsigned char block_t;
const int CHUNK_SIZE = 16;
const int CHUNK_HEIGHT = 64;

enum BlockID : block_t {
    BLOCK_AIR = 0,
    BLOCK_DIRT = 1,
    BLOCK_GRASS = 2,
    BLOCK_STONE = 3,
    BLOCK_GLASS = 4,
    BLOCK_OAK_LOG = 5,

    BLOCK_TYPES_COUNT
};

inline bool isBlockTransparent(block_t type) {
    return type == BLOCK_AIR || type == BLOCK_GLASS;
}

class Chunk {
public:
    Chunk(int cx, int cy, int cz);
    ~Chunk();

    void generate(const PerlinNoise& noise, unsigned int seed, int worldType = 0);
    void buildMesh(int totalBlocksInAtlas);
    void draw();
    void update();

    // Загрузка и сохранение чанка в бинарный файл
    bool loadFromFile(const std::string& path);
    void saveToFile(const std::string& path);

    // Доступ к сырому массиву блоков чанка для копирования/сериализации
    inline block_t* getBlocksPointer() { return blocks; }

    // Быстрый inline-доступ к оптимизированному плоскому массиву
    inline block_t getBlock(int x, int y, int z) const {
        return blocks[(x * CHUNK_HEIGHT + y) * CHUNK_SIZE + z];
    }

    inline void setBlock(int x, int y, int z, block_t type) {
        blocks[(x * CHUNK_HEIGHT + y) * CHUNK_SIZE + z] = type;
    }

    int getChunkX() const { return chunkX; }
    int getChunkY() const { return chunkY; }
    int getChunkZ() const { return chunkZ; }

private:
    int chunkX, chunkY, chunkZ;
    unsigned int VAO, VBO;
    int vertexCount;
    // Однородный 1D-массив без фрагментации ОЗУ
    block_t blocks[CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE];
};