#include "logger.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

std::ofstream Logger::logFile;
std::mutex Logger::logMutex; // Инициализация мьютекса

bool Logger::init() {
    std::lock_guard<std::mutex> lock(logMutex);
    try {
        if (!fs::exists("logs")) {
            fs::create_directory("logs");
        }
        if (!fs::exists("logs/history")) {
            fs::create_directory("logs/history");
        }

        if (fs::exists("logs/latest.log")) {
            rotateLogs();
        }

        logFile.open("logs/latest.log", std::ios::out | std::ios::trunc);
        if (!logFile.is_open()) {
            std::cerr << "[ERROR] Failed to create logs/latest.log" << std::endl;
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Logger initialization failed: " << e.what() << std::endl;
        return false;
    }
    return true;
}

void Logger::log(Level level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex); // Защищаем вывод во всех потоках
    std::string levelStr = levelToString(level);
    std::string formatted = "[" + levelStr + "] " + message;

    std::cout << formatted << std::endl;

    if (logFile.is_open()) {
        logFile << formatted << std::endl;
    }
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        logFile.close();
    }
}

std::string Logger::levelToString(Level level) {
    switch (level) {
    case Level::INFO:    return "INFO";
    case Level::WARNING: return "WARNING";
    case Level::ERROR:   return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::rotateLogs() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");

    std::string archiveName = "logs/history/log_" + ss.str() + ".log";
    fs::rename("logs/latest.log", archiveName);

    std::vector<fs::directory_entry> archiveFiles;
    for (const auto& entry : fs::directory_iterator("logs/history")) {
        if (entry.is_regular_file()) {
            archiveFiles.push_back(entry);
        }
    }

    if (archiveFiles.size() > 10) {
        std::sort(archiveFiles.begin(), archiveFiles.end(), [](const auto& a, const auto& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
            });

        size_t toDelete = archiveFiles.size() - 10;
        for (size_t i = 0; i < toDelete; ++i) {
            fs::remove(archiveFiles[i].path());
        }
    }
}