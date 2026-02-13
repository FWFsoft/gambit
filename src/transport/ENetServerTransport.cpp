#include "transport/ENetServerTransport.h"

#include <cstdint>

#include "Logger.h"

ENetServerTransport::ENetServerTransport()
    : server(nullptr), nextClientId(1) {}

ENetServerTransport::~ENetServerTransport() {
  if (server != nullptr) {
    enet_host_destroy(server);
    server = nullptr;
  }
  enet_deinitialize();
}

bool ENetServerTransport::initialize(const std::string& address,
                                     uint16_t port) {
  if (enet_initialize() != 0) {
    Logger::error("Failed to initialize ENet");
    return false;
  }

  ENetAddress addr;
  enet_address_set_host(&addr, address.c_str());
  addr.port = port;

  server = enet_host_create(&addr, 32, 2, 0, 0);
  if (server == nullptr) {
    Logger::error("Failed to create ENet server host on " + address + ":" +
                   std::to_string(port));
    enet_deinitialize();
    return false;
  }

  Logger::info("ENet server transport initialized on " + address + ":" +
               std::to_string(port));
  return true;
}

bool ENetServerTransport::poll(TransportEvent& event) {
  ENetEvent enetEvent;
  int result = enet_host_service(server, &enetEvent, 0);
  if (result <= 0) {
    return false;
  }

  switch (enetEvent.type) {
    case ENET_EVENT_TYPE_CONNECT: {
      uint32_t clientId = nextClientId++;
      clientPeers[clientId] = enetEvent.peer;
      enetEvent.peer->data = (void*)(uintptr_t)clientId;

      event.type = TransportEventType::CONNECT;
      event.clientId = clientId;
      event.data.clear();

      Logger::info("Client " + std::to_string(clientId) + " connected");
      return true;
    }

    case ENET_EVENT_TYPE_RECEIVE: {
      uint32_t clientId = (uint32_t)(uintptr_t)enetEvent.peer->data;

      event.type = TransportEventType::RECEIVE;
      event.clientId = clientId;
      event.data.assign(enetEvent.packet->data,
                        enetEvent.packet->data + enetEvent.packet->dataLength);

      enet_packet_destroy(enetEvent.packet);
      return true;
    }

    case ENET_EVENT_TYPE_DISCONNECT: {
      uint32_t clientId = (uint32_t)(uintptr_t)enetEvent.peer->data;

      event.type = TransportEventType::DISCONNECT;
      event.clientId = clientId;
      event.data.clear();

      clientPeers.erase(clientId);

      Logger::info("Client " + std::to_string(clientId) + " disconnected");
      return true;
    }

    default:
      break;
  }

  return false;
}

void ENetServerTransport::send(uint32_t clientId, const uint8_t* data,
                               size_t length) {
  auto it = clientPeers.find(clientId);
  if (it == clientPeers.end()) {
    Logger::error("Cannot send to unknown client " +
                   std::to_string(clientId));
    return;
  }

  ENetPacket* packet =
      enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
  enet_peer_send(it->second, 0, packet);
  enet_host_flush(server);
}

void ENetServerTransport::broadcast(const uint8_t* data, size_t length) {
  ENetPacket* packet =
      enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
  enet_host_broadcast(server, 0, packet);
  enet_host_flush(server);
}

void ENetServerTransport::stop() {
  if (server != nullptr) {
    enet_host_destroy(server);
    server = nullptr;
  }
  Logger::info("ENet server transport stopped");
}
