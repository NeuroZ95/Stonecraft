#pragma once

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "../renderer/shader.h"
#include "../renderer/text_renderer.h"

struct Button {
    std::string text;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    glm::vec4 normalColor = glm::vec4(0.0f);
    glm::vec4 hoverColor = glm::vec4(0.0f);
    glm::vec4 pressedColor = glm::vec4(0.0f);
    glm::vec4 disabledColor = glm::vec4(0.0f);
    bool isHovered = false;
    bool isPressed = false;
    bool isEnabled = false;
};

struct LANServer {
    std::string ip;
    uint16_t port = 0;
    std::string name;
    double lastSeen = 0.0;
};

struct FireParticle {
    float x, y;
    float vx, vy;
    float life;
    float maxLife;
    glm::vec4 color;
};

class Menu {
public:
    enum class Screen {
        Main,
        Singleplayer,  // Экран со списком сохранений
        CreateWorld,   // Экран создания нового мира (ввод сида)
        Multiplayer,
        Pause,
        Settings,      // Экран настроек
        ShaderSelect,  // Экран выбора шейдеров
        None
    };

    Menu();
    ~Menu();

    bool init();
    void update(float deltaTime);
    void render();

    void renderLoadingScreen(float progress, bool isMultiplayer, const std::string& stageText, bool swapBuffers);
    void renderSavingIndicator(float dt); // Синий огонек сохранения

    int handleMouseClick(double xpos, double ypos, int button, int action);
    void handleScroll(double yoffset);

    Screen getScreen() const { return currentScreen; }
    void setScreen(Screen screen) {
        currentScreen = screen;
        if (screen == Screen::CreateWorld) {
            clearSeedInput();
        }
        else if (screen == Screen::Multiplayer) {
            clearIPInput();
        }

        if (screen == Screen::None) {
            fireParticles.clear();
            fireParticles.shrink_to_fit();
        }
    }
    bool isVisible() const { return currentScreen != Screen::None; }

    void handleCharacterInput(unsigned int codepoint);
    void handleKeyInput(int key);

    std::string getSeedInput() const { return seedInputText; }
    void clearSeedInput() { seedInputText.clear(); }

    std::string getIPInput() const { return ipInputText; }
    void clearIPInput() { ipInputText = "127.0.0.1:54545"; }

    void loadSaves(); // Сканирование сохранений

    bool isFlatWorld() const { return isFlatWorldSelected; }

    std::vector<LANServer> lanServers;
    float scrollOffset;

    // Список сохранений и выбранное
    std::vector<std::string> saveList;
    std::string selectedSaveName;

    // Сигнал внешнему коду о необходимости перезагрузки шейдеров
    bool shaderReloadRequested = false;
    Screen previousScreen = Screen::Main;

private:
    void drawQuad(float x, float y, float width, float height, glm::vec4 color);
    std::vector<std::string> discoverShaders();

    Shader* uiShader;
    TextRenderer* textRenderer;
    unsigned int quadVAO, quadVBO;
    Screen currentScreen;

    std::vector<Button> mainButtons;
    std::vector<Button> singleplayerButtons;  // Кнопки экрана сохранений
    std::vector<Button> createWorldButtons;   // Кнопки экрана ввода сида
    std::vector<Button> multiplayerButtons;
    std::vector<Button> pauseButtons;
    std::vector<Button> settingsButtons;      // Кнопки настроек
    std::vector<Button> shaderSelectButtons;  // Кнопки выбора шейдеров

    bool isFlatWorldSelected = false; // Состояние кнопки выбора типа мира
    std::string seedInputText;
    std::string ipInputText;
    float cursorTimer;

    std::vector<std::string> shaderPacks;

    std::vector<FireParticle> fireParticles;
    std::vector<FireParticle> savingParticles; // Частицы сохранения
    float lastLoadingFrameTime;
};