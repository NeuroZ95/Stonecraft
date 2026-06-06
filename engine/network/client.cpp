#include "client.h"
#include "protocol.h"
#include "network_manager.h"
#include "../../tools/logger/logger.h"
#include <cstring>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#endif

NetworkClient::NetworkClient() : clientHost(nullptr), serverPeer(nullptr), connected(false), localID(0),
isMultiplayerLoading(false), receivedChunksCount(0), expectedChunksCount(121), loadingStageText("Connecting...") {
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& hostAddress, uint16_t port) {
    if (connected) return false;

    Logger::log(Logger::Level::INFO, "Client: Initializing connection request to " + hostAddress + ":" + std::to_string(port));

    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        clientHost = enet_host_create(nullptr, 1, CHANNEL_COUNT, 0, 0);
    }

    if (!clientHost) {
        Logger::log(Logger::Level::ERROR, "Client: Failed to create ENet host!");
        return false;
    }

    ENetAddress address;
    std::memset(&address, 0, sizeof(ENetAddress));

    Logger::log(Logger::Level::INFO, "Client: Resolving host address...");
    bool addressResolved = false;
    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        if (enet_address_set_host(&address, hostAddress.c_str()) >= 0) {
            addressResolved = true;
        }
    }

    if (!addressResolved) {
        Logger::log(Logger::Level::ERROR, "Client: Could not resolve host address: " + hostAddress);
        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            enet_host_destroy(clientHost);
        }
        clientHost = nullptr;
        return false;
    }
    address.port = port;

    Logger::log(Logger::Level::INFO, "Client: Attempting connection via socket...");
    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        serverPeer = enet_host_connect(clientHost, &address, CHANNEL_COUNT, 0);
    }

    if (!serverPeer) {
        Logger::log(Logger::Level::ERROR, "Client: Connection attempt failed during initialization!");
        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            enet_host_destroy(clientHost);
        }
        clientHost = nullptr;
        return false;
    }

    Logger::log(Logger::Level::INFO, "Client: Waiting for server handshake response (timeout: 2000ms)...");
    ENetEvent event;
    int serviceResult = 0;
    bool handshakeSuccess = false;

    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds(2000)) {
        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            serviceResult = enet_host_service(clientHost, &event, 0);
        }

        if (serviceResult > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                handshakeSuccess = true;
                break;
            }
            else {
                if (event.packet) {
                    std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                    enet_packet_destroy(event.packet);
                }
            }
        }
        else if (serviceResult < 0) {
            int err = 0;
#ifdef _WIN32
            err = WSAGetLastError();
#else
            err = errno;
#endif

            bool isNonFatal = false;
#ifdef _WIN32
            if (err == WSAECONNRESET || err == WSAECONNREFUSED || err == WSAEWOULDBLOCK || err == WSAEINTR) {
                isNonFatal = true;
            }
#else
            if (err == ECONNRESET || err == ECONNREFUSED || err == EAGAIN || err == EWOULDBLOCK || err == EINTR) {
                isNonFatal = true;
            }
#endif

            if (isNonFatal) {
                Logger::log(Logger::Level::WARNING, "Client: enet_host_service returned non-fatal socket error (" +
                    std::to_string(err) + "). Retrying handshake...");
            }
            else {
                Logger::log(Logger::Level::ERROR, "Client: enet_host_service returned fatal socket error (" +
                    std::to_string(err) + ") during handshake!");
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (handshakeSuccess) {
        connected = true;
        Logger::log(Logger::Level::INFO, "Client: Handshake completed successfully. Sending C2S_JOIN packet...");

        PacketC2SJoin joinPacket;
        joinPacket.header.type = PACKET_C2S_JOIN;
        std::memset(joinPacket.username, 0, sizeof(joinPacket.username));
        std::strncpy(joinPacket.username, "Player", sizeof(joinPacket.username) - 1);

        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            ENetPacket* packet = enet_packet_create(&joinPacket, sizeof(joinPacket), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(serverPeer, CHANNEL_RELIABLE, packet);
        }
        return true;
    }
    else {
        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            enet_peer_reset(serverPeer);
            enet_host_destroy(clientHost);
        }
        clientHost = nullptr;
        serverPeer = nullptr;
        Logger::log(Logger::Level::WARNING, "Client: Connection timed out or server rejected the request.");
        return false;
    }
}

void NetworkClient::disconnect() {
    if (!clientHost) return;

    if (serverPeer && connected) {
        Logger::log(Logger::Level::INFO, "Client: Terminating server peer connection...");
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        enet_peer_disconnect(serverPeer, 0);
        enet_host_flush(clientHost);
    }

    if (clientHost) {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        enet_host_destroy(clientHost);
        clientHost = nullptr;
    }

    serverPeer = nullptr;
    connected = false;
    localID = 0;
    isMultiplayerLoading = false;
    receivedChunksCount = 0;

    {
        std::lock_guard<std::mutex> lock(playersMutex);
        remotePlayers.clear();
    }

    Logger::log(Logger::Level::INFO, "Client: Disconnected from host.");
}

void NetworkClient::update(World& world) {
    if (!clientHost) return;

    ENetEvent event;
    int serviceResult = 0;
    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        serviceResult = enet_host_service(clientHost, &event, 0);
    }

    while (serviceResult > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_RECEIVE:
            handlePacket(event.packet, world);
            {
                std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                enet_packet_destroy(event.packet);
            }
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            connected = false;
            Logger::log(Logger::Level::INFO, "Client: Server connection closed by host.");
            break;
        default:
            break;
        }

        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            serviceResult = enet_host_check_events(clientHost, &event);
        }
    }
}

void NetworkClient::handlePacket(ENetPacket* packet, World& world) {
    if (packet->dataLength < sizeof(PacketHeader)) return;

    PacketHeader* header = reinterpret_cast<PacketHeader*>(packet->data);

    switch (header->type) {
    case PACKET_S2C_JOIN_ACK: {
        if (packet->dataLength < sizeof(PacketS2CJoinAck)) return;
        PacketS2CJoinAck* ack = reinterpret_cast<PacketS2CJoinAck*>(packet->data);
        localID = ack->playerID;
        world.worldType = ack->worldType; // Передаем и синхронизируем тип мира между сервером и клиентом!

        Logger::log(Logger::Level::INFO, "Client: Authenticated on server. Assigned client ID: " + std::to_string(localID) +
            " | World Seed: " + std::to_string(ack->seed) + " | World Type: " + std::to_string(ack->worldType));

        if (world.isMultiplayer) {
            isMultiplayerLoading = true;
            receivedChunksCount = 0;
            expectedChunksCount = 121;
            loadingStageText = "Downloading Chunks...";
            world.regenerate(ack->seed, 0.0f, 0.0f, false);
        }
        else {
            isMultiplayerLoading = false;
        }
        break;
    }
    case PACKET_S2C_CHUNK_DATA: {
        if (packet->dataLength < sizeof(PacketS2CChunkDataHeader)) return;

        if (!world.isMultiplayer) return;

        PacketS2CChunkDataHeader* header = reinterpret_cast<PacketS2CChunkDataHeader*>(packet->data);
        if (packet->dataLength < sizeof(PacketS2CChunkDataHeader) + header->compressedSize) return;

        uint8_t decompressedBlocks[16384];
        if (!decompressRLE(packet->data + sizeof(PacketS2CChunkDataHeader), header->compressedSize, decompressedBlocks, 16384)) {
            Logger::log(Logger::Level::ERROR, "Client: Failed to decompress received chunk RLE data!");
            return;
        }

        world.receiveChunk(header->cx, header->cz, decompressedBlocks);

        if (isMultiplayerLoading) {
            receivedChunksCount++;
            loadingStageText = "Downloading Chunks (" + std::to_string(receivedChunksCount) + " / " + std::to_string(expectedChunksCount) + ")";

            if (receivedChunksCount >= expectedChunksCount) {
                isMultiplayerLoading = false;
            }
        }
        break;
    }
    case PACKET_S2C_PLAYER_POSITION: {
        if (packet->dataLength < sizeof(PacketS2CPlayerPosition)) return;
        PacketS2CPlayerPosition* pos = reinterpret_cast<PacketS2CPlayerPosition*>(packet->data);

        if (pos->playerID == localID) return;

        bool sneaking = (pos->flags & 1) != 0;
        glm::vec3 playerPos(pos->position.x, pos->position.y, pos->position.z);

        std::lock_guard<std::mutex> lock(playersMutex);
        remotePlayers[pos->playerID] = { pos->playerID, playerPos, pos->yaw, pos->pitch, sneaking };
        break;
    }
    case PACKET_S2C_BLOCK_CHANGE: {
        if (packet->dataLength < sizeof(PacketS2CBlockChange)) return;
        PacketS2CBlockChange* block = reinterpret_cast<PacketS2CBlockChange*>(packet->data);

        glm::ivec3 blockPos(block->pos.x, block->pos.y, block->pos.z);
        world.setBlock(blockPos, block->blockType);
        break;
    }
    case PACKET_S2C_PLAYER_LEAVE: {
        if (packet->dataLength < sizeof(PacketS2CPlayerLeave)) return;
        PacketS2CPlayerLeave* leave = reinterpret_cast<PacketS2CPlayerLeave*>(packet->data);

        std::lock_guard<std::mutex> lock(playersMutex);
        remotePlayers.erase(leave->playerID);
        Logger::log(Logger::Level::INFO, "Client: Player " + std::to_string(leave->playerID) + " left the game session.");
        break;
    }
    default:
        break;
    }
}

void NetworkClient::sendPosition(glm::vec3 pos, float yaw, float pitch, bool isSneaking) {
    if (!connected || !serverPeer) return;

    PacketC2SPlayerPosition packetData;
    packetData.header.type = PACKET_C2S_PLAYER_POSITION;
    packetData.position = { pos.x, pos.y, pos.z };
    packetData.yaw = yaw;
    packetData.pitch = pitch;
    packetData.flags = isSneaking ? 1 : 0;

    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        ENetPacket* packet = enet_packet_create(&packetData, sizeof(packetData), 0);
        enet_peer_send(serverPeer, CHANNEL_UNRELIABLE, packet);
    }
}

void NetworkClient::sendBlockChange(glm::ivec3 pos, uint8_t blockType) {
    if (!connected || !serverPeer) return;

    PacketC2SBlockChange packetData;
    packetData.header.type = PACKET_C2S_BLOCK_CHANGE;
    packetData.pos = { pos.x, pos.y, pos.z };
    packetData.blockType = blockType;

    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        ENetPacket* packet = enet_packet_create(&packetData, sizeof(packetData), ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(serverPeer, CHANNEL_RELIABLE, packet);
    }
}

std::unordered_map<uint32_t, NetworkPlayer> NetworkClient::getRemotePlayers() {
    std::lock_guard<std::mutex> lock(playersMutex);
    return remotePlayers;
}