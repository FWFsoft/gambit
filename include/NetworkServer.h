#pragma once

#include <enet/enet.h>
#include <string>
#include <vector>
#include <cstdint>

class NetworkServer {
public:
    NetworkServer(const std::string& address, uint16_t port);
    ~NetworkServer();

    bool initialize();
    void run();
    void poll();  // Non-blocking event processing
    void stop();

    void broadcastPacket(const std::vector<uint8_t>& data);
    void send(ENetPeer* peer, const std::vector<uint8_t>& data);

private:
    ENetAddress address;
    ENetHost* server;
    bool running;
};

