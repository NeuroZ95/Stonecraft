#pragma once
#include <vector>
#include <numeric>
#include <random>
#include <algorithm>
#include <cmath>

class PerlinNoise {
public:
    unsigned int seed; // Изменено на public для обращения при генерации семян

    PerlinNoise(unsigned int seed = 1337) : seed(seed) {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(p.begin(), p.end(), engine);
        p.insert(p.end(), p.begin(), p.end());
    }

    // 3D шум Перлина
    float noise(float x, float y, float z) const {
        int X = static_cast<int>(std::floor(x)) & 255;
        int Y = static_cast<int>(std::floor(y)) & 255;
        int Z = static_cast<int>(std::floor(z)) & 255;

        x -= std::floor(x);
        y -= std::floor(y);
        z -= std::floor(z);

        float u = fade(x);
        float v = fade(y);
        float w = fade(z);

        int A = p[X] + Y;
        int AA = p[A & 255] + Z;
        int AB = p[(A + 1) & 255] + Z;
        int B = p[(X + 1) & 255] + Y;
        int BA = p[B & 255] + Z;
        int BB = p[(B + 1) & 255] + Z;

        return lerp(w, lerp(v, lerp(u, grad(p[AA & 255], x, y, z),
            grad(p[BA & 255], x - 1, y, z)),
            lerp(u, grad(p[AB & 255], x, y - 1, z),
                grad(p[BB & 255], x - 1, y - 1, z))),
            lerp(v, lerp(u, grad(p[(AA + 1) & 255], x, y, z - 1),
                grad(p[(BA + 1) & 255], x - 1, y, z - 1)),
                lerp(u, grad(p[(AB + 1) & 255], x, y - 1, z - 1),
                    grad(p[(BB + 1) & 255], x - 1, y - 1, z - 1))));
    }

    // 2D шум Перлина
    float noise(float x, float y) const {
        return noise(x, y, 0.0f);
    }

private:
    std::vector<int> p;

    float fade(float t) const {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }

    float lerp(float t, float a, float b) const {
        return a + t * (b - a);
    }

    float grad(int hash, float x, float y, float z) const {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14) ? x : z;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
};