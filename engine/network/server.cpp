#include "server.h"
#include "protocol.h"
#include "network_manager.h" // Добавлен для доступа к NetworkManager::enetMutex
#include "../../tools/logger/logger.h"
#include "../world/world.h"
#include <cstring>
#include <memory>

NetworkServer::NetworkServer() : serverHost(nullptr), running(false), activePort(0), nextClientID(1), worldSeed(1337), worldType(0), noiseGen(1337) {}

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start(uint16_t port, uint32_t seed, int type) {
    if (running) {
        Logger::log(Logger::Level::WARNING, "Server: Cannot start because server is already running!");
        return false;
    }

    worldSeed = seed;
    worldType = type;
    noiseGen = PerlinNoise(seed);

    Logger::log(Logger::Level::INFO, "Server: Starting local server initialization...");
    Logger::log(Logger::Level::INFO, "Server: Requested binding port: " + std::to_string(port) + " (0 means auto-allocate)");

    ENetAddress address;
    std::memset(&address, 0, sizeof(ENetAddress));

    {
        std::lock_guard<std::mutex> lock(NetworkManager::enetMutex);
        if (enet_address_set_host(&address, "0.0.0.0") < 0) {
            address.host = ENET_HOST_ANY;
        }
    }
    address.port = port;

    Logger::log(Logger::Level::INFO, "Server: Attempting to create ENet host with ENET_HOST_ANY...");
    {
        std::lock_guard<std::mutex> lock(NetworkManager::enetMutex);
        serverHost = enet_host_create(&address, 32, CHANNEL_COUNT, 0, 0);
    }

    if (!serverHost) {
        Logger::log(Logger::Level::ERROR, "Server: Failed to create ENet host on port " + std::to_string(port) + ". Port might be in use.");
        return false;
    }

    Logger::log(Logger::Level::INFO, "Server: ENet host created successfully. Max clients: 32, channels: " + std::to_string(CHANNEL_COUNT));

    if (port == 0) {
        Logger::log(Logger::Level::INFO, "Server: Port is 0, retrieving dynamically allocated system port...");
#ifdef _WIN32
        uintptr_t s = (uintptr_t)serverHost->socket;
#else
        int s = (int)serverHost->socket;
#endif
        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(s, (struct sockaddr*)&sin, &len) != -1) {
            activePort = ntohs(sin.sin_port);
            Logger::log(Logger::Level::INFO, "Server: Successfully bound to dynamically allocated port: " + std::to_string(activePort));
        }
        else {
            activePort = 54545;
            Logger::log(Logger::Level::WARNING, "Server: getsockname failed. Using fallback default: " + std::to_string(activePort));
        }
    }
    else {
        activePort = port;
        Logger::log(Logger::Level::INFO, "Server: Successfully bound to requested port: " + std::to_string(activePort));
    }

    running = true;
    Logger::log(Logger::Level::INFO, "Server: Spawning server update thread...");
    serverThread = std::thread(&NetworkServer::run, this);
    Logger::log(Logger::Level::INFO, "Server: Background update thread spawned successfully.");
    Logger::log(Logger::Level::INFO, "Server: Server is fully operational and listening on port " + std::to_string(activePort));
    return true;
}

void NetworkServer::stop() {
    if (!running) {
        Logger::log(Logger::Level::INFO, "Server: Stop requested, but server was not running.");
        return;
    }

    Logger::log(Logger::Level::INFO, "Server: Stopping server operations...");
    running = false;
    if (serverThread.joinable()) {
        Logger::log(Logger::Level::INFO, "Server: Waiting for background thread to join...");
        serverThread.join();
        Logger::log(Logger::Level::INFO, "Server: Background thread joined.");
    }

    if (serverHost) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        Logger::log(Logger::Level::INFO, "Server: Disconnecting " + std::to_string(clients.size()) + " connected peers...");

        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            for (auto& [id, client] : clients) {
                enet_peer_disconnect(client.peer, 0);
            }
            enet_host_flush(serverHost);
            enet_host_destroy(serverHost);
        }
        serverHost = nullptr;
    }

    clients.clear();
    {
        std::lock_guard<std::mutex> lock(modifiedBlocksMutex);
        modifiedBlocks.clear();
    }
    {
        std::lock_guard<std::mutex> lock(serverChunksMutex);
        serverChunks.clear();
    }
    activePort = 0;
    Logger::log(Logger::Level::INFO, "Server: Stopped successfully.");
}

void NetworkServer::run() {
    Logger::log(Logger::Level::INFO, "Server: Background update loop active.");
    while (running) {
        update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Logger::log(Logger::Level::INFO, "Server: Background update loop closed.");
}

void NetworkServer::update() {
    if (!serverHost) return;

    ENetEvent event;
    int serviceResult = 0;
    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        serviceResult = enet_host_service(serverHost, &event, 0);
    }

    if (serviceResult < 0) {
        int err = 0;
#ifdef _WIN32
        err = WSAGetLastError();
#else
        err = errno;
#endif
        Logger::log(Logger::Level::ERROR, "Server: enet_host_service returned socket error (" + std::to_string(err) + ")!");
    }

    while (serviceResult > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            char ipStr[64];
            {
                std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                enet_address_get_host_ip(&event.peer->address, ipStr, sizeof(ipStr));
            }
            Logger::log(Logger::Level::INFO, "Server: Incoming connection accepted from " + std::string(ipStr) + ":" + std::to_string(event.peer->address.port));
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE: {
            handlePacket(event.peer, event.packet);
            {
                std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                enet_packet_destroy(event.packet);
            }
            break;
        }
        case ENET_EVENT_TYPE_DISCONNECT: {
            uint32_t disconnectedID = 0;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    if (it->second.peer == event.peer) {
                        disconnectedID = it->first;
                        Logger::log(Logger::Level::INFO, "Server: Client '" + std::string(it->second.username) + "' (ID: " + std::to_string(disconnectedID) + ") disconnected.");
                        clients.erase(it);
                        break;
                    }
                }
            }

            if (disconnectedID != 0) {
                PacketS2CPlayerLeave leavePacket;
                leavePacket.header.type = PACKET_S2C_PLAYER_LEAVE;
                leavePacket.playerID = disconnectedID;
                broadcast(&leavePacket, sizeof(leavePacket), nullptr, true);
            }
            break;
        }
        default:
            break;
        }

        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            serviceResult = enet_host_check_events(serverHost, &event);
        }
    }
}

std::vector<block_t> NetworkServer::generateChunkBlocks(int cx, int cz, const PerlinNoise& noise, unsigned int seed, int type) {
    std::vector<block_t> blocks(16 * 64 * 16, 0);

    if (type == 1) { // Flat World
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                for (int y = 0; y < 64; ++y) {
                    block_t blockType = 0;
                    if (y >= 0 && y <= 2) {
                        blockType = 3; // BLOCK_STONE
                    }
                    else if (y == 3) {
                        blockType = 1; // BLOCK_DIRT
                    }
                    else if (y == 4) {
                        blockType = 2; // BLOCK_GRASS
                    }
                    blocks[(x * 64 + y) * 16 + z] = blockType;
                }
            }
        }
        return blocks;
    }

    // Pass 1: Генерация базового рельефа чанка
    for (int x = 0; x < 16; ++x) {
        for (int z = 0; z < 16; ++z) {
            int worldX = cx * 16 + x;
            int worldZ = cz * 16 + z;

            float continentalness = noise.noise(worldX * 0.003f, worldZ * 0.003f);
            float hills = noise.noise(worldX * 0.015f, worldZ * 0.015f);
            float detail = noise.noise(worldX * 0.06f, worldZ * 0.06f);

            float baseHeight = 32.0f;
            float mountainShape = continentalness * 16.0f;
            float hillShape = hills * 6.0f;
            float detailShape = detail * 1.5f;

            int surfaceHeight = static_cast<int>(baseHeight + mountainShape + hillShape + detailShape);

            if (surfaceHeight < 5) surfaceHeight = 5;
            if (surfaceHeight >= 64) surfaceHeight = 64 - 1;

            for (int y = 0; y < 64; ++y) {
                block_t blockType = 0;

                if (y == surfaceHeight) {
                    blockType = 2; // BLOCK_GRASS
                }
                else if (y < surfaceHeight && y >= surfaceHeight - 3) {
                    blockType = 1; // BLOCK_DIRT
                }
                else if (y < surfaceHeight - 3) {
                    blockType = 3; // BLOCK_STONE
                }

                if (blockType != 0 && y > 2 && y < surfaceHeight - 3) {
                    float cave1 = noise.noise(worldX * 0.04f, y * 0.08f, worldZ * 0.04f);
                    float cave2 = noise.noise(worldX * 0.04f + 100.0f, y * 0.08f + 100.0f, worldZ * 0.04f + 100.0f);

                    if (std::abs(cave1) < 0.12f && std::abs(cave2) < 0.12f) {
                        blockType = 0;
                    }
                }

                blocks[(x * 64 + y) * 16 + z] = blockType;
            }
        }
    }

    // Pass 2: Упреждающий спавн деревьев
    int worldStartX = cx * 16;
    int worldStartZ = cz * 16;

    for (int wx = worldStartX - 3; wx < worldStartX + 16 + 3; ++wx) {
        for (int wz = worldStartZ - 3; wz < worldStartZ + 16 + 3; ++wz) {
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
                    float continentalness = noise.noise(wx * 0.003f, wz * 0.003f);
                    float hills = noise.noise(wx * 0.015f, wz * 0.015f);
                    float detail = noise.noise(wx * 0.06f, wz * 0.06f);

                    float baseHeight = 32.0f;
                    float mountainShape = continentalness * 16.0f;
                    float hillShape = hills * 6.0f;
                    float detailShape = detail * 1.5f;

                    int surfaceHeight = static_cast<int>(baseHeight + mountainShape + hillShape + detailShape);
                    if (surfaceHeight < 5) surfaceHeight = 5;
                    if (surfaceHeight >= 64) surfaceHeight = 63;

                    unsigned int heightHash = hashVal ^ 38241243;
                    int treeHeight = 4 + (heightHash % 3);

                    // Если ствол находится внутри границ текущего чанка
                    if (wx >= worldStartX && wx < worldStartX + 16 &&
                        wz >= worldStartZ && wz < worldStartZ + 16) {
                        int localX = wx - worldStartX;
                        int localZ = wz - worldStartZ;

                        blocks[(localX * 64 + surfaceHeight) * 16 + localZ] = 1; // Превращаем траву в землю
                        for (int th = 1; th <= treeHeight; ++th) {
                            int ly = surfaceHeight + th;
                            if (ly < 64) {
                                blocks[(localX * 64 + ly) * 16 + localZ] = 5; // BLOCK_OAK_LOG
                            }
                        }
                    }

                    // Накладываем крону листьев по шаблону
                    int topY = surfaceHeight + treeHeight;
                    for (int ly = topY - 2; ly <= topY + 1; ++ly) {
                        if (ly >= 64) continue;
                        int relativeY = ly - topY;
                        int radius = 2;

                        if (relativeY == 1) {
                            radius = 1;
                        }
                        else if (relativeY == 0) {
                            radius = 2;
                        }
                        else if (relativeY == -1) {
                            radius = 3;
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

                                if (leafWorldX >= worldStartX && leafWorldX < worldStartX + 16 &&
                                    leafWorldZ >= worldStartZ && leafWorldZ < worldStartZ + 16) {
                                    int localLX = leafWorldX - worldStartX;
                                    int localLZ = leafWorldZ - worldStartZ;

                                    size_t idx = (localLX * 64 + ly) * 16 + localLZ;
                                    block_t current = blocks[idx];
                                    if (current == 0 || current == 6) { // BLOCK_AIR или BLOCK_OAK_LEAVES
                                        blocks[idx] = 6; // BLOCK_OAK_LEAVES
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return blocks;
}

void NetworkServer::sendChunkToClient(uint32_t clientId, int cx, int cz) {
    uint64_t key = (static_cast<uint64_t>(cx) << 32) | (static_cast<uint32_t>(cz) & 0xFFFFFFFF);

    std::vector<block_t> blocks;
    {
        std::lock_guard<std::mutex> lock(serverChunksMutex);
        auto it = serverChunks.find(key);
        if (it == serverChunks.end()) {
            blocks = generateChunkBlocks(cx, cz, noiseGen, worldSeed, worldType);

            {
                std::lock_guard<std::mutex> modLock(modifiedBlocksMutex);
                for (const auto& [pos, blockType] : modifiedBlocks) {
                    if (pos.y < 0 || pos.y >= 64) continue;

                    int ccx = World::floorDiv(pos.x, 16);
                    int ccz = World::floorDiv(pos.z, 16);
                    if (ccx == cx && ccz == cz) {
                        int localX = World::floorMod(pos.x, 16);
                        int localZ = World::floorMod(pos.z, 16);
                        blocks[(localX * 64 + pos.y) * 16 + localZ] = blockType;
                    }
                }
            }
            serverChunks[key] = blocks;
        }
        else {
            blocks = it->second;
        }
    }

    if (blocks.size() < 16384) {
        Logger::log(Logger::Level::ERROR, "Server: blocks size is too small (" + std::to_string(blocks.size()) + ") for chunk compression.");
        return;
    }

    std::vector<uint8_t> rleData = compressRLE(blocks.data(), blocks.size());

    std::vector<uint8_t> packetBuffer(sizeof(PacketS2CChunkDataHeader) + rleData.size());
    PacketS2CChunkDataHeader* header = reinterpret_cast<PacketS2CChunkDataHeader*>(packetBuffer.data());
    header->header.type = PACKET_S2C_CHUNK_DATA;
    header->cx = cx;
    header->cz = cz;
    header->compressedSize = static_cast<uint16_t>(rleData.size());

    std::memcpy(packetBuffer.data() + sizeof(PacketS2CChunkDataHeader), rleData.data(), rleData.size());

    ENetPacket* packet = nullptr;
    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        packet = enet_packet_create(packetBuffer.data(), packetBuffer.size(), ENET_PACKET_FLAG_RELIABLE);
    }
    if (!packet) return;

    bool sent = false;
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto cIt = clients.find(clientId);
    if (cIt != clients.end()) {
        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            enet_peer_send(cIt->second.peer, CHANNEL_RELIABLE, packet);
        }
        cIt->second.sentChunks.insert(key);
        sent = true;
    }

    if (!sent) {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        enet_packet_destroy(packet);
    }
}

void NetworkServer::handlePacket(ENetPeer* peer, ENetPacket* packet) {
    if (packet->dataLength < sizeof(PacketHeader)) return;

    PacketHeader* header = reinterpret_cast<PacketHeader*>(packet->data);

    switch (header->type) {
    case PACKET_C2S_JOIN: {
        if (packet->dataLength < sizeof(PacketC2SJoin)) return;
        PacketC2SJoin* joinData = reinterpret_cast<PacketC2SJoin*>(packet->data);

        uint32_t assignedID = nextClientID++;
        ServerClient newClient;
        newClient.id = assignedID;
        newClient.peer = peer;
        newClient.position = glm::vec3(0.0f, 40.0f, 0.0f);
        newClient.yaw = -90.0f;
        newClient.pitch = 0.0f;
        newClient.isSneaking = false;

        std::memset(newClient.username, 0, sizeof(newClient.username));
        std::strncpy(newClient.username, joinData->username, sizeof(newClient.username) - 1);

        char ipStr[64];
        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            enet_address_get_host_ip(&peer->address, ipStr, sizeof(ipStr));
        }
        Logger::log(Logger::Level::INFO, "Server: Player '" + std::string(newClient.username) + "' authorized from " + std::string(ipStr) + ". Assigned ID: " + std::to_string(assignedID));

        PacketS2CJoinAck ack;
        ack.header.type = PACKET_S2C_JOIN_ACK;
        ack.playerID = assignedID;
        ack.spawnPos = { newClient.position.x, newClient.position.y, newClient.position.z };
        ack.seed = worldSeed;
        ack.worldType = static_cast<uint8_t>(worldType);

        {
            std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
            ENetPacket* ackPacket = enet_packet_create(&ack, sizeof(ack), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peer, CHANNEL_RELIABLE, ackPacket);
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients[assignedID] = newClient;
        }

        const int spawnRadius = 5;
        for (int dx = -spawnRadius; dx <= spawnRadius; ++dx) {
            for (int dz = -spawnRadius; dz <= spawnRadius; ++dz) {
                sendChunkToClient(assignedID, dx, dz);
            }
        }

        {
            std::lock_guard<std::mutex> lock(modifiedBlocksMutex);
            for (const auto& [pos, blockType] : modifiedBlocks) {
                PacketS2CBlockChange s2cBlock;
                s2cBlock.header.type = PACKET_S2C_BLOCK_CHANGE;
                s2cBlock.pos = { pos.x, pos.y, pos.z };
                s2cBlock.blockType = blockType;

                {
                    std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                    ENetPacket* blockPacket = enet_packet_create(&s2cBlock, sizeof(s2cBlock), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(peer, CHANNEL_RELIABLE, blockPacket);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (const auto& [id, existing] : clients) {
                if (id == assignedID) continue;
                PacketS2CPlayerPosition existingPos;
                existingPos.header.type = PACKET_S2C_PLAYER_POSITION;
                existingPos.playerID = existing.id;
                existingPos.position = { existing.position.x, existing.position.y, existing.position.z };
                existingPos.yaw = existing.yaw;
                existingPos.pitch = existing.pitch;
                existingPos.flags = existing.isSneaking ? 1 : 0;

                {
                    std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                    ENetPacket* posPacket = enet_packet_create(&existingPos, sizeof(existingPos), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(peer, CHANNEL_RELIABLE, posPacket);
                }
            }
        }

        PacketS2CPlayerPosition newPlayerPos;
        newPlayerPos.header.type = PACKET_S2C_PLAYER_POSITION;
        newPlayerPos.playerID = assignedID;
        newPlayerPos.position = { newClient.position.x, newClient.position.y, newClient.position.z };
        newPlayerPos.yaw = newClient.yaw;
        newPlayerPos.pitch = newClient.pitch;
        newPlayerPos.flags = newClient.isSneaking ? 1 : 0;
        broadcast(&newPlayerPos, sizeof(newPlayerPos), peer, true);

        break;
    }
    case PACKET_C2S_PLAYER_POSITION: {
        if (packet->dataLength < sizeof(PacketC2SPlayerPosition)) return;
        PacketC2SPlayerPosition* posData = reinterpret_cast<PacketC2SPlayerPosition*>(packet->data);

        uint32_t clientID = 0;
        glm::vec3 playerPos;
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& [id, client] : clients) {
                if (client.peer == peer) {
                    client.position = glm::vec3(posData->position.x, posData->position.y, posData->position.z);
                    client.yaw = posData->yaw;
                    client.pitch = posData->pitch;
                    client.isSneaking = (posData->flags & 1) != 0;
                    clientID = id;
                    playerPos = client.position;
                    break;
                }
            }
        }

        if (clientID != 0) {
            PacketS2CPlayerPosition s2cPos;
            s2cPos.header.type = PACKET_S2C_PLAYER_POSITION;
            s2cPos.playerID = clientID;
            s2cPos.position = posData->position;
            s2cPos.yaw = posData->yaw;
            s2cPos.pitch = posData->pitch;
            s2cPos.flags = posData->flags;

            broadcast(&s2cPos, sizeof(s2cPos), peer, false);

            int pcx = World::floorDiv(static_cast<int>(playerPos.x), 16);
            int pcz = World::floorDiv(static_cast<int>(playerPos.z), 16);
            const int viewDistance = 5;

            for (int dx = -viewDistance; dx <= viewDistance; ++dx) {
                for (int dz = -viewDistance; dz <= viewDistance; ++dz) {
                    int cx = pcx + dx;
                    int cz = pcz + dz;
                    uint64_t key = (static_cast<uint64_t>(cx) << 32) | (static_cast<uint32_t>(cz) & 0xFFFFFFFF);

                    bool alreadySent = false;
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        auto cIt = clients.find(clientID);
                        if (cIt != clients.end() && cIt->second.sentChunks.count(key) > 0) {
                            alreadySent = true;
                        }
                    }

                    if (!alreadySent) {
                        sendChunkToClient(clientID, cx, cz);
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                auto cIt = clients.find(clientID);
                if (cIt != clients.end()) {
                    std::vector<uint64_t> toRemove;
                    for (uint64_t key : cIt->second.sentChunks) {
                        int cx = static_cast<int>(key >> 32);
                        int cz = static_cast<int>(key & 0xFFFFFFFF);
                        if (std::abs(cx - pcx) > viewDistance + 3 || std::abs(cz - pcz) > viewDistance + 3) {
                            toRemove.push_back(key);
                        }
                    }
                    for (uint64_t key : toRemove) {
                        cIt->second.sentChunks.erase(key);
                    }
                }
            }
        }
        break;
    }
    case PACKET_C2S_BLOCK_CHANGE: {
        if (packet->dataLength < sizeof(PacketC2SBlockChange)) return;
        PacketC2SBlockChange* blockData = reinterpret_cast<PacketC2SBlockChange*>(packet->data);

        if (blockData->pos.y < 0 || blockData->pos.y >= 64) {
            return;
        }

        Logger::log(Logger::Level::INFO, "Server: Block change request from client: " +
            std::to_string(blockData->pos.x) + ", " +
            std::to_string(blockData->pos.y) + ", " +
            std::to_string(blockData->pos.z) + " to ID " + std::to_string(blockData->blockType));

        ServerSimpleIVec3 blockPos = { blockData->pos.x, blockData->pos.y, blockData->pos.z };

        {
            std::lock_guard<std::mutex> lock(modifiedBlocksMutex);
            modifiedBlocks[blockPos] = blockData->blockType;
        }

        int cx = World::floorDiv(blockPos.x, 16);
        int cz = World::floorDiv(blockPos.z, 16);
        uint64_t key = (static_cast<uint64_t>(cx) << 32) | (static_cast<uint32_t>(cz) & 0xFFFFFFFF);
        {
            std::lock_guard<std::mutex> lock(serverChunksMutex);
            auto it = serverChunks.find(key);
            if (it != serverChunks.end()) {
                int localX = World::floorMod(blockPos.x, 16);
                int localZ = World::floorMod(blockPos.z, 16);
                it->second[(localX * 64 + blockPos.y) * 16 + localZ] = blockData->blockType;
            }
        }

        PacketS2CBlockChange s2cBlock;
        s2cBlock.header.type = PACKET_S2C_BLOCK_CHANGE;
        s2cBlock.pos = blockData->pos;
        s2cBlock.blockType = blockData->blockType;

        broadcast(&s2cBlock, sizeof(s2cBlock), nullptr, true);
        break;
    }
    default:
        break;
    }
}

void NetworkServer::broadcast(const void* data, size_t size, ENetPeer* excludePeer, bool reliable) {
    ENetPacket* packet = nullptr;
    {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        packet = enet_packet_create(data, size, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    }
    if (!packet) return;

    bool sent = false;
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& [id, client] : clients) {
        if (client.peer != excludePeer) {
            {
                std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
                enet_peer_send(client.peer, reliable ? CHANNEL_RELIABLE : CHANNEL_UNRELIABLE, packet);
            }
            sent = true;
        }
    }

    if (!sent) {
        std::lock_guard<std::mutex> enetLock(NetworkManager::enetMutex);
        enet_packet_destroy(packet);
    }
}