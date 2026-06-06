#include "menu.h"
#include "../window/window.h"
#include "../../tools/logger/logger.h"
#include "../network/network_manager.h"
#include "settings.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <filesystem>

#ifdef ERROR
#undef ERROR
#endif

namespace fs = std::filesystem;

Menu::Menu() : uiShader(nullptr), textRenderer(nullptr), quadVAO(0), quadVBO(0), currentScreen(Screen::Main), cursorTimer(0.0f), scrollOffset(0.0f), lastLoadingFrameTime(0.0f) {
    seedInputText = "";
    ipInputText = "127.0.0.1:54545";
    selectedSaveName = "";
}

Menu::~Menu() {
    delete uiShader;
    delete textRenderer;
    if (quadVAO != 0) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO != 0) glDeleteBuffers(1, &quadVBO);
}

std::vector<std::string> Menu::discoverShaders() {
    std::vector<std::string> packs = { "default" };
    try {
        if (fs::exists("shaders")) {
            for (const auto& entry : fs::directory_iterator("shaders")) {
                if (entry.is_directory()) {
                    std::string dirName = entry.path().filename().string();
                    if (fs::exists(entry.path() / "vertex.glsl") ||
                        fs::exists(entry.path() / "fragment.glsl")) {
                        packs.push_back(dirName);
                    }
                }
            }
        }
    }
    catch (...) {}
    return packs;
}

void Menu::loadSaves() {
    saveList.clear();
    try {
        if (fs::exists("saves")) {
            for (const auto& entry : fs::directory_iterator("saves")) {
                if (entry.is_directory()) {
                    saveList.push_back(entry.path().filename().string());
                }
            }
        }
    }
    catch (...) {}

    if (!saveList.empty()) {
        // Устанавливаем первое сохранение выбранным по умолчанию, если старое сбросилось
        if (selectedSaveName.empty() || std::find(saveList.begin(), saveList.end(), selectedSaveName) == saveList.end()) {
            selectedSaveName = saveList[0];
        }
    }
    else {
        selectedSaveName = "";
    }
}

bool Menu::init() {
    uiShader = new Shader("shaders/ui_vertex.glsl", "shaders/ui_fragment.glsl");

    textRenderer = new TextRenderer();
    if (!textRenderer->init("fonts/uiglobal.ttf", 24)) {
        Logger::log(Logger::Level::WARNING, "Could not load fonts/uiglobal.ttf. Trying fonts/uiglobal.otf...");
        if (!textRenderer->init("fonts/uiglobal.otf", 24)) {
            Logger::log(Logger::Level::ERROR, "Failed to load any font from fonts/ directory!");
            return false;
        }
    }

    float vertices[] = {
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glm::vec4 normal = glm::vec4(0.43f, 0.43f, 0.43f, 1.0f);
    glm::vec4 hover = glm::vec4(0.58f, 0.58f, 0.58f, 1.0f);
    glm::vec4 pressed = glm::vec4(0.28f, 0.28f, 0.28f, 1.0f);
    glm::vec4 disabled = glm::vec4(0.20f, 0.20f, 0.20f, 1.0f);

    // Главное меню
    Button btnSingle;
    btnSingle.text = "Singleplayer";
    btnSingle.normalColor = normal; btnSingle.hoverColor = hover; btnSingle.pressedColor = pressed; btnSingle.disabledColor = disabled;
    btnSingle.isEnabled = true; btnSingle.isHovered = false; btnSingle.isPressed = false;
    mainButtons.push_back(btnSingle);

    Button btnMulti;
    btnMulti.text = "Multiplayer";
    btnMulti.normalColor = normal; btnMulti.hoverColor = hover; btnMulti.pressedColor = pressed; btnMulti.disabledColor = disabled;
    btnMulti.isEnabled = true; btnMulti.isHovered = false; btnMulti.isPressed = false;
    mainButtons.push_back(btnMulti);

    Button btnSettings;
    btnSettings.text = "Settings";
    btnSettings.normalColor = normal; btnSettings.hoverColor = hover; btnSettings.pressedColor = pressed; btnSettings.disabledColor = disabled;
    btnSettings.isEnabled = true; btnSettings.isHovered = false; btnSettings.isPressed = false;
    mainButtons.push_back(btnSettings);

    Button btnQuit;
    btnQuit.text = "Quit Game";
    btnQuit.normalColor = normal; btnQuit.hoverColor = hover; btnQuit.pressedColor = pressed; btnQuit.disabledColor = disabled;
    btnQuit.isEnabled = true; btnQuit.isHovered = false; btnQuit.isPressed = false;
    mainButtons.push_back(btnQuit);

    // Одиночная игра (Список сохранений)
    Button btnPlay;
    btnPlay.text = "Play World";
    btnPlay.normalColor = normal; btnPlay.hoverColor = hover; btnPlay.pressedColor = pressed; btnPlay.disabledColor = disabled;
    btnPlay.isEnabled = false; btnPlay.isHovered = false; btnPlay.isPressed = false;
    singleplayerButtons.push_back(btnPlay);

    Button btnNewWorld;
    btnNewWorld.text = "New World";
    btnNewWorld.normalColor = normal; btnNewWorld.hoverColor = hover; btnNewWorld.pressedColor = pressed; btnNewWorld.disabledColor = disabled;
    btnNewWorld.isEnabled = true; btnNewWorld.isHovered = false; btnNewWorld.isPressed = false;
    singleplayerButtons.push_back(btnNewWorld);

    Button btnDelete;
    btnDelete.text = "Delete World";
    btnDelete.normalColor = normal; btnDelete.hoverColor = hover; btnDelete.pressedColor = pressed; btnDelete.disabledColor = disabled;
    btnDelete.isEnabled = false; btnDelete.isHovered = false; btnDelete.isPressed = false;
    singleplayerButtons.push_back(btnDelete);

    Button btnBack;
    btnBack.text = "Back";
    btnBack.normalColor = normal; btnBack.hoverColor = hover; btnBack.pressedColor = pressed; btnBack.disabledColor = disabled;
    btnBack.isEnabled = true; btnBack.isHovered = false; btnBack.isPressed = false;
    singleplayerButtons.push_back(btnBack);

    // Экран создания мира
    Button btnCreate;
    btnCreate.text = "Create World";
    btnCreate.normalColor = normal; btnCreate.hoverColor = hover; btnCreate.pressedColor = pressed; btnCreate.disabledColor = disabled;
    btnCreate.isEnabled = true; btnCreate.isHovered = false; btnCreate.isPressed = false;
    createWorldButtons.push_back(btnCreate);

    Button btnCreateBack;
    btnCreateBack.text = "Back";
    btnCreateBack.normalColor = normal; btnCreateBack.hoverColor = hover; btnCreateBack.pressedColor = pressed; btnCreateBack.disabledColor = disabled;
    btnCreateBack.isEnabled = true; btnCreateBack.isHovered = false; btnCreateBack.isPressed = false;
    createWorldButtons.push_back(btnCreateBack);

    Button btnWorldType; // Новая кнопка типа мира
    btnWorldType.text = "World Type: Normal";
    btnWorldType.normalColor = normal; btnWorldType.hoverColor = hover; btnWorldType.pressedColor = pressed; btnWorldType.disabledColor = disabled;
    btnWorldType.isEnabled = true; btnWorldType.isHovered = false; btnWorldType.isPressed = false;
    createWorldButtons.push_back(btnWorldType);

    // Мультиплеер (LAN)
    Button btnConnect;
    btnConnect.text = "Connect";
    btnConnect.normalColor = normal; btnConnect.hoverColor = hover; btnConnect.pressedColor = pressed; btnConnect.disabledColor = disabled;
    btnConnect.isEnabled = true; btnConnect.isHovered = false; btnConnect.isPressed = false;
    multiplayerButtons.push_back(btnConnect);

    Button btnMultiBack;
    btnMultiBack.text = "Back";
    btnMultiBack.normalColor = normal; btnMultiBack.hoverColor = hover; btnMultiBack.pressedColor = pressed; btnMultiBack.disabledColor = disabled;
    btnMultiBack.isEnabled = true; btnMultiBack.isHovered = false; btnMultiBack.isPressed = false;
    multiplayerButtons.push_back(btnMultiBack);

    // Меню паузы
    Button btnOpenLAN;
    btnOpenLAN.text = "Open to LAN";
    btnOpenLAN.normalColor = normal; btnOpenLAN.hoverColor = hover; btnOpenLAN.pressedColor = pressed; btnOpenLAN.disabledColor = disabled;
    btnOpenLAN.isEnabled = true; btnOpenLAN.isHovered = false; btnOpenLAN.isPressed = false;
    pauseButtons.push_back(btnOpenLAN);

    Button btnPauseSettings;
    btnPauseSettings.text = "Settings";
    btnPauseSettings.normalColor = normal; btnPauseSettings.hoverColor = hover; btnPauseSettings.pressedColor = pressed; btnPauseSettings.disabledColor = disabled;
    btnPauseSettings.isEnabled = true; btnPauseSettings.isHovered = false; btnPauseSettings.isPressed = false;
    pauseButtons.push_back(btnPauseSettings);

    Button btnToMainMenu;
    btnToMainMenu.text = "Quit to Main Menu";
    btnToMainMenu.normalColor = normal; btnToMainMenu.hoverColor = hover; btnToMainMenu.pressedColor = pressed; btnToMainMenu.disabledColor = disabled;
    btnToMainMenu.isEnabled = true; btnToMainMenu.isHovered = false; btnToMainMenu.isPressed = false;
    pauseButtons.push_back(btnToMainMenu);

    // Меню настроек
    Button btnRenderDist;
    btnRenderDist.text = "Render Distance: 12 Chunks";
    btnRenderDist.normalColor = normal; btnRenderDist.hoverColor = hover; btnRenderDist.pressedColor = pressed; btnRenderDist.disabledColor = disabled;
    btnRenderDist.isEnabled = true; btnRenderDist.isHovered = false; btnRenderDist.isPressed = false;
    settingsButtons.push_back(btnRenderDist);

    Button btnBrightness;
    btnBrightness.text = "Brightness: 100%";
    btnBrightness.normalColor = normal; btnBrightness.hoverColor = hover; btnBrightness.pressedColor = pressed; btnBrightness.disabledColor = disabled;
    btnBrightness.isEnabled = true; btnBrightness.isHovered = false; btnBrightness.isPressed = false;
    settingsButtons.push_back(btnBrightness);

    Button btnFOV;
    btnFOV.text = "FOV: 70";
    btnFOV.normalColor = normal; btnFOV.hoverColor = hover; btnFOV.pressedColor = pressed; btnFOV.disabledColor = disabled;
    btnFOV.isEnabled = true; btnFOV.isHovered = false; btnFOV.isPressed = false;
    settingsButtons.push_back(btnFOV);

    Button btnRTX;
    btnRTX.text = "RTX (Raytracing): OFF";
    btnRTX.normalColor = normal; btnRTX.hoverColor = hover; btnRTX.pressedColor = pressed; btnRTX.disabledColor = disabled;
    btnRTX.isEnabled = true; btnRTX.isHovered = false; btnRTX.isPressed = false;
    settingsButtons.push_back(btnRTX);

    Button btnRTXQuality;
    btnRTXQuality.text = "RTX Quality: Medium";
    btnRTXQuality.normalColor = normal; btnRTXQuality.hoverColor = hover; btnRTXQuality.pressedColor = pressed; btnRTXQuality.disabledColor = disabled;
    btnRTXQuality.isEnabled = true; btnRTXQuality.isHovered = false; btnRTXQuality.isPressed = false;
    settingsButtons.push_back(btnRTXQuality);

    Button btnShaders;
    btnShaders.text = "Shaders...";
    btnShaders.normalColor = normal; btnShaders.hoverColor = hover; btnShaders.pressedColor = pressed; btnShaders.disabledColor = disabled;
    btnShaders.isEnabled = true; btnShaders.isHovered = false; btnShaders.isPressed = false;
    settingsButtons.push_back(btnShaders);

    Button btnSettingsBack;
    btnSettingsBack.text = "Save & Back";
    btnSettingsBack.normalColor = normal; btnSettingsBack.hoverColor = hover; btnSettingsBack.pressedColor = pressed; btnSettingsBack.disabledColor = disabled;
    btnSettingsBack.isEnabled = true; btnSettingsBack.isHovered = false; btnSettingsBack.isPressed = false;
    settingsButtons.push_back(btnSettingsBack);

    // Выбор шейдеров
    Button btnShaderBack;
    btnShaderBack.text = "Back";
    btnShaderBack.normalColor = normal; btnShaderBack.hoverColor = hover; btnShaderBack.pressedColor = pressed; btnShaderBack.disabledColor = disabled;
    btnShaderBack.isEnabled = true; btnShaderBack.isHovered = false; btnShaderBack.isPressed = false;
    shaderSelectButtons.push_back(btnShaderBack);

    currentScreen = Screen::Main;
    return true;
}

void Menu::update(float deltaTime) {
    if (currentScreen == Screen::None) return;

    cursorTimer += deltaTime;

    double mouseX, mouseY;
    glfwGetCursorPos(Window::handle, &mouseX, &mouseY);
    float glMouseX = static_cast<float>(mouseX);
    float glMouseY = static_cast<float>(Window::height) - static_cast<float>(mouseY);

    float guiScale = std::max(1.0f, std::round(static_cast<float>(Window::height) / 240.0f)) * 0.75f;
    if (guiScale < 1.0f) guiScale = 1.0f;

    float btnWidth = 320.0f * (guiScale / 3.0f);
    float btnHeight = 44.0f * (guiScale / 3.0f);
    float centerX = (static_cast<float>(Window::width) - btnWidth) / 2.0f;

    std::vector<Button>* activeButtons = nullptr;

    if (currentScreen == Screen::Main) {
        float startY = static_cast<float>(Window::height) / 2.0f + 60.0f * (guiScale / 3.0f);
        for (size_t i = 0; i < mainButtons.size(); ++i) {
            mainButtons[i].x = centerX;
            mainButtons[i].y = startY - i * 60.0f * (guiScale / 3.0f);
            mainButtons[i].width = btnWidth;
            mainButtons[i].height = btnHeight;
        }
        activeButtons = &mainButtons;
    }
    else if (currentScreen == Screen::Singleplayer) {
        // Окно сохранений в центре
        float boxW = 500.0f * (guiScale / 3.0f);
        float boxH = 200.0f * (guiScale / 3.0f);
        float boxX = (static_cast<float>(Window::width) - boxW) / 2.0f;
        float boxY = (static_cast<float>(Window::height) - boxH) / 2.0f + 30.0f * (guiScale / 3.0f);

        float halfBtnW = 240.0f * (guiScale / 3.0f);
        float itemBtnH = 40.0f * (guiScale / 3.0f);

        // Кнопка Play (слева сверху)
        singleplayerButtons[0].x = boxX;
        singleplayerButtons[0].y = boxY - 50.0f * (guiScale / 3.0f);
        singleplayerButtons[0].width = halfBtnW;
        singleplayerButtons[0].height = itemBtnH;
        singleplayerButtons[0].isEnabled = !selectedSaveName.empty();

        // Кнопка New World (справа сверху)
        singleplayerButtons[1].x = boxX + boxW - halfBtnW;
        singleplayerButtons[1].y = boxY - 50.0f * (guiScale / 3.0f);
        singleplayerButtons[1].width = halfBtnW;
        singleplayerButtons[1].height = itemBtnH;

        // Кнопка Delete (слева снизу)
        singleplayerButtons[2].x = boxX;
        singleplayerButtons[2].y = boxY - 100.0f * (guiScale / 3.0f);
        singleplayerButtons[2].width = halfBtnW;
        singleplayerButtons[2].height = itemBtnH;
        singleplayerButtons[2].isEnabled = !selectedSaveName.empty();

        // Кнопка Back (справа снизу)
        singleplayerButtons[3].x = boxX + boxW - halfBtnW;
        singleplayerButtons[3].y = boxY - 100.0f * (guiScale / 3.0f);
        singleplayerButtons[3].width = halfBtnW;
        singleplayerButtons[3].height = itemBtnH;

        activeButtons = &singleplayerButtons;
    }
    else if (currentScreen == Screen::CreateWorld) {
        // Отрисовка кнопки типа мира
        createWorldButtons[2].x = centerX;
        createWorldButtons[2].y = 150.0f * (guiScale / 3.0f);
        createWorldButtons[2].width = btnWidth;
        createWorldButtons[2].height = btnHeight;
        createWorldButtons[2].text = isFlatWorldSelected ? "World Type: Flat" : "World Type: Normal";

        createWorldButtons[0].x = centerX;
        createWorldButtons[0].y = 90.0f * (guiScale / 3.0f);
        createWorldButtons[0].width = btnWidth;
        createWorldButtons[0].height = btnHeight;

        createWorldButtons[1].x = centerX;
        createWorldButtons[1].y = 30.0f * (guiScale / 3.0f);
        createWorldButtons[1].width = btnWidth;
        createWorldButtons[1].height = btnHeight;

        activeButtons = &createWorldButtons;
    }
    else if (currentScreen == Screen::Multiplayer) {
        multiplayerButtons[0].x = centerX;
        multiplayerButtons[0].y = 100.0f * (guiScale / 3.0f);
        multiplayerButtons[0].width = btnWidth;
        multiplayerButtons[0].height = btnHeight;

        multiplayerButtons[1].x = centerX;
        multiplayerButtons[1].y = 40.0f * (guiScale / 3.0f);
        multiplayerButtons[1].width = btnWidth;
        multiplayerButtons[1].height = btnHeight;

        activeButtons = &multiplayerButtons;
    }
    else if (currentScreen == Screen::Pause) {
        float startY = static_cast<float>(Window::height) / 2.0f + 60.0f * (guiScale / 3.0f);
        for (size_t i = 0; i < pauseButtons.size(); ++i) {
            pauseButtons[i].x = centerX;
            pauseButtons[i].y = startY - i * 60.0f * (guiScale / 3.0f);
            pauseButtons[i].width = btnWidth;
            pauseButtons[i].height = btnHeight;
        }
        activeButtons = &pauseButtons;
    }
    else if (currentScreen == Screen::Settings) {
        float startY = static_cast<float>(Window::height) / 2.0f + 140.0f * (guiScale / 3.0f);
        for (size_t i = 0; i < settingsButtons.size(); ++i) {
            settingsButtons[i].x = centerX;
            settingsButtons[i].y = startY - i * 44.0f * (guiScale / 3.0f);
            settingsButtons[i].width = btnWidth;
            settingsButtons[i].height = btnHeight;
        }

        settingsButtons[0].text = "Render Distance: " + std::to_string(g_Settings.renderDistance) + " Chunks";
        settingsButtons[1].text = "Brightness: " + std::to_string(static_cast<int>(g_Settings.brightness * 100.0f)) + "%";
        settingsButtons[2].text = "FOV: " + std::to_string(static_cast<int>(g_Settings.fov));
        settingsButtons[3].text = std::string("RTX (Raytracing): ") + (g_Settings.rtxEnabled ? "ON" : "OFF");

        std::string rtxQStr = "Medium";
        if (g_Settings.rtxQuality == 0) rtxQStr = "Low";
        else if (g_Settings.rtxQuality == 1) rtxQStr = "Medium";
        else if (g_Settings.rtxQuality == 2) rtxQStr = "High (RTX 3060)";
        else if (g_Settings.rtxQuality == 3) rtxQStr = "Ultra (No Steps)";
        settingsButtons[4].text = "RTX Quality: " + rtxQStr;

        settingsButtons[5].text = "Shader Pack: " + g_Settings.selectedShader;

        activeButtons = &settingsButtons;
    }
    else if (currentScreen == Screen::ShaderSelect) {
        shaderSelectButtons[0].x = centerX;
        shaderSelectButtons[0].y = 50.0f * (guiScale / 3.0f);
        shaderSelectButtons[0].width = btnWidth;
        shaderSelectButtons[0].height = btnHeight;

        activeButtons = &shaderSelectButtons;
    }

    if (activeButtons) {
        for (auto& btn : *activeButtons) {
            if (!btn.isEnabled) {
                btn.isHovered = false;
                btn.isPressed = false;
                continue;
            }

            if (glMouseX >= btn.x && glMouseX <= btn.x + btn.width &&
                glMouseY >= btn.y && glMouseY <= btn.y + btn.height) {
                btn.isHovered = true;
                if (glfwGetMouseButton(Window::handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                    btn.isPressed = true;
                }
                else {
                    btn.isPressed = false;
                }
            }
            else {
                btn.isHovered = false;
                btn.isPressed = false;
            }
        }
    }
}

int Menu::handleMouseClick(double xpos, double ypos, int button, int action) {
    if (currentScreen == Screen::None) return -1;

    float glMouseX = static_cast<float>(xpos);
    float glMouseY = static_cast<float>(Window::height) - static_cast<float>(ypos);

    float guiScale = std::max(1.0f, std::round(static_cast<float>(Window::height) / 240.0f)) * 0.75f;
    if (guiScale < 1.0f) guiScale = 1.0f;
    float scaleMultiplier = guiScale / 3.0f;

    // Выбор сохранений
    if (currentScreen == Screen::Singleplayer && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        float boxW = 500.0f * scaleMultiplier;
        float boxH = 200.0f * scaleMultiplier;
        float boxX = (static_cast<float>(Window::width) - boxW) / 2.0f;
        float boxY = (static_cast<float>(Window::height) - boxH) / 2.0f + 30.0f * scaleMultiplier;

        float listStartY = boxY + boxH - 30.0f * scaleMultiplier + scrollOffset;
        float itemH = 35.0f * scaleMultiplier;

        for (size_t i = 0; i < saveList.size(); ++i) {
            float itemY = listStartY - i * itemH;
            if (glMouseX >= boxX && glMouseX <= boxX + boxW &&
                glMouseY >= itemY && glMouseY <= itemY + itemH) {
                selectedSaveName = saveList[i];
                return 150 + static_cast<int>(i);
            }
        }
    }

    // Клик по серверам в списке мультиплеера
    if (currentScreen == Screen::Multiplayer && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        float listStartY = static_cast<float>(Window::height) - 180.0f * scaleMultiplier + scrollOffset;
        float itemH = 35.0f * scaleMultiplier;
        float listW = 500.0f * scaleMultiplier;
        float listX = (static_cast<float>(Window::width) - listW) / 2.0f;

        for (size_t i = 0; i < lanServers.size(); ++i) {
            float itemY = listStartY - i * itemH;
            if (glMouseX >= listX && glMouseX <= listX + listW &&
                glMouseY >= itemY && glMouseY <= itemY + itemH) {
                ipInputText = lanServers[i].ip + ":" + std::to_string(lanServers[i].port);
                return 100 + static_cast<int>(i);
            }
        }
    }

    // Клик по списку шейдеров
    if (currentScreen == Screen::ShaderSelect && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        float listStartY = static_cast<float>(Window::height) - 180.0f * scaleMultiplier + scrollOffset;
        float itemH = 35.0f * scaleMultiplier;
        float listW = 500.0f * scaleMultiplier;
        float listX = (static_cast<float>(Window::width) - listW) / 2.0f;

        for (size_t i = 0; i < shaderPacks.size(); ++i) {
            float itemY = listStartY - i * itemH;
            if (glMouseX >= listX && glMouseX <= listX + listW &&
                glMouseY >= itemY && glMouseY <= itemY + itemH) {
                g_Settings.selectedShader = shaderPacks[i];
                g_Settings.save();
                shaderReloadRequested = true;
                return 200 + static_cast<int>(i);
            }
        }
    }

    std::vector<Button>* activeButtons = nullptr;
    if (currentScreen == Screen::Main) activeButtons = &mainButtons;
    else if (currentScreen == Screen::Singleplayer) activeButtons = &singleplayerButtons;
    else if (currentScreen == Screen::CreateWorld) activeButtons = &createWorldButtons;
    else if (currentScreen == Screen::Multiplayer) activeButtons = &multiplayerButtons;
    else if (currentScreen == Screen::Pause) activeButtons = &pauseButtons;
    else if (currentScreen == Screen::Settings) activeButtons = &settingsButtons;
    else if (currentScreen == Screen::ShaderSelect) activeButtons = &shaderSelectButtons;

    if (activeButtons && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        for (size_t i = 0; i < activeButtons->size(); ++i) {
            auto& btn = (*activeButtons)[i];
            if (!btn.isEnabled) continue;

            if (glMouseX >= btn.x && glMouseX <= btn.x + btn.width &&
                glMouseY >= btn.y && glMouseY <= btn.y + btn.height) {
                btn.isPressed = false;

                if (currentScreen == Screen::CreateWorld) {
                    if (i == 2) {
                        isFlatWorldSelected = !isFlatWorldSelected; // Тоггл переключения
                    }
                    return static_cast<int>(i);
                }
                else if (currentScreen == Screen::Settings) {
                    if (i == 0) {
                        if (g_Settings.renderDistance == 2) g_Settings.renderDistance = 4;
                        else if (g_Settings.renderDistance == 4) g_Settings.renderDistance = 8;
                        else if (g_Settings.renderDistance == 8) g_Settings.renderDistance = 12;
                        else if (g_Settings.renderDistance == 12) g_Settings.renderDistance = 16;
                        else if (g_Settings.renderDistance == 16) g_Settings.renderDistance = 24;
                        else if (g_Settings.renderDistance == 24) g_Settings.renderDistance = 32;
                        else if (g_Settings.renderDistance == 32) g_Settings.renderDistance = 64;
                        else g_Settings.renderDistance = 2;
                    }
                    else if (i == 1) {
                        if (g_Settings.brightness < 0.7f) g_Settings.brightness = 0.75f;
                        else if (g_Settings.brightness < 0.9f) g_Settings.brightness = 1.0f;
                        else if (g_Settings.brightness < 1.1f) g_Settings.brightness = 1.25f;
                        else if (g_Settings.brightness < 1.4f) g_Settings.brightness = 1.5f;
                        else g_Settings.brightness = 0.5f;
                    }
                    else if (i == 2) {
                        g_Settings.fov += 10.0f;
                        if (g_Settings.fov > 110.0f) g_Settings.fov = 50.0f;
                    }
                    else if (i == 3) {
                        g_Settings.rtxEnabled = !g_Settings.rtxEnabled;
                    }
                    else if (i == 4) {
                        g_Settings.rtxQuality = (g_Settings.rtxQuality + 1) % 4;
                    }
                    else if (i == 5) {
                        shaderPacks = discoverShaders();
                        setScreen(Screen::ShaderSelect);
                    }
                    else if (i == 6) {
                        g_Settings.save();
                        setScreen(previousScreen);
                    }
                    return static_cast<int>(i);
                }

                if (currentScreen == Screen::ShaderSelect) {
                    if (i == 0) {
                        setScreen(Screen::Settings);
                    }
                    return static_cast<int>(i);
                }

                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

void Menu::handleScroll(double yoffset) {
    if (currentScreen == Screen::Multiplayer || currentScreen == Screen::ShaderSelect || currentScreen == Screen::Singleplayer) {
        scrollOffset += static_cast<float>(yoffset) * 20.0f;
        if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    }
}

void Menu::handleCharacterInput(unsigned int codepoint) {
    if (currentScreen == Screen::CreateWorld) {
        if (codepoint < 128 && seedInputText.length() < 24) {
            seedInputText += static_cast<char>(codepoint);
        }
    }
    else if (currentScreen == Screen::Multiplayer) {
        if (codepoint < 128 && ipInputText.length() < 32) {
            ipInputText += static_cast<char>(codepoint);
        }
    }
}

void Menu::handleKeyInput(int key) {
    if (currentScreen == Screen::CreateWorld) {
        if (key == GLFW_KEY_BACKSPACE) {
            if (!seedInputText.empty()) {
                seedInputText.pop_back();
            }
        }
    }
    else if (currentScreen == Screen::Multiplayer) {
        if (key == GLFW_KEY_BACKSPACE) {
            if (!ipInputText.empty()) {
                ipInputText.pop_back();
            }
        }
    }
}

void Menu::render() {
    bool isServerActive = NetworkManager::isServerRunning();
    bool isClientActive = NetworkManager::isClientConnected();

    if (currentScreen == Screen::None && !isServerActive && !isClientActive) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    float guiScale = std::max(1.0f, std::round(static_cast<float>(Window::height) / 240.0f)) * 0.75f;
    if (guiScale < 1.0f) guiScale = 1.0f;
    float scaleMultiplier = guiScale / 3.0f;

    uiShader->use();
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(Window::width), 0.0f, static_cast<float>(Window::height));
    glUniformMatrix4fv(glGetUniformLocation(uiShader->ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    if (currentScreen != Screen::None) {
        if (currentScreen == Screen::Pause) {
            drawQuad(0.0f, 0.0f, static_cast<float>(Window::width), static_cast<float>(Window::height), glm::vec4(0.0f, 0.0f, 0.0f, 0.6f));
        }
        else {
            drawQuad(0.0f, 0.0f, static_cast<float>(Window::width), static_cast<float>(Window::height), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        }

        std::string titleText;
        float titleScale = 1.5f * scaleMultiplier;
        if (currentScreen == Screen::Pause) {
            titleText = "GAME PAUSED";
        }
        else if (currentScreen == Screen::Singleplayer) {
            titleText = "SINGLEPLAYER";
        }
        else if (currentScreen == Screen::CreateWorld) {
            titleText = "NEW WORLD";
        }
        else if (currentScreen == Screen::Multiplayer) {
            titleText = "MULTIPLAYER";
        }
        else if (currentScreen == Screen::Settings) {
            titleText = "SETTINGS";
        }
        else if (currentScreen == Screen::ShaderSelect) {
            titleText = "SHADERS SYSTEM";
        }
        else {
            titleText = "STONECRAFT";
            titleScale = 2.0f * scaleMultiplier;
        }

        float titleWidth = textRenderer->getTextWidth(titleText, titleScale);
        float titleX = (static_cast<float>(Window::width) - titleWidth) / 2.0f;
        float titleY = static_cast<float>(Window::height) - 100.0f * scaleMultiplier;
        textRenderer->renderText(*uiShader, titleText, titleX, titleY, titleScale, glm::vec3(1.0f, 1.0f, 1.0f));

        if (currentScreen == Screen::Singleplayer) {
            float boxW = 500.0f * scaleMultiplier;
            float boxH = 200.0f * scaleMultiplier;
            float boxX = (static_cast<float>(Window::width) - boxW) / 2.0f;
            float boxY = (static_cast<float>(Window::height) - boxH) / 2.0f + 30.0f * scaleMultiplier;

            drawQuad(boxX - 2.0f, boxY - 2.0f, boxW + 4.0f, boxH + 4.0f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            drawQuad(boxX, boxY, boxW, boxH, glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));

            std::string subLabel = "Select World Save (Scroll Wheel):";
            textRenderer->renderText(*uiShader, subLabel, boxX, boxY + boxH + 10.0f, 0.7f * scaleMultiplier, glm::vec3(0.8f));

            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxW), static_cast<int>(boxH));

            float itemStartY = boxY + boxH - 30.0f * scaleMultiplier + scrollOffset;
            float itemH = 35.0f * scaleMultiplier;

            if (saveList.empty()) {
                textRenderer->renderText(*uiShader, "No saves found. Click 'New World'!", boxX + 20.0f * scaleMultiplier, boxY + boxH / 2.0f - 10.0f * scaleMultiplier, 0.8f * scaleMultiplier, glm::vec3(0.5f));
            }
            else {
                for (size_t i = 0; i < saveList.size(); ++i) {
                    float itemY = itemStartY - i * itemH;
                    std::string text = saveList[i];
                    if (text == selectedSaveName) {
                        text += " (Selected)";
                    }
                    textRenderer->renderText(*uiShader, text, boxX + 15.0f * scaleMultiplier, itemY, 0.8f * scaleMultiplier,
                        (saveList[i] == selectedSaveName) ? glm::vec3(1.0f, 1.0f, 0.5f) : glm::vec3(1.0f));
                }
            }
            glDisable(GL_SCISSOR_TEST);
        }
        else if (currentScreen == Screen::CreateWorld) {
            std::string label = "Enter World Seed:";
            float labelWidth = textRenderer->getTextWidth(label, 0.8f * scaleMultiplier);
            float labelX = (static_cast<float>(Window::width) - labelWidth) / 2.0f;
            textRenderer->renderText(*uiShader, label, labelX, titleY - 50.0f * scaleMultiplier, 0.8f * scaleMultiplier, glm::vec3(0.8f, 0.8f, 0.8f));

            float inputW = 400.0f * scaleMultiplier;
            float inputH = 40.0f * scaleMultiplier;
            float inputX = (static_cast<float>(Window::width) - inputW) / 2.0f;
            float inputY = titleY - 110.0f * scaleMultiplier;

            drawQuad(inputX - 2.0f, inputY - 2.0f, inputW + 4.0f, inputH + 4.0f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            drawQuad(inputX, inputY, inputW, inputH, glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));

            std::string displayText = seedInputText;
            if (displayText.empty()) {
                displayText = "Random Seed (Leave Empty)";
            }

            if (!seedInputText.empty() && std::fmod(cursorTimer, 1.0f) < 0.5f) {
                displayText += "_";
            }

            float textWidth = textRenderer->getTextWidth(displayText, 0.8f * scaleMultiplier);
            float textX = inputX + (inputW - textWidth) / 2.0f;
            float textY = inputY + (inputH - 16.0f * scaleMultiplier) / 2.0f;

            glm::vec3 textColor = seedInputText.empty() ? glm::vec3(0.5f, 0.5f, 0.5f) : glm::vec3(1.0f, 1.0f, 1.0f);
            textRenderer->renderText(*uiShader, displayText, textX, textY, 0.8f * scaleMultiplier, textColor);
        }
        else if (currentScreen == Screen::Multiplayer) {
            float boxW = 500.0f * scaleMultiplier;
            float boxH = 200.0f * scaleMultiplier;
            float boxX = (static_cast<float>(Window::width) - boxW) / 2.0f;
            float boxY = titleY - 260.0f * scaleMultiplier;

            drawQuad(boxX - 2.0f, boxY - 2.0f, boxW + 4.0f, boxH + 4.0f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            drawQuad(boxX, boxY, boxW, boxH, glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));

            std::string subLabel = "LAN Servers (Scroll Wheel):";
            textRenderer->renderText(*uiShader, subLabel, boxX, boxY + boxH + 10.0f, 0.7f * scaleMultiplier, glm::vec3(0.8f));

            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxW), static_cast<int>(boxH));

            float itemStartY = boxY + boxH - 30.0f * scaleMultiplier + scrollOffset;
            float itemH = 35.0f * scaleMultiplier;

            if (lanServers.empty()) {
                textRenderer->renderText(*uiShader, "Searching for local servers...", boxX + 20.0f * scaleMultiplier, boxY + boxH / 2.0f - 10.0f * scaleMultiplier, 0.8f * scaleMultiplier, glm::vec3(0.5f));
            }
            else {
                for (size_t i = 0; i < lanServers.size(); ++i) {
                    float itemY = itemStartY - i * itemH;
                    std::string text = lanServers[i].name + " [" + lanServers[i].ip + ":" + std::to_string(lanServers[i].port) + "]";
                    textRenderer->renderText(*uiShader, text, boxX + 15.0f * scaleMultiplier, itemY, 0.8f * scaleMultiplier, glm::vec3(1.0f));
                }
            }
            glDisable(GL_SCISSOR_TEST);

            std::string ipLabel = "IP Address / Port:";
            textRenderer->renderText(*uiShader, ipLabel, boxX, boxY - 35.0f * scaleMultiplier, 0.7f * scaleMultiplier, glm::vec3(0.8f));

            float inputW = 400.0f * scaleMultiplier;
            float inputH = 40.0f * scaleMultiplier;
            float inputX = (static_cast<float>(Window::width) - inputW) / 2.0f;
            float inputY = boxY - 80.0f * scaleMultiplier;

            drawQuad(inputX - 2.0f, inputY - 2.0f, inputW + 4.0f, inputH + 4.0f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            drawQuad(inputX, inputY, inputW, inputH, glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));

            std::string dispIP = ipInputText;
            if (std::fmod(cursorTimer, 1.0f) < 0.5f) dispIP += "_";

            float ipW = textRenderer->getTextWidth(dispIP, 0.8f * scaleMultiplier);
            textRenderer->renderText(*uiShader, dispIP, inputX + (inputW - ipW) / 2.0f, inputY + (inputH - 16.0f * scaleMultiplier) / 2.0f, 0.8f * scaleMultiplier, glm::vec3(1.0f, 1.0f, 1.0f));
        }
        else if (currentScreen == Screen::ShaderSelect) {
            float boxW = 500.0f * scaleMultiplier;
            float boxH = 200.0f * scaleMultiplier;
            float boxX = (static_cast<float>(Window::width) - boxW) / 2.0f;
            float boxY = titleY - 260.0f * scaleMultiplier;

            drawQuad(boxX - 2.0f, boxY - 2.0f, boxW + 4.0f, boxH + 4.0f, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            drawQuad(boxX, boxY, boxW, boxH, glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));

            std::string subLabel = "Shader Packs (Scroll Wheel):";
            textRenderer->renderText(*uiShader, subLabel, boxX, boxY + boxH + 10.0f, 0.7f * scaleMultiplier, glm::vec3(0.8f));

            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxW), static_cast<int>(boxH));

            float itemStartY = boxY + boxH - 30.0f * scaleMultiplier + scrollOffset;
            float itemH = 35.0f * scaleMultiplier;

            if (shaderPacks.empty()) {
                textRenderer->renderText(*uiShader, "No shader packs found...", boxX + 20.0f * scaleMultiplier, boxY + boxH / 2.0f - 10.0f * scaleMultiplier, 0.8f * scaleMultiplier, glm::vec3(0.5f));
            }
            else {
                for (size_t i = 0; i < shaderPacks.size(); ++i) {
                    float itemY = itemStartY - i * itemH;
                    std::string text = shaderPacks[i];
                    if (text == g_Settings.selectedShader) {
                        text += " (Selected)";
                    }
                    textRenderer->renderText(*uiShader, text, boxX + 15.0f * scaleMultiplier, itemY, 0.8f * scaleMultiplier,
                        (shaderPacks[i] == g_Settings.selectedShader) ? glm::vec3(1.0f, 1.0f, 0.5f) : glm::vec3(1.0f));
                }
            }
            glDisable(GL_SCISSOR_TEST);
        }

        std::vector<Button>* activeButtons = nullptr;
        if (currentScreen == Screen::Main) activeButtons = &mainButtons;
        else if (currentScreen == Screen::Singleplayer) activeButtons = &singleplayerButtons;
        else if (currentScreen == Screen::CreateWorld) activeButtons = &createWorldButtons;
        else if (currentScreen == Screen::Multiplayer) activeButtons = &multiplayerButtons;
        else if (currentScreen == Screen::Pause) activeButtons = &pauseButtons;
        else if (currentScreen == Screen::Settings) activeButtons = &settingsButtons;
        else if (currentScreen == Screen::ShaderSelect) activeButtons = &shaderSelectButtons;

        if (activeButtons) {
            for (const auto& btn : *activeButtons) {
                glm::vec4 color;
                glm::vec3 textColor;

                if (!btn.isEnabled) {
                    color = btn.disabledColor;
                    textColor = glm::vec3(0.4f, 0.4f, 0.4f);
                }
                else {
                    if (btn.isPressed) color = btn.pressedColor;
                    else if (btn.isHovered) color = btn.hoverColor;
                    else color = btn.normalColor;

                    textColor = btn.isHovered ? glm::vec3(1.0f, 1.0f, 0.6f) : glm::vec3(1.0f, 1.0f, 1.0f);
                }

                float border = 2.0f;
                drawQuad(btn.x - border, btn.y - border, btn.width + border * 2, btn.height + border * 2, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                drawQuad(btn.x, btn.y, btn.width, btn.height, color);

                float textWidth = textRenderer->getTextWidth(btn.text, 1.0f * scaleMultiplier);
                float textX = btn.x + (btn.width - textWidth) / 2.0f;
                float textY = btn.y + (btn.height - 14.0f * scaleMultiplier) / 2.0f;
                textRenderer->renderText(*uiShader, btn.text, textX, textY, 1.0f * scaleMultiplier, textColor);
            }
        }
    }

    if (isServerActive) {
        uint16_t port = NetworkManager::getServer()->getPort();
        std::string portText = "Local Server Port: " + std::to_string(port);
        float scale = 0.6f * scaleMultiplier;
        float textWidth = textRenderer->getTextWidth(portText, scale);
        float x = static_cast<float>(Window::width) - textWidth - 15.0f * scaleMultiplier;
        float y = static_cast<float>(Window::height) - 30.0f * scaleMultiplier;
        textRenderer->renderText(*uiShader, portText, x, y, scale, glm::vec3(1.0f, 1.0f, 1.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Menu::drawQuad(float x, float y, float width, float height, glm::vec4 color) {
    uiShader->use();
    glUniform1i(glGetUniformLocation(uiShader->ID, "isText"), 0);
    glUniform4f(glGetUniformLocation(uiShader->ID, "solidColor"), color.r, color.g, color.b, color.a);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(width, height, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(uiShader->ID, "model"), 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Menu::renderLoadingScreen(float progress, bool isMultiplayer, const std::string& stageText, bool swapBuffers) {
    float currentFrame = static_cast<float>(glfwGetTime());
    if (lastLoadingFrameTime == 0.0f) {
        lastLoadingFrameTime = currentFrame;
    }
    float dt = currentFrame - lastLoadingFrameTime;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    lastLoadingFrameTime = currentFrame;

    if (swapBuffers) {
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    uiShader->use();
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(Window::width), 0.0f, static_cast<float>(Window::height));
    glUniformMatrix4fv(glGetUniformLocation(uiShader->ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    float barWidth = static_cast<float>(Window::width) / 3.0f;
    float barHeight = 4.0f;
    float barX = (static_cast<float>(Window::width) - barWidth) / 2.0f;

    float barY = isMultiplayer ? (static_cast<float>(Window::height) / 3.0f) : (static_cast<float>(Window::height) / 2.0f);

    drawQuad(barX, barY, barWidth, barHeight, glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));

    glm::vec4 barColor = isMultiplayer ? glm::vec4(0.0f, 0.5f, 1.0f, 1.0f) : glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
    drawQuad(barX, barY, barWidth * progress, barHeight, barColor);

    float filledWidth = barWidth * progress;
    // Увеличен спавн частиц огня для плотности и густоты ("более густой огонь")
    int spawnCount = isMultiplayer ? 15 : 60;
    float cellSize = isMultiplayer ? 3.0f : 4.0f;

    const size_t MAX_FIRE_PARTICLES = 1200; // Повышен лимит частиц

    if (filledWidth > 1.0f) {
        for (int i = 0; i < spawnCount; ++i) {
            FireParticle p;
            float rawX = barX + (static_cast<float>(rand() % 1000) / 1000.0f) * filledWidth;

            p.x = std::floor(rawX / cellSize) * cellSize;
            p.y = std::floor((barY + barHeight) / cellSize) * cellSize;

            if (isMultiplayer) {
                p.vx = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 15.0f;
                p.vy = (static_cast<float>(rand() % 100) / 100.0f) * 40.0f + 10.0f;
                p.maxLife = 0.2f + (static_cast<float>(rand() % 100) / 100.0f) * 0.2f;
            }
            else {
                p.vx = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 40.0f;
                p.vy = (static_cast<float>(rand() % 100) / 100.0f) * 180.0f + 60.0f;
                p.maxLife = 0.6f + (static_cast<float>(rand() % 100) / 100.0f) * 0.6f;
            }
            p.life = p.maxLife;
            p.color = glm::vec4(1.0f);

            if (fireParticles.size() >= MAX_FIRE_PARTICLES) {
                fireParticles.erase(fireParticles.begin());
            }
            fireParticles.push_back(p);
        }
    }

    std::vector<FireParticle> aliveParticles;
    aliveParticles.reserve(fireParticles.size());

    for (auto& p : fireParticles) {
        p.life -= dt;
        if (p.life <= 0.0f) continue;

        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vx += (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 20.0f * dt;

        float ratio = p.life / p.maxLife;
        if (isMultiplayer) {
            if (ratio > 0.7f) {
                float t = (ratio - 0.7f) / 0.3f;
                p.color = glm::vec4(glm::mix(glm::vec3(0.0f, 0.5f, 1.0f), glm::vec3(0.7f, 1.0f, 1.0f), t), 1.0f);
            }
            else if (ratio > 0.3f) {
                float t = (ratio - 0.3f) / 0.4f;
                p.color = glm::vec4(glm::mix(glm::vec3(0.3f, 0.0f, 0.8f), glm::vec3(0.0f, 0.5f, 1.0f), t), 1.0f);
            }
            else {
                float t = ratio / 0.3f;
                p.color = glm::vec4(glm::mix(glm::vec3(0.05f, 0.05f, 0.3f), glm::vec3(0.3f, 0.0f, 0.8f), t), 1.0f);
            }
        }
        else {
            if (ratio > 0.7f) {
                float t = (ratio - 0.7f) / 0.3f;
                p.color = glm::vec4(glm::mix(glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(1.0f, 1.0f, 0.7f), t), 1.0f);
            }
            else if (ratio > 0.3f) {
                float t = (ratio - 0.3f) / 0.4f;
                p.color = glm::vec4(glm::mix(glm::vec3(0.8f, 0.1f, 0.0f), glm::vec3(1.0f, 0.5f, 0.0f), t), 1.0f);
            }
            else {
                float t = ratio / 0.3f;
                p.color = glm::vec4(glm::mix(glm::vec3(0.15f, 0.15f, 0.15f), glm::vec3(0.8f, 0.1f, 0.0f), t), 1.0f);
            }
        }

        float renderX = std::floor(p.x / cellSize) * cellSize;
        float renderY = std::floor(p.y / cellSize) * cellSize;
        drawQuad(renderX, renderY, cellSize, cellSize, p.color);

        aliveParticles.push_back(p);
    }
    fireParticles = std::move(aliveParticles);

    float textScale = 0.8f;
    float textWidth = textRenderer->getTextWidth(stageText, textScale);
    float textX = (static_cast<float>(Window::width) - textWidth) / 2.0f;
    float textY = barY + 15.0f;
    textRenderer->renderText(*uiShader, stageText, textX, textY, textScale, glm::vec3(1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    if (swapBuffers) {
        glfwSwapBuffers(Window::handle);
        glfwPollEvents();
    }
}

// Отрисовка синего огонька в левом нижнем углу HUD при фоновом сохранении мира
void Menu::renderSavingIndicator(float dt) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    uiShader->use();
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(Window::width), 0.0f, static_cast<float>(Window::height));
    glUniformMatrix4fv(glGetUniformLocation(uiShader->ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    float guiScale = std::max(1.0f, std::round(static_cast<float>(Window::height) / 240.0f)) * 0.75f;
    if (guiScale < 1.0f) guiScale = 1.0f;
    float scaleMultiplier = guiScale / 3.0f;

    float startX = 20.0f * scaleMultiplier;
    float startY = 20.0f * scaleMultiplier;
    float cellSize = 3.0f * scaleMultiplier;

    // Рождаем синие частицы в точке спавна индикатора
    int spawnCount = 4;
    for (int i = 0; i < spawnCount; ++i) {
        FireParticle p;
        p.x = startX + (static_cast<float>(rand() % 100) / 100.0f) * 60.0f * scaleMultiplier;
        p.y = startY;
        p.vx = (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 15.0f * scaleMultiplier;
        p.vy = (static_cast<float>(rand() % 100) / 100.0f) * 45.0f * scaleMultiplier + 15.0f;
        p.maxLife = 0.4f + (static_cast<float>(rand() % 100) / 100.0f) * 0.4f;
        p.life = p.maxLife;
        p.color = glm::vec4(0.0f, 0.5f, 1.0f, 1.0f);
        savingParticles.push_back(p);
    }

    std::vector<FireParticle> aliveParticles;
    for (auto& p : savingParticles) {
        p.life -= dt;
        if (p.life <= 0.0f) continue;

        p.x += p.vx * dt;
        p.y += p.vy * dt;

        float ratio = p.life / p.maxLife;
        if (ratio > 0.7f) {
            float t = (ratio - 0.7f) / 0.3f;
            p.color = glm::vec4(glm::mix(glm::vec3(0.0f, 0.5f, 1.0f), glm::vec3(0.7f, 1.0f, 1.0f), t), 1.0f);
        }
        else if (ratio > 0.3f) {
            float t = (ratio - 0.3f) / 0.4f;
            p.color = glm::vec4(glm::mix(glm::vec3(0.3f, 0.0f, 0.8f), glm::vec3(0.0f, 0.5f, 1.0f), t), 1.0f);
        }
        else {
            float t = ratio / 0.3f;
            p.color = glm::vec4(glm::mix(glm::vec3(0.05f, 0.05f, 0.3f), glm::vec3(0.3f, 0.0f, 0.8f), t), 1.0f);
        }

        drawQuad(p.x, p.y, cellSize, cellSize, p.color);
        aliveParticles.push_back(p);
    }
    savingParticles = std::move(aliveParticles);

    std::string text = "Saving world...";
    float textScale = 0.6f * scaleMultiplier;
    textRenderer->renderText(*uiShader, text, startX + 70.0f * scaleMultiplier, startY + 5.0f * scaleMultiplier, textScale, glm::vec3(0.2f, 0.6f, 1.0f));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}