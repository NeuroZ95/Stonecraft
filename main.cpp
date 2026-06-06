#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cmath> 
#include <cctype>
#include <thread>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include "engine/window/window.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/crosshair.h"
#include "engine/gameplay/player.h"
#include "engine/world/world.h"
#include "tools/logger/logger.h"
#include "tools/texture_generator/generator.h"
#include "engine/world/chunk.h"
#include "engine/renderer/selection.h"
#include "engine/network/network_manager.h" 
#include "engine/gameplay/menu.h" 
#include "engine/renderer/hud.h"
#include "engine/gameplay/settings.h"

#ifdef ERROR
#undef ERROR
#endif
#include <engine/network/protocol.h>

const float SPAWN_COORD_X = 0.0f;
const float SPAWN_COORD_Z = 0.0f;

Menu* g_MenuInstance = nullptr;
static Player* g_PlayerInstance = nullptr;

std::vector<LANServer> g_DiscoveredLANServers;
std::mutex g_DiscoveryMutex;
bool g_DiscoveryRunning = true;
uint16_t g_BroadcastHostPort = 0;

// Переменные фонового сохранения
std::atomic<bool> g_IsSaving(false);

struct ChunkSaveTaskData {
    int cx, cz;
    std::vector<uint8_t> blocks;
};

// Функция записи данных на физический накопитель, исполняемая в параллельном потоке
void asyncSaveTask(std::string saveName, unsigned int seed, int worldType, glm::vec3 pPos, float pYaw, float pPitch, int pSlot, std::vector<uint8_t> pHotbar, std::map<SimpleIVec3, block_t, SimpleIVec3Less> mBlocks, std::vector<ChunkSaveTaskData> chunkData) {
    g_IsSaving = true;

    // Имитируем небольшую задержку, чтобы визуализировать синий огонёк
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    namespace fs = std::filesystem;
    fs::create_directories("saves/" + saveName + "/chunks");

    // Запись world.dat
    std::string path = "saves/" + saveName + "/world.dat";
    std::ofstream file(path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
        file.write(reinterpret_cast<const char*>(&worldType), sizeof(worldType)); // Теперь сохраняется тип мира!
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

    g_IsSaving = false;
}

// Запуск асинхронного сохранения
void triggerWorldSave(World& world, Player& player) {
    if (world.activeSaveName.empty() || world.isMultiplayer) return;
    if (g_IsSaving) return; // Уже сохраняем

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

    std::thread saveThread(asyncSaveTask,
        world.activeSaveName,
        world.worldSeed,
        world.worldType, // Передаем тип мира во вспомогательный поток
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

unsigned int parseSeed(const std::string& input) {
    if (input.empty()) {
        return static_cast<unsigned int>(glfwGetTime() * 1000.0f);
    }

    bool isNumeric = true;
    for (char c : input) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            isNumeric = false;
            break;
        }
    }

    if (isNumeric) {
        try {
            unsigned long long parsedValue = std::stoull(input);
            return static_cast<unsigned int>(parsedValue);
        }
        catch (...) {}
    }

    unsigned int calculatedHash = 0;
    for (char c : input) {
        calculatedHash = calculatedHash * 31 + static_cast<unsigned int>(c);
    }
    return calculatedHash;
}

void char_input_callback(GLFWwindow* window, unsigned int codepoint) {
    if (g_MenuInstance) {
        g_MenuInstance->handleCharacterInput(codepoint);
    }
}

void key_input_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (g_MenuInstance && g_MenuInstance->getScreen() != Menu::Screen::None) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            g_MenuInstance->handleKeyInput(key);
        }
    }
    else if (g_PlayerInstance) {
        if (action == GLFW_PRESS) {
            if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                g_PlayerInstance->selectedSlot = key - GLFW_KEY_1;
            }
        }
    }
}

void scroll_input_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (g_MenuInstance && g_MenuInstance->getScreen() != Menu::Screen::None) {
        g_MenuInstance->handleScroll(yoffset);
    }
    else if (g_PlayerInstance) {
        int slot = g_PlayerInstance->selectedSlot;
        if (yoffset > 0) {
            slot = (slot - 1 + 9) % 9;
        }
        else if (yoffset < 0) {
            slot = (slot + 1) % 9;
        }
        g_PlayerInstance->selectedSlot = slot;
    }
}

void resetPlayerToSpawn(Player& player, World& world) {
    float spawnX = SPAWN_COORD_X;
    float spawnZ = SPAWN_COORD_Z;
    float spawnY = 40.0f;

    for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
        if (world.getBlock(static_cast<int>(spawnX), y, static_cast<int>(spawnZ)) != BLOCK_AIR) {
            spawnY = static_cast<float>(y) + 1.0f + 1.62f;
            break;
        }
    }
    player.position = glm::vec3(spawnX, spawnY - 1.62f, spawnZ);
    player.velocity = glm::vec3(0.0f);
    player.isGrounded = false;
    player.camera.position = player.position + glm::vec3(0.0f, player.eyeHeight, 0.0f);
}

void runLANListener() {
    SOCKET recvSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvSock == INVALID_SOCKET) {
        return;
    }

    int opt = 1;
    setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

#ifdef _WIN32
    DWORD timeout = 1000;
    setsockopt(recvSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(recvSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(54546);

    if (bind(recvSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(recvSock);
        return;
    }

    char buffer[256];
    while (g_DiscoveryRunning) {
        sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);
        int bytes = recvfrom(recvSock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&senderAddr, &senderLen);
        if (bytes >= (int)sizeof(PacketLANDiscovery)) {
            PacketLANDiscovery* p = (PacketLANDiscovery*)buffer;
            if (p->header.type == PACKET_LAN_DISCOVERY) {
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, sizeof(ipStr));

                std::lock_guard<std::mutex> lock(g_DiscoveryMutex);
                bool found = false;
                for (auto& s : g_DiscoveredLANServers) {
                    if (s.ip == ipStr && s.port == p->port) {
                        s.lastSeen = glfwGetTime();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LANServer s;
                    s.ip = ipStr;
                    s.port = p->port;

                    char safeName[33];
                    std::memset(safeName, 0, sizeof(safeName));
                    std::strncpy(safeName, p->serverName, 32);
                    safeName[32] = '\0';
                    s.name = safeName;

                    s.lastSeen = glfwGetTime();
                    g_DiscoveredLANServers.push_back(s);
                }
            }
        }
    }
    closesocket(recvSock);
}

void runLANBroadcaster() {
    SOCKET sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSock == INVALID_SOCKET) return;

    int broadcastOpt = 1;
    setsockopt(sendSock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcastOpt, sizeof(broadcastOpt));

    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_port = htons(54546);

    while (g_DiscoveryRunning) {
        if (g_BroadcastHostPort > 0) {
            PacketLANDiscovery p;
            p.header.type = PACKET_LAN_DISCOVERY;
            p.port = g_BroadcastHostPort;
            std::memset(p.serverName, 0, sizeof(p.serverName));
            std::strncpy(p.serverName, "Stonecraft Local Server", sizeof(p.serverName) - 1);

            sendto(sendSock, (const char*)&p, sizeof(p), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    closesocket(sendSock);
}

void loadActiveShader(Shader*& shaderInstance, const std::string& shaderName) {
    if (shaderInstance != nullptr) {
        delete shaderInstance;
        shaderInstance = nullptr;
    }

    std::string vertPath = "shaders/vertex.glsl";
    std::string fragPath = "shaders/fragment.glsl";

    if (shaderName != "default" && !shaderName.empty()) {
        std::string customVert = "shaders/" + shaderName + "/vertex.glsl";
        std::string customFrag = "shaders/" + shaderName + "/fragment.glsl";
        if (std::filesystem::exists(customVert) && std::filesystem::exists(customFrag)) {
            vertPath = customVert;
            fragPath = customFrag;
        }
        else {
            Logger::log(Logger::Level::WARNING, "Shader files not found: " + shaderName + ". Using default.");
        }
    }

    Logger::log(Logger::Level::INFO, "Compiling shader pack: [" + shaderName + "]");
    shaderInstance = new Shader(vertPath.c_str(), fragPath.c_str());
}

int main() {
    g_Settings.load();

    if (!Logger::init()) {
        std::cerr << "Critical failure: Could not initialize system logger!" << std::endl;
        return -1;
    }

    Logger::log(Logger::Level::INFO, "Starting Stonecraft engine...");

    if (!NetworkManager::init()) {
        Logger::log(Logger::Level::ERROR, "Network initialization failed!");
        Logger::close();
        return -1;
    }

    if (Window::init(1280, 720, "Stonecraft") != 0) {
        Logger::log(Logger::Level::ERROR, "Window initialization failed!");
        NetworkManager::cleanup();
        Logger::close();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    Shader* ourShader = nullptr;
    loadActiveShader(ourShader, g_Settings.selectedShader);

    Shader selectionShader("shaders/vertex.glsl", "shaders/hud_fragment.glsl");

    const char* pVertexCode =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec3 aCol;\n"
        "out vec3 ourColor;\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
        "    ourColor = aCol;\n"
        "}\0";

    const char* pFragmentCode =
        "#version 330 core\n"
        "in vec3 ourColor;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    FragColor = vec4(ourColor, 1.0);\n"
        "}\0";

    unsigned int pVertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(pVertex, 1, &pVertexCode, NULL);
    glCompileShader(pVertex);

    unsigned int pFragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(pFragment, 1, &pFragmentCode, NULL);
    glCompileShader(pFragment);

    unsigned int pShader = glCreateProgram();
    glAttachShader(pShader, pVertex);
    glAttachShader(pShader, pFragment);
    glLinkProgram(pShader);
    glDeleteShader(pVertex);
    glDeleteShader(pFragment);

    float playerCubeVertices[] = {
        // Front (Z+) -> Pink (1.0, 0.41, 0.71)
        -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,

        // Back (Z-) -> White (1.0, 1.0, 1.0)
        -0.125f, -0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
        -0.125f,  0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
         0.125f,  0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
         0.125f,  0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
         0.125f, -0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
        -0.125f, -0.125f, -0.125f,  1.0f, 1.0f, 1.0f,

        // Left (X-) -> Pink
        -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,

        // Right (X+) -> Pink
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,

         // Top (Y+) -> Pink
         -0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,

         // Bottom (Y-) -> Pink
         -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
          0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
          0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f
    };

    unsigned int pVAO, pVBO;
    glGenVertexArrays(1, &pVAO);
    glGenBuffers(1, &pVBO);
    glBindVertexArray(pVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(playerCubeVertices), playerCubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    const char* sunVertexCode =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "out vec2 uv;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "uniform vec3 cameraPos;\n"
        "uniform vec3 cameraRight;\n"
        "uniform vec3 cameraUp;\n"
        "void main() {\n"
        "    vec3 sunDir = normalize(vec3(0.5, 0.707, 0.5));\n"
        "    vec3 worldPos = cameraPos + sunDir * 120.0 + cameraRight * aPos.x * 6.0 + cameraUp * aPos.y * 6.0;\n"
        "    gl_Position = projection * view * vec4(worldPos, 1.0);\n"
        "    uv = aPos.xy;\n"
        "}\0";

    const char* sunFragmentCode =
        "#version 330 core\n"
        "in vec2 uv;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    float dist = length(uv);\n"
        "    if (dist > 1.0) discard;\n"
        "    float alpha = smoothstep(1.0, 0.1, dist);\n"
        "    vec3 sunColor = mix(vec3(1.0, 1.0, 0.95), vec3(1.0, 0.85, 0.5), dist);\n"
        "    FragColor = vec4(sunColor, alpha);\n"
        "}\0";

    unsigned int sVertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(sVertex, 1, &sunVertexCode, NULL);
    glCompileShader(sVertex);

    unsigned int sFragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sFragment, 1, &sunFragmentCode, NULL);
    glCompileShader(sFragment);

    unsigned int sunShader = glCreateProgram();
    glAttachShader(sunShader, sVertex);
    glAttachShader(sunShader, sFragment);
    glLinkProgram(sunShader);
    glDeleteShader(sVertex);
    glDeleteShader(sFragment);

    float sunQuadVertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f
    };
    unsigned int sunVAO, sunVBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sunQuadVertices), sunQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::vector<std::string> blockFiles(BLOCK_TYPES_COUNT);
    blockFiles[BLOCK_AIR] = "";
    blockFiles[BLOCK_DIRT] = "dirt.png";
    blockFiles[BLOCK_GRASS] = "grass.png";
    blockFiles[BLOCK_STONE] = "stone.png";
    blockFiles[BLOCK_GLASS] = "glass.png";
    blockFiles[BLOCK_OAK_LOG] = "oak_log.png";

    Texture blockAtlas("textures/blocks", blockFiles);
    Crosshair crosshair;
    World world;

    Player player(glm::vec3(SPAWN_COORD_X, 40.0f, SPAWN_COORD_Z));
    g_PlayerInstance = &player;

    BlockSelection selection;
    selection.init();

    Menu menu;
    if (!menu.init()) {
        Logger::log(Logger::Level::ERROR, "Failed to initialize menu UI!");
    }

    Hud hud;
    if (!hud.init()) {
        Logger::log(Logger::Level::ERROR, "Failed to initialize Hud UI!");
    }

    g_MenuInstance = &menu;
    glfwSetCharCallback(Window::handle, char_input_callback);
    glfwSetKeyCallback(Window::handle, key_input_callback);
    glfwSetScrollCallback(Window::handle, scroll_input_callback);

    glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    Window::isCursorLocked = false;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    static bool f11PressedLastFrame = false;
    static bool escPressedLastFrame = false;
    static bool uiLeftMousePressed = false;

    static bool leftMousePressed = false;
    static bool rightMousePressed = false;

    TextureGenerator::initTexturesFolder();

    std::thread listenerThread(runLANListener);
    std::thread broadcasterThread(runLANBroadcaster);

    while (!Window::shouldClose()) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (menu.shaderReloadRequested) {
            loadActiveShader(ourShader, g_Settings.selectedShader);
            menu.shaderReloadRequested = false;
        }

        {
            std::lock_guard<std::mutex> lock(g_DiscoveryMutex);
            g_DiscoveredLANServers.erase(
                std::remove_if(g_DiscoveredLANServers.begin(), g_DiscoveredLANServers.end(),
                    [](const LANServer& s) { return (glfwGetTime() - s.lastSeen > 5.0); }),
                g_DiscoveredLANServers.end()
            );
            menu.lanServers = g_DiscoveredLANServers;
        }

        bool isMultiplayerLoading = false;
        float loadingProgress = 0.0f;
        std::string loadingStage = "Connecting...";
        if (NetworkManager::isClientConnected()) {
            auto client = NetworkManager::getClient();
            if (client->isMultiplayerLoading) {
                isMultiplayerLoading = true;
                loadingStage = client->loadingStageText;
                loadingProgress = static_cast<float>(client->receivedChunksCount) / client->expectedChunksCount;
                if (loadingProgress > 1.0f) loadingProgress = 1.0f;
            }
        }

        bool inGame = (menu.getScreen() == Menu::Screen::None || menu.getScreen() == Menu::Screen::Pause) && !isMultiplayerLoading;

        bool escPressed = (glfwGetKey(Window::handle, GLFW_KEY_ESCAPE) == GLFW_PRESS);
        if (escPressed && !escPressedLastFrame) {
            if (menu.getScreen() == Menu::Screen::None && !isMultiplayerLoading) {
                menu.setScreen(Menu::Screen::Pause);
                glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                Window::isCursorLocked = false;
                // Автосохранение при открытии паузы
                triggerWorldSave(world, player);
            }
            else if (menu.getScreen() == Menu::Screen::Pause) {
                menu.setScreen(Menu::Screen::None);
                glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                Window::isCursorLocked = true;
                player.firstMouse = true;
            }
            else if (menu.getScreen() == Menu::Screen::Singleplayer || menu.getScreen() == Menu::Screen::CreateWorld || menu.getScreen() == Menu::Screen::Multiplayer) {
                menu.setScreen(Menu::Screen::Main);
            }
            else if (menu.getScreen() == Menu::Screen::Settings) {
                g_Settings.save();
                menu.setScreen(menu.previousScreen);
            }
            else if (menu.getScreen() == Menu::Screen::ShaderSelect) {
                menu.setScreen(Menu::Screen::Settings);
            }
        }
        escPressedLastFrame = escPressed;

        if (glfwGetKey(Window::handle, GLFW_KEY_F11) == GLFW_PRESS) {
            if (!f11PressedLastFrame) {
                Window::toggleFullscreen();
                f11PressedLastFrame = true;
            }
        }
        else {
            f11PressedLastFrame = false;
        }

        if (isMultiplayerLoading) {
            glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            Window::isCursorLocked = false;
        }
        else if (menu.getScreen() == Menu::Screen::None && !Window::isCursorLocked) {
            glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            Window::isCursorLocked = true;
            player.firstMouse = true;
        }

        bool uiLeftMouse = (glfwGetMouseButton(Window::handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        if (menu.getScreen() != Menu::Screen::None) {
            menu.update(deltaTime);

            if (!uiLeftMouse && uiLeftMousePressed) {
                double xpos, ypos;
                glfwGetCursorPos(Window::handle, &xpos, &ypos);
                int clickedBtn = menu.handleMouseClick(xpos, ypos, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE);

                if (menu.getScreen() == Menu::Screen::Main) {
                    if (clickedBtn == 0) {
                        menu.loadSaves(); // Сканируем папки сохранений перед входом
                        menu.setScreen(Menu::Screen::Singleplayer);
                    }
                    else if (clickedBtn == 1) {
                        menu.setScreen(Menu::Screen::Multiplayer);
                    }
                    else if (clickedBtn == 2) {
                        menu.previousScreen = Menu::Screen::Main;
                        menu.setScreen(Menu::Screen::Settings);
                    }
                    else if (clickedBtn == 3) {
                        glfwSetWindowShouldClose(Window::handle, true);
                    }
                }
                else if (menu.getScreen() == Menu::Screen::Singleplayer) {
                    if (clickedBtn == 0) { // Play
                        std::string targetWorld = menu.selectedSaveName;
                        if (!targetWorld.empty()) {
                            Logger::log(Logger::Level::INFO, "Selected world save: " + targetWorld);
                            world.activeSaveName = targetWorld;
                            world.isMultiplayer = false;

                            glm::vec3 savedPos(SPAWN_COORD_X, 40.0f, SPAWN_COORD_Z);
                            float savedYaw = -90.0f;
                            float savedPitch = 0.0f;
                            int savedSlot = 0;
                            uint8_t savedHotbar[9] = { BLOCK_DIRT, BLOCK_GRASS, BLOCK_STONE, BLOCK_GLASS, BLOCK_AIR, BLOCK_AIR, BLOCK_AIR, BLOCK_AIR, BLOCK_AIR };

                            if (world.loadWorldData(targetWorld, savedPos, savedYaw, savedPitch, savedSlot, savedHotbar)) {
                                Logger::log(Logger::Level::INFO, "Loaded world.dat from " + targetWorld);
                                world.regenerate(world.worldSeed, savedPos.x, savedPos.z, true);

                                player.position = savedPos;
                                player.camera.yaw = savedYaw;
                                player.camera.pitch = savedPitch;
                                player.camera.updateVectors();
                                player.selectedSlot = savedSlot;
                                for (int i = 0; i < 9; ++i) player.hotbar[i] = savedHotbar[i];
                                player.velocity = glm::vec3(0.0f);
                                player.isGrounded = false;
                            }
                            else {
                                Logger::log(Logger::Level::WARNING, "World save details not found. Creating default...");
                                world.worldSeed = 1337;
                                world.worldType = 0;
                                world.regenerate(1337, SPAWN_COORD_X, SPAWN_COORD_Z, true);
                                resetPlayerToSpawn(player, world);
                            }

                            // Запускаем локальный сервер с правильным типом сохраненного мира!
                            if (NetworkManager::startLocalServer(54545, world.worldSeed, world.worldType)) {
                                if (NetworkManager::connectToServer("127.0.0.1", 54545)) {
                                    Logger::log(Logger::Level::INFO, "Connected.");
                                }
                                else {
                                    NetworkManager::stopLocalServer();
                                }
                            }

                            leftMousePressed = false;
                            rightMousePressed = false;

                            menu.setScreen(Menu::Screen::None);
                            glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                            Window::isCursorLocked = true;
                            player.firstMouse = true;
                        }
                    }
                    else if (clickedBtn == 1) { // New World
                        menu.setScreen(Menu::Screen::CreateWorld);
                    }
                    else if (clickedBtn == 2) { // Delete
                        std::string targetWorld = menu.selectedSaveName;
                        if (!targetWorld.empty()) {
                            Logger::log(Logger::Level::INFO, "Deleting world save: " + targetWorld);
                            std::filesystem::remove_all("saves/" + targetWorld);
                            menu.loadSaves(); // обновляем список
                        }
                    }
                    else if (clickedBtn == 3) { // Back
                        menu.setScreen(Menu::Screen::Main);
                    }
                }
                else if (menu.getScreen() == Menu::Screen::CreateWorld) {
                    if (clickedBtn == 0) { // Create World
                        Logger::log(Logger::Level::INFO, "Generating new world...");

                        unsigned int targetSeed = parseSeed(menu.getSeedInput());
                        Logger::log(Logger::Level::INFO, "Selected seed: " + std::to_string(targetSeed));

                        std::string targetSave = "World_" + std::to_string(targetSeed);
                        int attempt = 1;
                        while (std::filesystem::exists("saves/" + targetSave)) {
                            targetSave = "World_" + std::to_string(targetSeed) + "_" + std::to_string(attempt++);
                        }

                        world.activeSaveName = targetSave;
                        world.worldSeed = targetSeed;
                        world.worldType = menu.isFlatWorld() ? 1 : 0; // Сохраняем выбранный тип мира
                        world.isMultiplayer = false;

                        world.regenerate(targetSeed, SPAWN_COORD_X, SPAWN_COORD_Z, true);
                        resetPlayerToSpawn(player, world);

                        // Первичное мгновенное сохранение
                        world.saveWorldData(targetSave, player.position, player.camera.yaw, player.camera.pitch, player.selectedSlot, player.hotbar);

                        // Запускаем локальный сервер с корректным типом мира
                        if (NetworkManager::startLocalServer(54545, targetSeed, world.worldType)) {
                            if (NetworkManager::connectToServer("127.0.0.1", 54545)) {
                                Logger::log(Logger::Level::INFO, "Connected.");
                            }
                            else {
                                NetworkManager::stopLocalServer();
                            }
                        }

                        leftMousePressed = false;
                        rightMousePressed = false;

                        menu.setScreen(Menu::Screen::None);
                        glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                        Window::isCursorLocked = true;
                        player.firstMouse = true;
                    }
                    else if (clickedBtn == 1) { // Back
                        menu.setScreen(Menu::Screen::Singleplayer);
                    }
                }
                else if (menu.getScreen() == Menu::Screen::Multiplayer) {
                    if (clickedBtn == 0) {
                        std::string ipPort = menu.getIPInput();
                        size_t colon = ipPort.find(':');
                        std::string ip = "127.0.0.1";
                        uint16_t port = 54545;
                        if (colon != std::string::npos) {
                            ip = ipPort.substr(0, colon);
                            try {
                                port = static_cast<uint16_t>(std::stoi(ipPort.substr(colon + 1)));
                            }
                            catch (...) {}
                        }
                        else {
                            ip = ipPort;
                        }

                        world.isMultiplayer = true;
                        world.activeSaveName = "";
                        world.regenerate(1337, SPAWN_COORD_X, SPAWN_COORD_Z, false);
                        resetPlayerToSpawn(player, world);

                        if (NetworkManager::connectToServer(ip, port)) {
                            leftMousePressed = false;
                            rightMousePressed = false;

                            menu.setScreen(Menu::Screen::None);
                            glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                            Window::isCursorLocked = false;
                            player.firstMouse = true;
                        }
                    }
                    else if (clickedBtn == 1) {
                        menu.setScreen(Menu::Screen::Main);
                    }
                }
                else if (menu.getScreen() == Menu::Screen::Pause) {
                    if (clickedBtn == 0) {
                        if (!NetworkManager::isServerRunning()) {
                            if (NetworkManager::startLocalServer(0)) {
                                uint16_t boundPort = NetworkManager::getServer()->getPort();
                                g_BroadcastHostPort = boundPort;
                            }
                        }
                    }
                    else if (clickedBtn == 1) {
                        menu.previousScreen = Menu::Screen::Pause;
                        menu.setScreen(Menu::Screen::Settings);
                    }
                    else if (clickedBtn == 2) {
                        g_BroadcastHostPort = 0;
                        NetworkManager::disconnectFromServer();
                        NetworkManager::stopLocalServer();

                        // Сохраняем мир перед выходом и дожидаемся окончания записи во избежание коррапта файлов
                        if (!world.isMultiplayer && !world.activeSaveName.empty()) {
                            triggerWorldSave(world, player);
                            while (g_IsSaving) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }
                        world.activeSaveName = "";

                        menu.setScreen(Menu::Screen::Main);
                        glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                        Window::isCursorLocked = false;
                    }
                }
            }
            uiLeftMousePressed = uiLeftMouse;
        }
        else {
            uiLeftMousePressed = false;
            if (!isMultiplayerLoading) {
                player.update(deltaTime, world);
            }
        }

        if (inGame || isMultiplayerLoading) {
            NetworkManager::update(world);

            if (NetworkManager::isClientConnected() && !isMultiplayerLoading) {
                NetworkManager::getClient()->sendPosition(player.position, player.camera.yaw, player.camera.pitch, player.isSneaking);
            }
        }

        char windowTitle[128];
        if (inGame) {
            snprintf(windowTitle, sizeof(windowTitle),
                "Stonecraft | XYZ: %.2f, %.2f, %.2f",
                player.camera.position.x, player.camera.position.y, player.camera.position.z);
        }
        else {
            snprintf(windowTitle, sizeof(windowTitle), "Stonecraft");
        }
        glfwSetWindowTitle(Window::handle, windowTitle);

        if (inGame) {
            glClearColor(0.5f, 0.7f, 0.9f, 1.0f);
        }
        else {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (inGame) {
            ourShader->use();
            glUniform1i(glGetUniformLocation(ourShader->ID, "texture1"), 0);
            blockAtlas.bind(0);

            if (world.heightmapTexID != 0) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, world.heightmapTexID);
                glUniform1i(glGetUniformLocation(ourShader->ID, "heightmapTex"), 1);
                glUniform2f(glGetUniformLocation(ourShader->ID, "heightmapCenter"), world.heightmapCenter.x, world.heightmapCenter.y);
            }

            if (world.voxels3DTexID != 0) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_3D, world.voxels3DTexID);
                glUniform1i(glGetUniformLocation(ourShader->ID, "voxels3DTex"), 2);
                glUniform2f(glGetUniformLocation(ourShader->ID, "heightmapCenter"), world.heightmapCenter.x, world.heightmapCenter.y);
            }

            glUniform1f(glGetUniformLocation(ourShader->ID, "brightness"), g_Settings.brightness);
            glUniform1i(glGetUniformLocation(ourShader->ID, "rtxEnabled"), g_Settings.rtxEnabled ? 1 : 0);

            float rtxMaxDistance = 50.0f;
            if (g_Settings.rtxQuality == 0) {
                rtxMaxDistance = 32.0f;
            }
            else if (g_Settings.rtxQuality == 1) {
                rtxMaxDistance = 64.0f;
            }
            else if (g_Settings.rtxQuality == 2) {
                rtxMaxDistance = 128.0f;
            }
            else if (g_Settings.rtxQuality == 3) {
                rtxMaxDistance = 200.0f;
            }
            glUniform1f(glGetUniformLocation(ourShader->ID, "rtxMaxDistance"), rtxMaxDistance);

            glUniform3fv(glGetUniformLocation(ourShader->ID, "cameraPos"), 1, glm::value_ptr(player.camera.position));

            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 view = glm::lookAt(glm::vec3(0.0f), player.camera.front, player.camera.up);
            glm::mat4 projection = glm::perspective(glm::radians(g_Settings.fov), static_cast<float>(Window::width) / static_cast<float>(Window::height), 0.01f, 200.0f);

            unsigned int modelLoc = glGetUniformLocation(ourShader->ID, "model");
            unsigned int viewLoc = glGetUniformLocation(ourShader->ID, "view");
            unsigned int projLoc = glGetUniformLocation(ourShader->ID, "projection");

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

            glDisable(GL_CULL_FACE);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(sunShader);
            glUniformMatrix4fv(glGetUniformLocation(sunShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(sunShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(sunShader, "cameraPos"), 1, glm::value_ptr(player.camera.position));
            glUniform3fv(glGetUniformLocation(sunShader, "cameraRight"), 1, glm::value_ptr(player.camera.right));
            glUniform3fv(glGetUniformLocation(sunShader, "cameraUp"), 1, glm::value_ptr(player.camera.up));

            glBindVertexArray(sunVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);

            ourShader->use();
            world.render(player.camera.position, ourShader->ID);

            if (NetworkManager::isClientConnected()) {
                auto remotePlayers = NetworkManager::getClient()->getRemotePlayers();
                glUseProgram(pShader);
                glBindVertexArray(pVAO);

                for (const auto& [id, remotePlayer] : remotePlayers) {
                    float heightOffset = remotePlayer.isSneaking ? 1.45f : 1.62f;
                    glm::vec3 visualPos = remotePlayer.position + glm::vec3(0.0f, heightOffset, 0.0f);
                    glm::vec3 relativePos = visualPos - player.camera.position;

                    glm::mat4 pModel = glm::translate(glm::mat4(1.0f), relativePos);
                    pModel = glm::rotate(pModel, glm::radians(-remotePlayer.yaw - 90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    pModel = glm::rotate(pModel, glm::radians(remotePlayer.pitch), glm::vec3(1.0f, 0.0f, 0.0f));

                    glUniformMatrix4fv(glGetUniformLocation(pShader, "model"), 1, GL_FALSE, glm::value_ptr(pModel));
                    glUniformMatrix4fv(glGetUniformLocation(pShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
                    glUniformMatrix4fv(glGetUniformLocation(pShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
                glBindVertexArray(0);
            }

            RaycastResult lookAt = world.raycast(player.camera.position, player.camera.front, 6.0f);

            if (lookAt.hit && menu.getScreen() == Menu::Screen::None) {
                selection.render(glm::vec3(lookAt.blockPos), player.camera.position, view, projection, selectionShader.ID);

                if (glfwGetMouseButton(Window::handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                    if (!leftMousePressed) {
                        if (NetworkManager::isClientConnected()) {
                            NetworkManager::getClient()->sendBlockChange(lookAt.blockPos, BLOCK_AIR);
                        }
                        else {
                            world.setBlock(lookAt.blockPos, BLOCK_AIR);
                        }
                        leftMousePressed = true;
                    }
                }
                else {
                    leftMousePressed = false;
                }

                if (glfwGetMouseButton(Window::handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                    if (!rightMousePressed) {
                        float currentHeight = player.isSneaking ? 1.65f : 1.8f;
                        glm::vec3 playerMin = player.position - glm::vec3(0.3f, 0.0f, 0.3f);
                        glm::vec3 playerMax = player.position + glm::vec3(0.3f, currentHeight, 0.3f);

                        glm::vec3 newBlockMin = glm::vec3(lookAt.adjacentPos);
                        glm::vec3 newBlockMax = glm::vec3(lookAt.adjacentPos) + glm::vec3(1.0f, 1.0f, 1.0f);

                        bool collision =
                            playerMin.x < newBlockMax.x && playerMax.x > newBlockMin.x &&
                            playerMin.y < newBlockMax.y && playerMax.y > newBlockMin.y &&
                            playerMin.z < newBlockMax.z && playerMax.z > newBlockMin.z;

                        if (!collision) {
                            block_t currentBlock = player.hotbar[player.selectedSlot];
                            if (currentBlock != BLOCK_AIR) {
                                if (NetworkManager::isClientConnected()) {
                                    NetworkManager::getClient()->sendBlockChange(lookAt.adjacentPos, currentBlock);
                                }
                                else {
                                    world.setBlock(lookAt.adjacentPos, currentBlock);
                                }
                            }
                        }
                        rightMousePressed = true;
                    }
                }
                else {
                    rightMousePressed = false;
                }
            }
            else {
                leftMousePressed = false;
                rightMousePressed = false;
            }

            if (menu.getScreen() == Menu::Screen::None) {
                crosshair.render(static_cast<float>(Window::width), static_cast<float>(Window::height));
                hud.render(player, blockAtlas, static_cast<float>(Window::width), static_cast<float>(Window::height));
            }
        }

        if (isMultiplayerLoading) {
            glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            menu.renderLoadingScreen(loadingProgress, true, loadingStage, false);
        }

        menu.render();

        // Отрисовка синего огонька сохранения в нижнем левом углу
        if (g_IsSaving) {
            menu.renderSavingIndicator(deltaTime);
        }

        Window::update();
    }

    g_DiscoveryRunning = false;
    if (listenerThread.joinable()) listenerThread.join();
    if (broadcasterThread.joinable()) broadcasterThread.join();

    // Автосохранение на закрытии окна
    if (!world.isMultiplayer && !world.activeSaveName.empty()) {
        Logger::log(Logger::Level::INFO, "Auto-saving active world on game exit...");
        while (g_IsSaving) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        world.saveWorldData(world.activeSaveName, player.position, player.camera.yaw, player.camera.pitch, player.selectedSlot, player.hotbar);
    }

    glDeleteVertexArrays(1, &pVAO);
    glDeleteBuffers(1, &pVBO);
    glDeleteProgram(pShader);

    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteProgram(sunShader);

    if (ourShader != nullptr) {
        delete ourShader;
    }

    Logger::log(Logger::Level::INFO, "Stopping Stonecraft engine...");
    NetworkManager::cleanup();
    Logger::close();
    Window::terminate();
    return 0;
}