#include "network_manager.h"
#include "../../tools/logger/logger.h"

NetworkServer* NetworkManager::server = nullptr;
NetworkClient* NetworkManager::client = nullptr;
bool NetworkManager::initialized = false;
std::mutex NetworkManager::enetMutex; // Определение мьютекса в cpp-файле

bool NetworkManager::init() {
    if (initialized) return true;

    // Синхронизируем инициализацию через мьютекс
    std::lock_guard<std::mutex> lock(enetMutex);
    if (enet_initialize() != 0) {
        Logger::log(Logger::Level::ERROR, "NetworkManager: Failed to initialize ENet!");
        return false;
    }

    initialized = true;
    Logger::log(Logger::Level::INFO, "NetworkManager: Initialized ENet.");
    return true;
}

void NetworkManager::cleanup() {
    if (!initialized) return;

    stopLocalServer();
    disconnectFromServer();

    // Синхронизируем деинициализацию через мьютекс
    std::lock_guard<std::mutex> lock(enetMutex);
    enet_deinitialize();
    initialized = false;
    Logger::log(Logger::Level::INFO, "NetworkManager: Cleaned up ENet.");
}

bool NetworkManager::startLocalServer(uint16_t port, uint32_t seed, int worldType) {
    if (!initialized) return false;

    stopLocalServer();

    server = new NetworkServer();
    if (!server->start(port, seed, worldType)) {
        delete server;
        server = nullptr;
        return false;
    }

    return true;
}

void NetworkManager::stopLocalServer() {
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
}

bool NetworkManager::connectToServer(const std::string& address, uint16_t port) {
    if (!initialized) return false;

    disconnectFromServer();

    client = new NetworkClient();
    if (!client->connect(address, port)) {
        delete client;
        client = nullptr;
        return false;
    }

    return true;
}

void NetworkManager::disconnectFromServer() {
    if (client) {
        client->disconnect();
        delete client;
        client = nullptr;
    }
}

void NetworkManager::update(World& world) {
    if (client) {
        client->update(world);
    }
}