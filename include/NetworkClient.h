#pragma once

#include <enet/enet.h>
#include <string>
#include <vector>
#include <cstdint>

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool initialize();
    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    void run();
    void send(const std::string& message);
    void send(const std::vector<uint8_t>& data);  // Binary packet send

private:
    ENetHost* client;
    ENetPeer* peer;
    bool connected;
};

