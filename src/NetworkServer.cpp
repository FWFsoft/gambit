#include "NetworkServer.h"

#include "EventBus.h"
#include "Logger.h"
#include "config/NetworkConfig.h"

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

  server = enet_host_create(&address, Config::Network::MAX_CLIENTS,
                            Config::Network::CHANNEL_COUNT, 0, 0);
  if (!server) {
    Logger::error(
        "An error occurred while trying to create an ENet server host.");
    enet_deinitialize();
    return false;
  }

  Logger::info("Server initialized and listening on port " +
               std::to_string(address.port));
  return true;
}

void NetworkServer::run() {
  running = true;
  ENetEvent event;

  while (running) {
    while (enet_host_service(server, &event, Config::Network::POLL_TIMEOUT_MS) >
           0) {
      switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
          uint32_t clientId =
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer));
          EventBus::instance().publish(
              ClientConnectedEvent{event.peer, clientId});
          break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
          EventBus::instance().publish(NetworkPacketReceivedEvent{
              event.peer, event.packet->data, event.packet->dataLength});
          enet_packet_destroy(event.packet);
          break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
          uint32_t clientId =
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer));
          EventBus::instance().publish(ClientDisconnectedEvent{clientId});
          break;
        }

        default:
          break;
      }
    }
  }
  Logger::info("Server shutting down.");
}

void NetworkServer::poll() {
  if (!server) return;

  ENetEvent event;
  while (enet_host_service(server, &event, 0) >
         0) {  // Non-blocking (0ms timeout)
    switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT: {
        uint32_t clientId =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer));
        EventBus::instance().publish(
            ClientConnectedEvent{event.peer, clientId});
        break;
      }

      case ENET_EVENT_TYPE_RECEIVE: {
        EventBus::instance().publish(NetworkPacketReceivedEvent{
            event.peer, event.packet->data, event.packet->dataLength});
        enet_packet_destroy(event.packet);
        break;
      }

      case ENET_EVENT_TYPE_DISCONNECT: {
        uint32_t clientId =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer));
        EventBus::instance().publish(ClientDisconnectedEvent{clientId});
        break;
      }

      default:
        break;
    }
  }
}

void NetworkServer::stop() { running = false; }

void NetworkServer::broadcastPacket(const std::vector<uint8_t>& data) {
  if (!server) return;

  ENetPacket* packet =
      enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_host_broadcast(server, 0, packet);
  enet_host_flush(server);
}

void NetworkServer::send(ENetPeer* peer, const std::vector<uint8_t>& data) {
  if (!peer) return;

  ENetPacket* packet =
      enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(peer, 0, packet);
  enet_host_flush(server);
}
