#pragma once
#include <enet.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <glm/glm.hpp>
#include "../world/perlin.h"
#include "../world/chunk.h"

struct ServerClient {
    uint32_t id = 0;
    ENetPeer* peer = nullptr;
    glm::vec3 position = glm::vec3(0.0f);
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool isSneaking = false;
    char username[32] = { 0 };
    std::unordered_set<uint64_t> sentChunks;
};

// Безопасная структура для карты modifiedBlocks на сервере (без выравнивания SIMD)
struct ServerSimpleIVec3 {
    int x;
    int y;
    int z;
};

struct ServerSimpleIVec3Less {
    bool operator()(const ServerSimpleIVec3& a, const ServerSimpleIVec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

class NetworkServer {
public:
    NetworkServer();
    ~NetworkServer();

    bool start(uint16_t port, uint32_t seed = 1337, int type = 0);
    void stop();
    void update();

    bool isRunning() const { return running; }
    uint16_t getPort() const { return activePort; }
    uint32_t getSeed() const { return worldSeed; }
    int getWorldType() const { return worldType; }

private:
    void run();
    void handlePacket(ENetPeer* peer, ENetPacket* packet);
    void broadcast(const void* data, size_t size, ENetPeer* excludePeer = nullptr, bool reliable = true);

    std::vector<block_t> generateChunkBlocks(int cx, int cz, const PerlinNoise& noise, unsigned int seed, int type);
    void sendChunkToClient(uint32_t clientId, int cx, int cz);

    ENetHost* serverHost;
    std::thread serverThread;
    std::atomic<bool> running;
    uint16_t activePort;
    uint32_t worldSeed;
    int worldType; // 0 = Default, 1 = Flat

    std::mutex clientsMutex;
    std::unordered_map<uint32_t, ServerClient> clients;
    uint32_t nextClientID;

    std::mutex modifiedBlocksMutex;
    std::map<ServerSimpleIVec3, uint8_t, ServerSimpleIVec3Less> modifiedBlocks; // Используем ServerSimpleIVec3

    std::mutex serverChunksMutex;
    std::unordered_map<uint64_t, std::vector<block_t>> serverChunks;
    PerlinNoise noiseGen;
};