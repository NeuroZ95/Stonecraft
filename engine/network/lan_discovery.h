#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include "../gameplay/menu.h" // Для структуры LANServer

class LANDiscovery {
public:
    static LANDiscovery& getInstance();

    void start(uint16_t broadcastPort = 0);
    void stop();

    void setBroadcastPort(uint16_t port);
    std::vector<LANServer> getDiscoveredServers();

private:
    LANDiscovery();
    ~LANDiscovery();

    void runListener();
    void runBroadcaster();

    std::atomic<bool> running;
    std::atomic<uint16_t> broadcastHostPort;
    std::mutex discoveryMutex;
    std::vector<LANServer> discoveredServers;

    std::thread listenerThread;
    std::thread broadcasterThread;
};