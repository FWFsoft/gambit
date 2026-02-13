#include "transport/ENetTransport.h"

#include "Logger.h"

ENetTransport::ENetTransport()
    : client(nullptr), peer(nullptr), connected(false) {}

ENetTransport::~ENetTransport() {
  if (client) {
    enet_host_destroy(client);
    enet_deinitialize();
  }
}

bool ENetTransport::connect(const std::string& host, uint16_t port) {
  if (enet_initialize() != 0) {
    Logger::error("An error occurred while initializing ENet.");
    return false;
  }

  client = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!client) {
    Logger::error(
        "An error occurred while trying to create an ENet client host.");
    enet_deinitialize();
    return false;
  }

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
    Logger::error("Connection to " + host + ":" + std::to_string(port) +
                  " failed.");
    enet_peer_reset(peer);
    return false;
  }
}

void ENetTransport::disconnect() {
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

void ENetTransport::send(const uint8_t* data, size_t length, bool reliable) {
  if (!connected) return;

  ENetPacket* packet = enet_packet_create(
      data, length, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
  enet_peer_send(peer, 0, packet);
  enet_host_flush(client);
}

bool ENetTransport::poll(TransportEvent& event) {
  if (!client) return false;

  ENetEvent enetEvent;
  if (enet_host_service(client, &enetEvent, 0) > 0) {
    if (enetEvent.type == ENET_EVENT_TYPE_RECEIVE) {
      event.type = TransportEventType::RECEIVE;
      event.data.assign(enetEvent.packet->data,
                        enetEvent.packet->data + enetEvent.packet->dataLength);
      enet_packet_destroy(enetEvent.packet);
      return true;
    }
  }

  return false;
}

bool ENetTransport::isConnected() const { return connected; }
