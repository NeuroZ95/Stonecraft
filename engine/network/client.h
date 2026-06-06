#pragma once
#include <enet.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <glm/glm.hpp>
#include "../world/world.h"

struct NetworkPlayer {
    uint32_t id;
    glm::vec3 position;
    float yaw;
    float pitch;
    bool isSneaking;
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool connect(const std::string& hostAddress, uint16_t port);
    void disconnect();
    void update(World& world);

    void sendPosition(glm::vec3 pos, float yaw, float pitch, bool isSneaking);
    void sendBlockChange(glm::ivec3 pos, uint8_t blockType);

    bool isConnected() const { return connected; }
    uint32_t getLocalID() const { return localID; }

    std::unordered_map<uint32_t, NetworkPlayer> getRemotePlayers();

    // Переменные для красивого экрана загрузки мультиплеера
    bool isMultiplayerLoading;
    int receivedChunksCount;
    int expectedChunksCount;
    std::string loadingStageText;

private:
    void handlePacket(ENetPacket* packet, World& world);

    ENetHost* clientHost;
    ENetPeer* serverPeer;
    bool connected;
    uint32_t localID;

    std::mutex playersMutex;
    std::unordered_map<uint32_t, NetworkPlayer> remotePlayers;
};