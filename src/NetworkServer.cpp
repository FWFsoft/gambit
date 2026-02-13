#include "NetworkServer.h"

#include <SDL2/SDL.h>

#include "EventBus.h"
#include "Logger.h"

NetworkServer::NetworkServer(std::unique_ptr<IServerTransport> transport)
    : transport(std::move(transport)), running(false) {}

NetworkServer::~NetworkServer() {
  if (transport) {
    transport->stop();
  }
}

bool NetworkServer::initialize(const std::string& address, uint16_t port) {
  if (!transport->initialize(address, port)) {
    Logger::error("Failed to initialize server transport.");
    return false;
  }
  Logger::info("Server initialized and listening on port " +
               std::to_string(port));
  return true;
}

void NetworkServer::run() {
  running = true;

  while (running) {
    poll();
    // Small sleep to avoid busy-waiting in blocking mode
    SDL_Delay(1);
  }
  Logger::info("Server shutting down.");
}

void NetworkServer::poll() {
  if (!transport) return;

  TransportEvent event;
  while (transport->poll(event)) {
    switch (event.type) {
      case TransportEventType::CONNECT:
        EventBus::instance().publish(ClientConnectedEvent{event.clientId});
        break;

      case TransportEventType::RECEIVE:
        EventBus::instance().publish(NetworkPacketReceivedEvent{
            event.clientId, event.data.data(), event.data.size()});
        break;

      case TransportEventType::DISCONNECT:
        EventBus::instance().publish(ClientDisconnectedEvent{event.clientId});
        break;

      default:
        break;
    }
  }
}

void NetworkServer::stop() { running = false; }

void NetworkServer::broadcastPacket(const std::vector<uint8_t>& data) {
  if (!transport) return;
  transport->broadcast(data.data(), data.size());
}

void NetworkServer::send(uint32_t clientId, const std::vector<uint8_t>& data) {
  if (!transport) return;
  transport->send(clientId, data.data(), data.size());
}
