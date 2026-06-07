#pragma once
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <glm/glm.hpp>
#include "world.h"

class Player;

struct ChunkSaveTaskData {
    int cx, cz;
    std::vector<uint8_t> blocks;
};

class SaveManager {
public:
    static SaveManager& getInstance();

    bool isSaving() const;
    void triggerWorldSave(World& world, Player& player);
    void waitTillSaved();

private:
    SaveManager();
    ~SaveManager();

    void asyncSaveTask(
        std::string saveName,
        unsigned int seed,
        int worldType,
        glm::vec3 pPos,
        float pYaw,
        float pPitch,
        int pSlot,
        std::vector<uint8_t> pHotbar,
        std::map<SimpleIVec3, block_t, SimpleIVec3Less> mBlocks,
        std::vector<ChunkSaveTaskData> chunkData
    );

    std::atomic<bool> savingFlag;
};