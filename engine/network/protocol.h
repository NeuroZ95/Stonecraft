#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

// Каналы передачи данных ENet
enum NetworkChannel {
    CHANNEL_RELIABLE = 0, // Надежный канал (для блоков, подключения)
    CHANNEL_UNRELIABLE,   // Ненадежный канал (для позиций игроков)
    CHANNEL_COUNT
};

// Типы сетевых сообщений
enum PacketType : uint8_t {
    PACKET_C2S_JOIN = 0,
    PACKET_S2C_JOIN_ACK,
    PACKET_C2S_PLAYER_POSITION,
    PACKET_S2C_PLAYER_POSITION,
    PACKET_C2S_BLOCK_CHANGE,
    PACKET_S2C_BLOCK_CHANGE,
    PACKET_S2C_PLAYER_LEAVE,
    PACKET_LAN_DISCOVERY,
    PACKET_S2C_CHUNK_DATA
};

#pragma pack(push, 1)

// Простые скалярные типы без скрытого выравнивания и директив компилятора
struct NetVec3 {
    float x;
    float y;
    float z;
};

struct NetIVec3 {
    int32_t x;
    int32_t y;
    int32_t z;
};

struct PacketHeader {
    uint8_t type;
};

struct PacketC2SJoin {
    PacketHeader header;
    char username[32];
};

struct PacketS2CJoinAck {
    PacketHeader header;
    uint32_t playerID;
    NetVec3 spawnPos;
    uint32_t seed;
    uint8_t worldType; // 0 = Default, 1 = Flat
};

struct PacketC2SPlayerPosition {
    PacketHeader header;
    NetVec3 position;
    float yaw;
    float pitch;
    uint8_t flags;
};

struct PacketS2CPlayerPosition {
    PacketHeader header;
    uint32_t playerID;
    NetVec3 position;
    float yaw;
    float pitch;
    uint8_t flags;
};

struct PacketC2SBlockChange {
    PacketHeader header;
    NetIVec3 pos;
    uint8_t blockType;
};

struct PacketS2CBlockChange {
    PacketHeader header;
    NetIVec3 pos;
    uint8_t blockType;
};

struct PacketS2CPlayerLeave {
    PacketHeader header;
    uint32_t playerID;
};

struct PacketLANDiscovery {
    PacketHeader header;
    uint16_t port;
    char serverName[32];
};

// Изменено на компактный заголовок для сжатых данных
struct PacketS2CChunkDataHeader {
    PacketHeader header;
    int32_t cx;
    int32_t cz;
    uint16_t compressedSize;
};

#pragma pack(pop)

// Простейшее RLE-сжатие: пары [кол-во, тип_блока]
inline std::vector<uint8_t> compressRLE(const uint8_t* data, size_t size) {
    std::vector<uint8_t> compressed;
    if (size == 0) return compressed;

    uint8_t currentVal = data[0];
    uint8_t count = 1;

    for (size_t i = 1; i < size; ++i) {
        if (data[i] == currentVal && count < 255) {
            count++;
        }
        else {
            compressed.push_back(count);
            compressed.push_back(currentVal);
            currentVal = data[i];
            count = 1;
        }
    }
    compressed.push_back(count);
    compressed.push_back(currentVal);

    return compressed;
}

// RLE-распаковка с защитой от выхода за границы памяти
inline bool decompressRLE(const uint8_t* compressed, size_t compressedSize, uint8_t* decompressed, size_t expectedSize) {
    size_t decompressedIdx = 0;
    for (size_t i = 0; i < compressedSize; i += 2) {
        if (i + 1 >= compressedSize) return false;
        uint8_t count = compressed[i];
        uint8_t val = compressed[i + 1];

        if (decompressedIdx + count > expectedSize) return false;

        std::memset(decompressed + decompressedIdx, val, count);
        decompressedIdx += count;
    }
    return decompressedIdx == expectedSize;
}