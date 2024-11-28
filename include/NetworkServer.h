#pragma once

#include <enet/enet.h>
#include <string>

class NetworkServer {
public:
    NetworkServer(const std::string& address, uint16_t port);
    ~NetworkServer();

    bool initialize();
    void run();
    void stop();

private:
    ENetAddress address;
    ENetHost* server;
    bool running;
};

