#pragma once

#include <enet/enet.h>
#include <string>

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool initialize();
    bool connect(const std::string& host, uint16_t port);
    void disconnect();
    void run();
    void send(const std::string& message);

private:
    ENetHost* client;
    ENetPeer* peer;
    bool connected;
};

