#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

struct Settings {
    int renderDistance = 12;
    float brightness = 1.0f;
    float fov = 110.0f;
    bool rtxEnabled = true;
    int rtxQuality = 3; // 0: Low, 1: Medium, 2: High, 3: Ultra
    std::string selectedShader = "default";

    inline void load() {
        std::ifstream file("options.txt");
        if (!file.is_open()) return;
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string key;
            if (std::getline(ss, key, '=')) {
                std::string value;
                if (std::getline(ss, value)) {
                    if (key == "renderDistance") renderDistance = std::stoi(value);
                    else if (key == "brightness") brightness = std::stof(value);
                    else if (key == "fov") fov = std::stof(value);
                    else if (key == "rtxEnabled") rtxEnabled = (value == "1" || value == "true");
                    else if (key == "rtxQuality") rtxQuality = std::stoi(value);
                    else if (key == "selectedShader") selectedShader = value;
                }
            }
        }
    }

    inline void save() {
        std::ofstream file("options.txt");
        if (!file.is_open()) return;
        file << "renderDistance=" << renderDistance << "\n";
        file << "brightness=" << brightness << "\n";
        file << "fov=" << fov << "\n";
        file << "rtxEnabled=" << (rtxEnabled ? "1" : "0") << "\n";
        file << "rtxQuality=" << rtxQuality << "\n";
        file << "selectedShader=" << selectedShader << "\n";
    }
};

inline Settings g_Settings;