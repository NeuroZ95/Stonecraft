#include "hud.h"
#include "../window/window.h"
#include "../../tools/logger/logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

Hud::Hud() : hudShader(nullptr), slotDeactTex(nullptr), slotActTex(nullptr), heartTex(nullptr), quadVAO(0), quadVBO(0) {}

Hud::~Hud() {
    delete hudShader;
    delete slotDeactTex;
    delete slotActTex;
    delete heartTex;
    if (quadVAO != 0) glDeleteVertexArrays(1, &quadVAO);
    if (quadVBO != 0) glDeleteBuffers(1, &quadVBO);
}

bool Hud::init() {
    hudShader = new Shader("shaders/ui_vertex.glsl", "shaders/ui_fragment.glsl");
    slotDeactTex = new Texture("textures/hud/slot_deact.png");
    slotActTex = new Texture("textures/hud/slot_act.png");
    heartTex = new Texture("textures/hud/heart.png"); // Загрузка текстуры сердца

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

    return true;
}

void Hud::render(const Player& player, const Texture& blockAtlas, float screenWidth, float screenHeight) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    hudShader->use();
    glm::mat4 projection = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);
    glUniformMatrix4fv(glGetUniformLocation(hudShader->ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Расчет адаптивного масштаба интерфейса на основе высоты экрана (240px - референсная высота для х1 масштаба)
    float guiScale = std::max(1.0f, std::round(screenHeight / 240.0f));
    float slotSize = 16.0f * guiScale;
    float totalWidth = 9.0f * slotSize;

    // Центрируем хотбар
    float startX = (screenWidth - totalWidth) / 2.0f;
    float startY = 16.0f * (guiScale / 3.0f); // Адаптивный отступ снизу

    // 1. Отрисовка подложки / слотов
    for (int i = 0; i < 9; ++i) {
        float x = startX + i * slotSize;
        GLuint texID = (player.selectedSlot == i) ? slotActTex->getID() : slotDeactTex->getID();
        drawQuad(x, startY, slotSize, slotSize, glm::vec4(1.0f), texID);
    }

    // 2. Отрисовка предметов внутри слотов с адаптивным масштабом
    float itemScale = guiScale * (2.0f / 3.0f);
    float itemSize = 16.0f * itemScale;
    float padding = (slotSize - itemSize) / 2.0f;

    float segmentWidth = 1.0f / 6.0f;
    float segmentHeight = 1.0f / (float)BLOCK_TYPES_COUNT;

    for (int i = 0; i < 9; ++i) {
        block_t blockType = player.hotbar[i];
        if (blockType == BLOCK_AIR) continue;

        float x = startX + i * slotSize + padding;
        float y = startY + padding;

        int faceIndex = 2;
        float tuStart = faceIndex * segmentWidth;
        float tuEnd = tuStart + segmentWidth;
        float tvStart = blockType * segmentHeight;
        float tvEnd = tvStart + segmentHeight;

        glm::vec4 texCoords(tuStart, tvEnd, tuEnd, tvStart);
        drawQuad(x, y, itemSize, itemSize, glm::vec4(1.0f), blockAtlas.getID(), texCoords);
    }

    // 3. Отрисовка сердец режима выживания над хотбаром
    // Текстура сердца имеет размер 9x9 пикселей.
    // С учетом адаптивного масштаба интерфейса guiScale:
    // Ширина и высота одного сердца составят 9.0f * guiScale.
    // Наложение соседних сердец друг на друга на 1 пиксель означает, что шаг смещения между ними равен 8.0f * guiScale.
    float heartWidth = 9.f * 0.75 * guiScale;
    float heartHeight = 9.f * 0.75 * guiScale;
    float heartStep = 8.f * 0.75 * guiScale; // Смещение на 8 пикселей (перекрытие в 1 пиксель)

    // Размещаем на высоте 2 пикселей над хотбаром
    float heartY = startY + slotSize + (2.0f * guiScale);

    for (int i = 0; i < player.maxHealth; ++i) {
        float hx = startX + i * heartStep;

        // Если текущее здоровье меньше или равно индексу, рисуем сердце темным (потерянное здоровье)
        glm::vec4 color = (i < player.health) ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // Передаем текстурные координаты с инвертированной осью Y, чтобы компенсировать вертикальный переворот stb_image
        drawQuad(hx, heartY, heartWidth, heartHeight, color, heartTex->getID(), glm::vec4(0.0f, 1.0f, 1.0f, 0.0f));
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Hud::drawQuad(float x, float y, float width, float height, glm::vec4 color, GLuint textureID, glm::vec4 texCoords) {
    glUniform1i(glGetUniformLocation(hudShader->ID, "isText"), 0);

    // Всегда передаем цвет в шейдер для смешивания (tint)
    glUniform4f(glGetUniformLocation(hudShader->ID, "solidColor"), color.r, color.g, color.b, color.a);

    if (textureID != 0) {
        glUniform1i(glGetUniformLocation(hudShader->ID, "isTexture"), 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(glGetUniformLocation(hudShader->ID, "text"), 0);
    }
    else {
        glUniform1i(glGetUniformLocation(hudShader->ID, "isTexture"), 0);
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(width, height, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(hudShader->ID, "model"), 1, GL_FALSE, glm::value_ptr(model));

    float vertices[] = {
        0.0f, 1.0f, texCoords.x, texCoords.w,
        0.0f, 0.0f, texCoords.x, texCoords.y,
        1.0f, 0.0f, texCoords.z, texCoords.y,

        0.0f, 1.0f, texCoords.x, texCoords.w,
        1.0f, 0.0f, texCoords.z, texCoords.y,
        1.0f, 1.0f, texCoords.z, texCoords.w
    };

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}