#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 FragWorldPos;

uniform sampler2D texture1;
uniform sampler2D heightmapTex;
uniform sampler3D voxels3DTex; // 3D текстура вокселей
uniform vec2 heightmapCenter;

uniform float brightness;
uniform int rtxEnabled;
uniform float deathFactor; // Юниформ коэффициента смерти

// Пределы итераций RTX
uniform float rtxMaxDistance;

// Направление солнца под углом 45 градусов
const vec3 lightDir = vec3(0.5, 0.707, 0.5);

// Точный воксельный рэйкастинг (Аналитический DDA)
float calculateVoxelShadow(vec3 worldPos) {
    // Небольшое смещение по вектору света предотвращает самозатенение плоскости
    vec3 ro = worldPos + lightDir * 0.01;
    vec3 rd = lightDir;

    // Переводим мировые координаты луча в локальные координаты сетки 512x64x512
    vec3 gridOrigin = ro - vec3(heightmapCenter.x - 256.0, 0.0, heightmapCenter.y - 256.0);

    // Целочисленные координаты текущей ячейки вокселя
    ivec3 mapPos = ivec3(floor(gridOrigin));

    // Настройка шага по осям (Аналитический метод DDA)
    vec3 deltaDist = abs(1.0 / max(abs(rd), vec3(1e-6)));
    ivec3 stepDir = ivec3(sign(rd));

    vec3 sideDist;
    sideDist.x = (rd.x > 0.0) ? (float(mapPos.x + 1) - gridOrigin.x) * deltaDist.x : (gridOrigin.x - float(mapPos.x)) * deltaDist.x;
    sideDist.y = (rd.y > 0.0) ? (float(mapPos.y + 1) - gridOrigin.y) * deltaDist.y : (gridOrigin.y - float(mapPos.y)) * deltaDist.y;
    sideDist.z = (rd.z > 0.0) ? (float(mapPos.z + 1) - gridOrigin.z) * deltaDist.z : (gridOrigin.z - float(mapPos.z)) * deltaDist.z;

    // Ограничиваем максимальное количество итераций на уровне 128 для оптимизации производительности
    int maxSteps = int(min(rtxMaxDistance * 2.0, 128.0)); 
    vec3 invVoxelSize = vec3(1.0 / 512.0, 1.0 / 64.0, 1.0 / 512.0);

    for (int i = 0; i < 128; ++i) {
        if (i >= maxSteps) break;

        // Выход за границы локальной 3D зоны отслеживания
        if (mapPos.x < 0 || mapPos.x >= 512 || mapPos.y < 0 || mapPos.y >= 64 || mapPos.z < 0 || mapPos.z >= 512) {
            break;
        }

        // Переводим координаты в диапазон [0.0, 1.0] для 3D текстуры
        vec3 texCoord = (vec3(mapPos) + 0.5) * invVoxelSize;
        
        // Использование textureLod вместо texture убирает вычисление неявных производных внутри цикла
        float blockType = textureLod(voxels3DTex, texCoord, 0.0).r * 255.0;
        int blockID = int(round(blockType));

        // Если это плотный блок (не воздух = 0 и не стекло = 4)
        if (blockID != 0 && blockID != 4) {
            return 0.35; // Быстрый выход: точка находится в тени
        }

        // Вычисляем направление шага по сетке вокселей (branchless DDA)
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) {
                sideDist.x += deltaDist.x;
                mapPos.x += stepDir.x;
            } else {
                sideDist.z += deltaDist.z;
                mapPos.z += stepDir.z;
            }
        } else {
            if (sideDist.y < sideDist.z) {
                sideDist.y += deltaDist.y;
                mapPos.y += stepDir.y;
            } else {
                sideDist.z += deltaDist.z;
                mapPos.z += stepDir.z;
            }
        }
    }

    return 1.0; // Точка освещена
}

void main() {
    vec4 texColor = texture(texture1, TexCoord);
    if(texColor.a < 0.1)
        discard;

    // Вычисляем нормаль грани на лету
    vec3 normal = normalize(cross(dFdx(FragWorldPos), dFdy(FragWorldPos)));
    
    float ambient = 0.35;
    float diffuse = max(dot(normal, lightDir), 0.0) * 0.65;
    
    float shadow = 1.0;
    if (rtxEnabled == 1) {
        shadow = calculateVoxelShadow(FragWorldPos);
    } else {
        // Простая быстрая аппроксимация теней в пещерах/под навесами (RTX OFF)
        float h = texture(heightmapTex, (FragWorldPos.xz - heightmapCenter) / 512.0 + 0.5).r * 255.0;
        if (FragWorldPos.y < h - 0.5) {
            shadow = 0.55; 
        }
    }
    
    float lighting = ambient + diffuse * shadow;
    
    vec3 finalColor = texColor.rgb * lighting * brightness;

    // Плавное наложение эффектов при смерти
    if (deathFactor > 0.0) {
        // Добавляем красный оттенок (до 60% интенсивности)
        vec3 deathRed = vec3(0.5, 0.0, 0.0);
        finalColor = mix(finalColor, deathRed, deathFactor * 0.6);
        
        // Затемняем мир (до 40% ослабления исходной яркости)
        finalColor *= (1.0 - deathFactor * 0.4);
    }
    
    FragColor = vec4(finalColor, texColor.a);
}