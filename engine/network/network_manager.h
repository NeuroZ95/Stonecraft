#pragma once
#include "server.h"
#include "client.h"
#include <mutex> // Необходимый заголовок для синхронизации

class NetworkManager {
public:
    static bool init();
    static void cleanup();

    static bool startLocalServer(uint16_t port = 54545, uint32_t seed = 1337, int worldType = 0);
    static void stopLocalServer();

    static bool connectToServer(const std::string& address, uint16_t port = 54545);
    static void disconnectFromServer();

    static void update(World& world);

    static NetworkServer* getServer() { return server; }
    static NetworkClient* getClient() { return client; }

    static bool isServerRunning() { return server != nullptr && server->isRunning(); }
    static bool isClientConnected() { return client != nullptr && client->isConnected(); }

    // Глобальный мьютекс для защиты ВСЕХ вызовов ENet в разных потоках
    static std::mutex enetMutex;

private:
    static NetworkServer* server;
    static NetworkClient* client;
    static bool initialized;
};