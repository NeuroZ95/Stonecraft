#include "game.h"
#include "../window/window.h"
#include "../../tools/logger/logger.h"
#include "../../tools/texture_generator/generator.h"
#include "../network/network_manager.h"
#include "../network/lan_discovery.h"
#include "../world/save_manager.h"
#include "../world/world_utils.h"
#include "settings.h"

#ifdef ERROR
#undef ERROR
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <thread>

// Определение глобального указателя для корректной линковки с world.cpp
Menu* g_MenuInstance = nullptr;

Game::Game() :
    player(glm::vec3(0.0f, 40.0f, 0.0f)),
    ourShader(nullptr),
    selectionShader(nullptr),
    pShader(0), pVAO(0), pVBO(0),
    sunShader(0), sunVAO(0), sunVBO(0),
    blockAtlas(nullptr),
    crosshair(nullptr), // Безопасно инициализируем указатель нулевым значением
    deltaTime(0.0f), lastFrame(0.0f),
    f11PressedLastFrame(false), escPressedLastFrame(false),
    uiLeftMousePressed(false), leftMousePressed(false), rightMousePressed(false)
{
}

Game::~Game() {
    cleanup();
}

bool Game::init() {
    g_Settings.load();

    if (!Logger::init()) {
        return false;
    }

    Logger::log(Logger::Level::INFO, "Starting Stonecraft engine...");

    if (!NetworkManager::init()) {
        Logger::log(Logger::Level::ERROR, "Network initialization failed!");
        Logger::close();
        return false;
    }

    if (Window::init(1280, 720, "Stonecraft") != 0) {
        Logger::log(Logger::Level::ERROR, "Window initialization failed!");
        NetworkManager::cleanup();
        Logger::close();
        return false;
    }

    // Регистрация маршрутизаторов callback-функций
    glfwSetWindowUserPointer(Window::handle, this);
    glfwSetCharCallback(Window::handle, charCallback);
    glfwSetKeyCallback(Window::handle, keyCallback);
    glfwSetScrollCallback(Window::handle, scrollCallback);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    loadActiveShader(g_Settings.selectedShader);
    selectionShader = new Shader("shaders/vertex.glsl", "shaders/hud_fragment.glsl");

    // Отложенная инициализация Crosshair ПОСЛЕ настройки контекста OpenGL и GLEW
    crosshair = new Crosshair();

    // Инициализация ресурсов игрока и солнца
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

    pShader = glCreateProgram();
    glAttachShader(pShader, pVertex);
    glAttachShader(pShader, pFragment);
    glLinkProgram(pShader);
    glDeleteShader(pVertex);
    glDeleteShader(pFragment);

    float playerCubeVertices[] = {
        -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,

        -0.125f, -0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
        -0.125f,  0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
         0.125f,  0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
         0.125f,  0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
         0.125f, -0.125f, -0.125f,  1.0f, 1.0f, 1.0f,
        -0.125f, -0.125f, -0.125f,  1.0f, 1.0f, 1.0f,

        -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
        -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,

         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,

         -0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f,  0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f,  0.125f, -0.125f,  1.0f, 0.41f, 0.71f,

         -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
          0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f,
          0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
          0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f, -0.125f,  0.125f,  1.0f, 0.41f, 0.71f,
         -0.125f, -0.125f, -0.125f,  1.0f, 0.41f, 0.71f
    };

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

    sunShader = glCreateProgram();
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
    blockFiles[BLOCK_OAK_LEAVES] = "oak_leaves.png"; // Регистрируем текстуру для листьев дуба

    blockAtlas = new Texture("textures/blocks", blockFiles);

    selection.init();
    if (!menu.init()) {
        Logger::log(Logger::Level::ERROR, "Failed to initialize menu UI!");
    }
    if (!hud.init()) {
        Logger::log(Logger::Level::ERROR, "Failed to initialize Hud UI!");
    }

    // Связываем глобальный указатель с созданным экземпляром меню для внешних модулей
    g_MenuInstance = &menu;

    glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    Window::isCursorLocked = false;

    TextureGenerator::initTexturesFolder();

    // Запуск фонового прослушивания сети
    LANDiscovery::getInstance().start(0);

    return true;
}

void Game::run() {
    lastFrame = glfwGetTime();

    while (!Window::shouldClose()) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        update(deltaTime);
        render(deltaTime);

        Window::update();
    }
}

void Game::update(float deltaTime) {
    if (menu.shaderReloadRequested) {
        loadActiveShader(g_Settings.selectedShader);
        menu.shaderReloadRequested = false;
    }

    // Синхронизация обнаруженных LAN-серверов в меню
    menu.lanServers = LANDiscovery::getInstance().getDiscoveredServers();

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

    // Разрешаем обработку игрового окружения, когда открыт экран Respawn
    bool inGame = (menu.getScreen() == Menu::Screen::None || menu.getScreen() == Menu::Screen::Pause || menu.getScreen() == Menu::Screen::Respawn) && !isMultiplayerLoading;

    // Контроль ESC для входа / выхода из меню паузы
    bool escPressed = (glfwGetKey(Window::handle, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    if (escPressed && !escPressedLastFrame) {
        if (menu.getScreen() == Menu::Screen::None && !isMultiplayerLoading) {
            menu.setScreen(Menu::Screen::Pause);
            glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            Window::isCursorLocked = false;
            SaveManager::getInstance().triggerWorldSave(world, player);
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
                    menu.loadSaves();
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

                        glm::vec3 savedPos(0.0f, 40.0f, 0.0f);
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
                            player.isGrounded = true;
                            player.highestY = player.position.y;
                            player.health = player.maxHealth;
                            player.regenTimer = 0.0f;
                        }
                        else {
                            Logger::log(Logger::Level::WARNING, "World save details not found. Creating default...");
                            world.worldSeed = 1337;
                            world.worldType = 0;
                            world.regenerate(1337, 0.0f, 0.0f, true);
                            WorldUtils::resetPlayerToSpawn(player, world);
                        }

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
                        menu.loadSaves();
                    }
                }
                else if (clickedBtn == 3) { // Back
                    menu.setScreen(Menu::Screen::Main);
                }
            }
            else if (menu.getScreen() == Menu::Screen::CreateWorld) {
                if (clickedBtn == 0) { // Create World
                    Logger::log(Logger::Level::INFO, "Generating new world...");

                    unsigned int targetSeed = WorldUtils::parseSeed(menu.getSeedInput());
                    Logger::log(Logger::Level::INFO, "Selected seed: " + std::to_string(targetSeed));

                    std::string targetSave = "World_" + std::to_string(targetSeed);
                    int attempt = 1;
                    while (std::filesystem::exists("saves/" + targetSave)) {
                        targetSave = "World_" + std::to_string(targetSeed) + "_" + std::to_string(attempt++);
                    }

                    world.activeSaveName = targetSave;
                    world.worldSeed = targetSeed;
                    world.worldType = menu.isFlatWorld() ? 1 : 0;
                    world.isMultiplayer = false;

                    world.regenerate(targetSeed, 0.0f, 0.0f, true);
                    WorldUtils::resetPlayerToSpawn(player, world);

                    world.saveWorldData(targetSave, player.position, player.camera.yaw, player.camera.pitch, player.selectedSlot, player.hotbar);

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
                    world.regenerate(1337, 0.0f, 0.0f, false);
                    WorldUtils::resetPlayerToSpawn(player, world);

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
                if (clickedBtn == 0) { // Open to LAN
                    if (!NetworkManager::isServerRunning()) {
                        if (NetworkManager::startLocalServer(0)) {
                            uint16_t boundPort = NetworkManager::getServer()->getPort();
                            LANDiscovery::getInstance().setBroadcastPort(boundPort);
                        }
                    }
                }
                else if (clickedBtn == 1) { // Settings
                    menu.previousScreen = Menu::Screen::Pause;
                    menu.setScreen(Menu::Screen::Settings);
                }
                else if (clickedBtn == 2) { // Quit to Main Menu
                    LANDiscovery::getInstance().setBroadcastPort(0);
                    NetworkManager::disconnectFromServer();
                    NetworkManager::stopLocalServer();

                    if (!world.isMultiplayer && !world.activeSaveName.empty()) {
                        SaveManager::getInstance().triggerWorldSave(world, player);
                        SaveManager::getInstance().waitTillSaved();
                    }
                    world.activeSaveName = "";

                    menu.setScreen(Menu::Screen::Main);
                    glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    Window::isCursorLocked = false;
                }
            }
            else if (menu.getScreen() == Menu::Screen::Respawn) {
                if (clickedBtn == 0) { // Нажатие кнопки возрождения
                    player.isDead = false;
                    player.deathTime = 0.0f;
                    player.camera.deathRollOffset = 0.0f;

                    // Возрождаем игрока в мире
                    WorldUtils::resetPlayerToSpawn(player, world);
                    player.health = player.maxHealth;
                    player.highestY = player.position.y;
                    player.regenTimer = 0.0f;

                    leftMousePressed = false;
                    rightMousePressed = false;

                    menu.setScreen(Menu::Screen::None);
                    glfwSetInputMode(Window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    Window::isCursorLocked = true;
                    player.firstMouse = true;
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

    // Изменение заголовка окна
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
}

void Game::render(float deltaTime) {
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

    // Переменная inGame теперь включает в себя экран Respawn, чтобы мир продолжал отрисовываться под ним
    bool inGame = (menu.getScreen() == Menu::Screen::None || menu.getScreen() == Menu::Screen::Pause || menu.getScreen() == Menu::Screen::Respawn) && !isMultiplayerLoading;

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
        blockAtlas->bind(0);

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

        // Передаем фактор смерти для пост-обработки в шейдере мира
        float deathFactor = 0.0f;
        if (player.isDead) {
            float t = player.deathTime;
            deathFactor = t * (2.0f - t); // Плавный квадратичный ease-out
        }
        glUniform1f(glGetUniformLocation(ourShader->ID, "deathFactor"), deathFactor);

        float rtxMaxDistance = 50.0f;
        if (g_Settings.rtxQuality == 0) rtxMaxDistance = 32.0f;
        else if (g_Settings.rtxQuality == 1) rtxMaxDistance = 64.0f;
        else if (g_Settings.rtxQuality == 2) rtxMaxDistance = 128.0f;
        else if (g_Settings.rtxQuality == 3) rtxMaxDistance = 200.0f;
        glUniform1f(glGetUniformLocation(ourShader->ID, "rtxMaxDistance"), rtxMaxDistance);

        glUniform3fv(glGetUniformLocation(ourShader->ID, "cameraPos"), 1, glm::value_ptr(player.camera.position));

        glm::mat4 model = glm::mat4(1.0f);
        // Запрашиваем матрицу вида с эффектом тряски в относительных координатах
        glm::mat4 view = player.camera.getViewMatrix(true);
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

        // Отрисовка солнца
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

        // Отрисовка мира
        ourShader->use();
        world.render(player.camera.position, ourShader->ID);

        // Отрисовка удаленных игроков
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

        // Рэйкастинг взгляда игрока
        RaycastResult lookAt = world.raycast(player.camera.position, player.camera.front, 6.0f);

        if (lookAt.hit && menu.getScreen() == Menu::Screen::None) {
            selection.render(glm::vec3(lookAt.blockPos), player.camera.position, view, projection, selectionShader->ID);

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
            if (crosshair != nullptr) {
                crosshair->render(static_cast<float>(Window::width), static_cast<float>(Window::height));
            }
            hud.render(player, *blockAtlas, static_cast<float>(Window::width), static_cast<float>(Window::height));
        }
    }

    if (isMultiplayerLoading) {
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        menu.renderLoadingScreen(loadingProgress, true, loadingStage, false);
    }

    menu.render();

    // Визуализация индикатора сохранения мира в фоне
    if (SaveManager::getInstance().isSaving()) {
        menu.renderSavingIndicator(deltaTime);
    }
}

void Game::loadActiveShader(const std::string& shaderName) {
    if (ourShader != nullptr) {
        delete ourShader;
        ourShader = nullptr;
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
    ourShader = new Shader(vertPath.c_str(), fragPath.c_str());
}

void Game::cleanup() {
    LANDiscovery::getInstance().stop();

    if (!world.isMultiplayer && !world.activeSaveName.empty()) {
        Logger::log(Logger::Level::INFO, "Auto-saving active world on game exit...");
        SaveManager::getInstance().waitTillSaved();
        world.saveWorldData(world.activeSaveName, player.position, player.camera.yaw, player.camera.pitch, player.selectedSlot, player.hotbar);
    }

    // Сброс глобального указателя
    g_MenuInstance = nullptr;

    glDeleteVertexArrays(1, &pVAO);
    glDeleteBuffers(1, &pVBO);
    glDeleteProgram(pShader);

    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteProgram(sunShader);

    delete ourShader;
    delete selectionShader;
    delete blockAtlas;
    delete crosshair; // Безопасно очищаем динамическую память прицела

    Logger::log(Logger::Level::INFO, "Stopping Stonecraft engine...");
    NetworkManager::cleanup();
    Logger::close();
    Window::terminate();
}

void Game::handleCharacterInput(unsigned int codepoint) {
    menu.handleCharacterInput(codepoint);
}

void Game::handleKeyInput(int key, int scancode, int action, int mods) {
    if (menu.getScreen() != Menu::Screen::None) {
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            menu.handleKeyInput(key);
        }
    }
    else {
        if (action == GLFW_PRESS) {
            if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
                player.selectedSlot = key - GLFW_KEY_1;
            }
        }
    }
}

void Game::handleScroll(double yoffset) {
    if (menu.getScreen() != Menu::Screen::None) {
        menu.handleScroll(yoffset);
    }
    else {
        int slot = player.selectedSlot;
        if (yoffset > 0) {
            slot = (slot - 1 + 9) % 9;
        }
        else if (yoffset < 0) {
            slot = (slot + 1) % 9;
        }
        player.selectedSlot = slot;
    }
}

void Game::charCallback(GLFWwindow* window, unsigned int codepoint) {
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game) game->handleCharacterInput(codepoint);
}

void Game::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game) game->handleKeyInput(key, scancode, action, mods);
}

void Game::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    if (game) game->handleScroll(yoffset);
}