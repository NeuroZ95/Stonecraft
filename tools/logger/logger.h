#pragma once
#include <string>
#include <fstream>
#include <mutex>

#ifdef ERROR
#undef ERROR
#endif

class Logger {
public:
    enum class Level {
        INFO,
        WARNING,
        ERROR
    };

    static bool init();
    static void log(Level level, const std::string& message);
    static void close();

private:
    static std::ofstream logFile;
    static std::mutex logMutex; // Мьютекс для защиты потоков ввода-вывода
    static void rotateLogs();
    static std::string levelToString(Level level);
};