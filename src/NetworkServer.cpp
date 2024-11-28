#include "NetworkServer.h"
#include "Logger.h"
#include <csignal>

volatile bool keepRunning = true;

NetworkServer::NetworkServer(const std::string& addressStr, uint16_t port)
    : server(nullptr), running(false) {
    enet_address_set_host(&address, addressStr.c_str());
    address.port = port;
}

NetworkServer::~NetworkServer() {
    if (server) {
        enet_host_destroy(server);
    }
    enet_deinitialize();
}

bool NetworkServer::initialize() {
    if (enet_initialize() != 0) {
        Logger::error("An error occurred while initializing ENet.");
        return false;
    }

    server = enet_host_create(&address, 32, 2, 0, 0);
    if (!server) {
        Logger::error("An error occurred while trying to create an ENet server host.");
        enet_deinitialize();
        return false;
    }

    Logger::info("Server initialized and listening on port " + std::to_string(address.port));
    return true;
}

void signalHandler(int signum) {
    keepRunning = false;
}

void NetworkServer::run() {
    signal(SIGINT, signalHandler); // Capture Ctrl+C signal
    running = true;
    ENetEvent event;

    while (running && keepRunning) {
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    Logger::info("Client connected from " +
                                 std::to_string(event.peer->address.host) + ":" +
                                 std::to_string(event.peer->address.port));
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    Logger::info("Received packet: " +
                                 std::string((char*)event.packet->data, event.packet->dataLength));
                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    Logger::info("Client disconnected.");
                    break;

                default:
                    break;
            }
        }
    }
    Logger::info("Server shutting down.");
}

void NetworkServer::stop() {
    running = false;
}

