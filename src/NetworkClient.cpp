#include "NetworkClient.h"
#include "Logger.h"

NetworkClient::NetworkClient()
    : client(nullptr), peer(nullptr), connected(false) {}

NetworkClient::~NetworkClient() {
    if (client) {
        enet_host_destroy(client);
    }
    enet_deinitialize();
}

bool NetworkClient::initialize() {
    if (enet_initialize() != 0) {
        Logger::error("An error occurred while initializing ENet.");
        return false;
    }

    client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!client) {
        Logger::error("An error occurred while trying to create an ENet client host.");
        enet_deinitialize();
        return false;
    }

    return true;
}

bool NetworkClient::connect(const std::string& host, uint16_t port) {
    ENetAddress address;
    enet_address_set_host(&address, host.c_str());
    address.port = port;

    peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        Logger::error("No available peers for initiating an ENet connection.");
        return false;
    }

    ENetEvent event;
    if (enet_host_service(client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        Logger::info("Connected to " + host + ":" + std::to_string(port));
        connected = true;
        return true;
    } else {
        Logger::error("Connection to " + host + ":" + std::to_string(port) + " failed.");
        enet_peer_reset(peer);
        return false;
    }
}

void NetworkClient::disconnect() {
    if (connected && peer) {
        enet_peer_disconnect(peer, 0);

        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                Logger::info("Disconnected from server.");
                connected = false;
                break;
            }
        }

        if (connected) {
            enet_peer_reset(peer);
            connected = false;
        }
    }
}

void NetworkClient::run() {
    if (!connected) return;

    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0) {
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            Logger::info("Received packet: " +
                         std::string((char*)event.packet->data, event.packet->dataLength));
            enet_packet_destroy(event.packet);
        }
    }
}

void NetworkClient::send(const std::string& message) {
    if (!connected) return;

    ENetPacket* packet = enet_packet_create(message.c_str(),
                                            message.length() + 1,
                                            ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(client);
}

