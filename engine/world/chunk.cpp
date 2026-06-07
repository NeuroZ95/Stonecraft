// --- FILE: .\engine\world\chunk.cpp ---
#include "chunk.h"
#include <GL/glew.h>
#include <cmath>
#include <cstring>
#include <fstream>

Chunk::Chunk(int cx, int cy, int cz) : chunkX(cx), chunkY(cy), chunkZ(cz), VAO(0), VBO(0), vertexCount(0) {
    std::memset(blocks, BLOCK_AIR, sizeof(blocks));
}

Chunk::~Chunk() {
    if (VAO != 0 && glDeleteVertexArrays != nullptr) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0 && glDeleteBuffers != nullptr) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
}

void Chunk::generate(const PerlinNoise& noise, unsigned int seed, int worldType) {
    if (worldType == 1) { // Flat World
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    block_t type = BLOCK_AIR;
                    if (y >= 0 && y <= 2) {
                        type = BLOCK_STONE;
                    }
                    else if (y == 3) {
                        type = BLOCK_DIRT;
                    }
                    else if (y == 4) {
                        type = BLOCK_GRASS;
                    }
                    setBlock(x, y, z, type);
                }
            }
        }
        return;
    }

    // Pass 1: Генерация базового рельефа чанка
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int worldX = chunkX * CHUNK_SIZE + x;
            int worldZ = chunkZ * CHUNK_SIZE + z;

            float continentalness = noise.noise(worldX * 0.003f, worldZ * 0.003f);
            float hills = noise.noise(worldX * 0.015f, worldZ * 0.015f);
            float detail = noise.noise(worldX * 0.06f, worldZ * 0.06f);

            float baseHeight = 32.0f;
            float mountainShape = continentalness * 16.0f;
            float hillShape = hills * 6.0f;
            float detailShape = detail * 1.5f;

            int surfaceHeight = static_cast<int>(baseHeight + mountainShape + hillShape + detailShape);

            if (surfaceHeight < 5) surfaceHeight = 5;
            if (surfaceHeight >= CHUNK_HEIGHT) surfaceHeight = CHUNK_HEIGHT - 1;

            for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                block_t blockType = BLOCK_AIR;

                if (y == surfaceHeight) {
                    blockType = BLOCK_GRASS;
                }
                else if (y < surfaceHeight && y >= surfaceHeight - 3) {
                    blockType = BLOCK_DIRT;
                }
                else if (y < surfaceHeight - 3) {
                    blockType = BLOCK_STONE;
                }

                if (blockType != BLOCK_AIR && y > 2 && y < surfaceHeight - 3) {
                    float cave1 = noise.noise(worldX * 0.04f, y * 0.08f, worldZ * 0.04f);
                    float cave2 = noise.noise(worldX * 0.04f + 100.0f, y * 0.08f + 100.0f, worldZ * 0.04f + 100.0f);

                    if (std::abs(cave1) < 0.12f && std::abs(cave2) < 0.12f) {
                        blockType = BLOCK_AIR;
                    }
                }

                setBlock(x, y, z, blockType);
            }
        }
    }

    // Pass 2: Упреждающий спавн деревьев (сканируем также и соседние границы в радиусе 3 блоков)
    int worldStartX = chunkX * CHUNK_SIZE;
    int worldStartZ = chunkZ * CHUNK_SIZE;

    for (int wx = worldStartX - 3; wx < worldStartX + CHUNK_SIZE + 3; ++wx) {
        for (int wz = worldStartZ - 3; wz < worldStartZ + CHUNK_SIZE + 3; ++wz) {
            float biomeVal = noise.noise(wx * 0.005f, wz * 0.005f);
            float spawnProb = 0.0f;

            if (biomeVal < -0.4f) {
                spawnProb = 0.0f;
            }
            else if (biomeVal < 0.0f) {
                spawnProb = 0.002f;
            }
            else if (biomeVal < 0.4f) {
                spawnProb = 0.012f;
            }
            else {
                spawnProb = 0.055f;
            }

            if (spawnProb > 0.0f) {
                unsigned int hashVal = seed ^ (wx * 73856093) ^ (wz * 19349663);
                hashVal = (hashVal ^ 61) ^ (hashVal >> 16);
                hashVal *= 9;
                hashVal = hashVal ^ (hashVal >> 11);
                float randVal = static_cast<float>(hashVal & 0xFFFF) / 65535.0f;

                if (randVal < spawnProb) {
                    // Рассчитываем высоту поверхности для данной координаты (wx, wz)
                    float continentalness = noise.noise(wx * 0.003f, wz * 0.003f);
                    float hills = noise.noise(wx * 0.015f, wz * 0.015f);
                    float detail = noise.noise(wx * 0.06f, wz * 0.06f);

                    float baseHeight = 32.0f;
                    float mountainShape = continentalness * 16.0f;
                    float hillShape = hills * 6.0f;
                    float detailShape = detail * 1.5f;

                    int surfaceHeight = static_cast<int>(baseHeight + mountainShape + hillShape + detailShape);
                    if (surfaceHeight < 5) surfaceHeight = 5;
                    if (surfaceHeight >= CHUNK_HEIGHT) surfaceHeight = CHUNK_HEIGHT - 1;

                    unsigned int heightHash = hashVal ^ 38241243;
                    int treeHeight = 4 + (heightHash % 3);

                    // Если ствол дерева находится внутри нашего чанка, строим его
                    if (wx >= worldStartX && wx < worldStartX + CHUNK_SIZE &&
                        wz >= worldStartZ && wz < worldStartZ + CHUNK_SIZE) {
                        int localX = wx - worldStartX;
                        int localZ = wz - worldStartZ;

                        setBlock(localX, surfaceHeight, localZ, BLOCK_DIRT);
                        for (int th = 1; th <= treeHeight; ++th) {
                            int ly = surfaceHeight + th;
                            if (ly < CHUNK_HEIGHT) {
                                setBlock(localX, ly, localZ, BLOCK_OAK_LOG);
                            }
                        }
                    }

                    // Рассчитываем и накладываем крону листьев (даже если само дерево за границей чанка)
                    int topY = surfaceHeight + treeHeight;
                    for (int ly = topY - 2; ly <= topY + 1; ++ly) {
                        if (ly >= CHUNK_HEIGHT) continue;
                        int relativeY = ly - topY;
                        int radius = 2;

                        if (relativeY == 1) {
                            radius = 1; // 1 блок над верхним бревном
                        }
                        else if (relativeY == 0) {
                            radius = 2;
                        }
                        else if (relativeY == -1) {
                            radius = 3; // Максимальный радиус в 3 блока
                        }
                        else if (relativeY == -2) {
                            radius = 2;
                        }

                        for (int ldx = -radius; ldx <= radius; ++ldx) {
                            for (int ldz = -radius; ldz <= radius; ++ldz) {
                                if (radius == 1) {
                                    if (std::abs(ldx) == 1 && std::abs(ldz) == 1) continue;
                                }
                                else if (radius == 2) {
                                    if (std::abs(ldx) == 2 && std::abs(ldz) == 2) continue;
                                }
                                else if (radius == 3) {
                                    if (std::abs(ldx) == 3 && std::abs(ldz) == 3) continue;
                                    if (std::abs(ldx) + std::abs(ldz) >= 5) continue;
                                }

                                int leafWorldX = wx + ldx;
                                int leafWorldZ = wz + ldz;

                                // Размещаем листья только в том случае, если они попадают в границы текущего чанка
                                if (leafWorldX >= worldStartX && leafWorldX < worldStartX + CHUNK_SIZE &&
                                    leafWorldZ >= worldStartZ && leafWorldZ < worldStartZ + CHUNK_SIZE) {
                                    int localLX = leafWorldX - worldStartX;
                                    int localLZ = leafWorldZ - worldStartZ;

                                    block_t current = getBlock(localLX, ly, localLZ);
                                    if (current == BLOCK_AIR || current == BLOCK_OAK_LEAVES) {
                                        setBlock(localLX, ly, localLZ, BLOCK_OAK_LEAVES);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

bool Chunk::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    file.read(reinterpret_cast<char*>(blocks), sizeof(blocks));
    return true;
}

void Chunk::saveToFile(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return;
    file.write(reinterpret_cast<const char*>(blocks), sizeof(blocks));
}

void Chunk::buildMesh(int totalBlocksInAtlas) {
    std::vector<float> vertices;

    float baseVertices[6][6][5] = {
        {{0.0f, 0.0f, 1.0f,  0.0f, 0.0f}, {1.0f, 0.0f, 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f, 1.0f,  1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 1.0f,  0.0f, 1.0f}, {0.0f, 0.0f, 1.0f,  0.0f, 0.0f}},
        {{0.0f, 0.0f, 0.0f,  0.0f, 0.0f}, {0.0f, 1.0f, 0.0f,  0.0f, 1.0f}, {1.0f, 1.0f, 0.0f,  1.0f, 1.0f},
         {1.0f, 1.0f, 0.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 0.0f,  0.0f, 0.0f}},
        {{0.0f, 1.0f, 1.0f,  0.0f, 1.0f}, {0.0f, 1.0f, 0.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 0.0f,  1.0f, 0.0f},
         {0.0f, 0.0f, 0.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f,  0.0f, 0.0f}, {0.0f, 1.0f, 1.0f,  0.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f,  0.0f, 1.0f}, {1.0f, 0.0f, 1.0f,  0.0f, 0.0f}, {1.0f, 0.0f, 0.0f,  1.0f, 0.0f},
         {1.0f, 0.0f, 0.0f,  1.0f, 0.0f}, {1.0f, 1.0f, 0.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 1.0f,  0.0f, 1.0f}},
        {{0.0f, 1.0f, 0.0f,  0.0f, 1.0f}, {0.0f, 1.0f, 1.0f,  0.0f, 0.0f}, {1.0f, 1.0f, 1.0f,  1.0f, 0.0f},
         {1.0f, 1.0f, 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f, 0.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 0.0f,  0.0f, 1.0f}},
        {{0.0f, 0.0f, 0.0f,  0.0f, 1.0f}, {1.0f, 0.0f, 0.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 1.0f,  1.0f, 0.0f},
         {1.0f, 0.0f, 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f,  0.0f, 0.0f}, {0.0f, 0.0f, 0.0f,  0.0f, 1.0f}}
    };

    float segmentWidth = 1.0f / 6.0f;
    float segmentHeight = 1.0f / (float)totalBlocksInAtlas;

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                block_t blockType = getBlock(x, y, z);
                if (blockType == BLOCK_AIR) continue;

                float wx = (float)x;
                float wy = (float)y;
                float wz = (float)z;

                int dx[] = { 0,  0, -1,  1,  0,  0 };
                int dy[] = { 0,  0,  0,  0,  1, -1 };
                int dz[] = { 1, -1,  0,  0,  0,  0 };

                for (int face = 0; face < 6; ++face) {
                    int nx = x + dx[face];
                    int ny = y + dy[face];
                    int nz = z + dz[face];

                    bool showFace = false;
                    if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= CHUNK_SIZE) {
                        showFace = true;
                    }
                    else {
                        block_t neighborType = getBlock(nx, ny, nz);
                        if (blockType == BLOCK_OAK_LEAVES) {
                            // Листья рендерят свои грани перед воздухом, стеклом и другими листьями
                            showFace = (neighborType == BLOCK_AIR || neighborType == BLOCK_GLASS || neighborType == BLOCK_OAK_LEAVES);
                        }
                        else if (blockType == BLOCK_GLASS) {
                            showFace = (neighborType == BLOCK_AIR || (isBlockTransparent(neighborType) && neighborType != BLOCK_GLASS));
                        }
                        else {
                            showFace = isBlockTransparent(neighborType);
                        }
                    }

                    if (!showFace) continue;

                    for (int v = 0; v < 6; ++v) {
                        float vx = baseVertices[face][v][0] + wx;
                        float vy = baseVertices[face][v][1] + wy;
                        float vz = baseVertices[face][v][2] + wz;

                        float tu = baseVertices[face][v][3];
                        float tv = baseVertices[face][v][4];

                        if (face < 4) {
                            tv = 1.0f - tv;
                        }

                        tu = (tu * segmentWidth) + ((float)face * segmentWidth);
                        tv = (tv * segmentHeight) + ((float)blockType * segmentHeight);

                        vertices.push_back(vx);
                        vertices.push_back(vy);
                        vertices.push_back(vz);
                        vertices.push_back(tu);
                        vertices.push_back(tv);
                    }
                }
            }
        }
    }

    vertexCount = vertices.size() / 5;
    if (vertexCount == 0) return;

    if (VAO == 0) glGenVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Chunk::draw() {
    if (vertexCount == 0) return;
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void Chunk::update() {
    bool needsRebuild = false;

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                if (getBlock(x, y, z) == BLOCK_GRASS) {
                    bool covered = false;

                    if (y + 1 < CHUNK_HEIGHT) {
                        if (getBlock(x, y + 1, z) != BLOCK_AIR) {
                            covered = true;
                        }
                    }

                    if (covered) {
                        setBlock(x, y, z, BLOCK_DIRT);
                        needsRebuild = true;
                    }
                }
            }
        }
    }

    if (needsRebuild) {
        buildMesh(BLOCK_TYPES_COUNT);
    }
}