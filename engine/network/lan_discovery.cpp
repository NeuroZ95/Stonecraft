#include "lan_discovery.h"
#include "protocol.h"
#include "../../tools/logger/logger.h"
#include <cstring>
#include <chrono>
#include <algorithm>

#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

LANDiscovery::LANDiscovery() : running(false), broadcastHostPort(0) {}

LANDiscovery::~LANDiscovery() {
    stop();
}

LANDiscovery& LANDiscovery::getInstance() {
    static LANDiscovery instance;
    return instance;
}

void LANDiscovery::start(uint16_t broadcastPort) {
    if (running.load()) return;
    running.store(true);
    broadcastHostPort.store(broadcastPort);

    listenerThread = std::thread(&LANDiscovery::runListener, this);
    broadcasterThread = std::thread(&LANDiscovery::runBroadcaster, this);
}

void LANDiscovery::stop() {
    if (!running.load()) return;
    running.store(false);

    if (listenerThread.joinable()) listenerThread.join();
    if (broadcasterThread.joinable()) broadcasterThread.join();

    std::lock_guard<std::mutex> lock(discoveryMutex);
    discoveredServers.clear();
}

void LANDiscovery::setBroadcastPort(uint16_t port) {
    broadcastHostPort.store(port);
}

std::vector<LANServer> LANDiscovery::getDiscoveredServers() {
    std::lock_guard<std::mutex> lock(discoveryMutex);
    double currentTime = glfwGetTime();
    discoveredServers.erase(
        std::remove_if(discoveredServers.begin(), discoveredServers.end(),
            [currentTime](const LANServer& s) { return (currentTime - s.lastSeen > 5.0); }),
        discoveredServers.end()
    );
    return discoveredServers;
}

void LANDiscovery::runListener() {
    SOCKET recvSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvSock == INVALID_SOCKET) {
        return;
    }

    int opt = 1;
    setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

#ifdef _WIN32
    DWORD timeout = 1000;
    setsockopt(recvSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(recvSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(54546);

    if (bind(recvSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(recvSock);
        return;
    }

    char buffer[256];
    while (running.load()) {
        sockaddr_in senderAddr;
        socklen_t senderLen = sizeof(senderAddr);
        int bytes = recvfrom(recvSock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&senderAddr, &senderLen);
        if (bytes >= (int)sizeof(PacketLANDiscovery)) {
            PacketLANDiscovery* p = (PacketLANDiscovery*)buffer;
            if (p->header.type == PACKET_LAN_DISCOVERY) {
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, sizeof(ipStr));

                std::lock_guard<std::mutex> lock(discoveryMutex);
                bool found = false;
                for (auto& s : discoveredServers) {
                    if (s.ip == ipStr && s.port == p->port) {
                        s.lastSeen = glfwGetTime();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    LANServer s;
                    s.ip = ipStr;
                    s.port = p->port;

                    char safeName[33];
                    std::memset(safeName, 0, sizeof(safeName));
                    std::strncpy(safeName, p->serverName, 32);
                    safeName[32] = '\0';
                    s.name = safeName;

                    s.lastSeen = glfwGetTime();
                    discoveredServers.push_back(s);
                }
            }
        }
    }
    closesocket(recvSock);
}

void LANDiscovery::runBroadcaster() {
    SOCKET sendSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSock == INVALID_SOCKET) return;

    int broadcastOpt = 1;
    setsockopt(sendSock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcastOpt, sizeof(broadcastOpt));

    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_port = htons(54546);

    while (running.load()) {
        uint16_t port = broadcastHostPort.load();
        if (port > 0) {
            PacketLANDiscovery p;
            p.header.type = PACKET_LAN_DISCOVERY;
            p.port = port;
            std::memset(p.serverName, 0, sizeof(p.serverName));
            std::strncpy(p.serverName, "Stonecraft Local Server", sizeof(p.serverName) - 1);

            sendto(sendSock, (const char*)&p, sizeof(p), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    closesocket(sendSock);
}