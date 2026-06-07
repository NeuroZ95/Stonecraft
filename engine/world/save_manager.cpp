#include "save_manager.h"
#include "../gameplay/player.h"
#include "../../tools/logger/logger.h"
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

SaveManager::SaveManager() : savingFlag(false) {}

SaveManager::~SaveManager() {
    waitTillSaved();
}

SaveManager& SaveManager::getInstance() {
    static SaveManager instance;
    return instance;
}

bool SaveManager::isSaving() const {
    return savingFlag.load();
}

void SaveManager::waitTillSaved() {
    while (savingFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SaveManager::asyncSaveTask(
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
) {
    savingFlag.store(true);

    // Имитируем небольшую задержку, чтобы визуализировать синий огонёк сохранения
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    namespace fs = std::filesystem;
    fs::create_directories("saves/" + saveName + "/chunks");

    // Запись world.dat
    std::string path = "saves/" + saveName + "/world.dat";
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
        file.write(reinterpret_cast<const char*>(&worldType), sizeof(worldType));
        file.write(reinterpret_cast<const char*>(&pPos), sizeof(pPos));
        file.write(reinterpret_cast<const char*>(&pYaw), sizeof(pYaw));
        file.write(reinterpret_cast<const char*>(&pPitch), sizeof(pPitch));
        file.write(reinterpret_cast<const char*>(&pSlot), sizeof(pSlot));
        file.write(reinterpret_cast<const char*>(pHotbar.data()), 9);

        size_t mapSize = mBlocks.size();
        file.write(reinterpret_cast<const char*>(&mapSize), sizeof(mapSize));
        for (const auto& [pos, type] : mBlocks) {
            file.write(reinterpret_cast<const char*>(&pos), sizeof(pos));
            file.write(reinterpret_cast<const char*>(&type), sizeof(type));
        }
        file.close();
    }

    // Запись бинарников чанков
    for (const auto& c : chunkData) {
        std::string chunkPath = "saves/" + saveName + "/chunks/chunk_" + std::to_string(c.cx) + "_" + std::to_string(c.cz) + ".bin";
        std::ofstream chunkFile(chunkPath, std::ios::binary);
        if (chunkFile.is_open()) {
            chunkFile.write(reinterpret_cast<const char*>(c.blocks.data()), c.blocks.size());
            chunkFile.close();
        }
    }

    savingFlag.store(false);
}

void SaveManager::triggerWorldSave(World& world, Player& player) {
    if (world.activeSaveName.empty() || world.isMultiplayer) return;
    if (savingFlag.load()) return; // Уже сохраняем

    std::vector<ChunkSaveTaskData> chunkData;
    for (const auto& [key, chunk] : world.getChunksMap()) {
        ChunkSaveTaskData td;
        td.cx = chunk->getChunkX();
        td.cz = chunk->getChunkZ();
        td.blocks.assign(chunk->getBlocksPointer(), chunk->getBlocksPointer() + (CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE));
        chunkData.push_back(td);
    }

    std::vector<uint8_t> hotbarCopy(9);
    for (int i = 0; i < 9; ++i) hotbarCopy[i] = player.hotbar[i];

    std::thread saveThread(&SaveManager::asyncSaveTask, this,
        world.activeSaveName,
        world.worldSeed,
        world.worldType,
        player.position,
        player.camera.yaw,
        player.camera.pitch,
        player.selectedSlot,
        hotbarCopy,
        world.modifiedBlocks,
        chunkData
    );
    saveThread.detach();
}