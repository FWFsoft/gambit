#include "transport/InMemoryServerTransport.h"

#include "Logger.h"

InMemoryServerTransport::InMemoryServerTransport(
    std::shared_ptr<InMemoryChannel> channel)
    : channel(std::move(channel)) {}

bool InMemoryServerTransport::initialize(const std::string& /*address*/,
                                          uint16_t /*port*/) {
  if (!channel) {
    Logger::error("InMemoryServerTransport: No channel provided");
    return false;
  }

  running = true;
  Logger::info("InMemoryServerTransport: Initialized (embedded mode)");
  return true;
}

bool InMemoryServerTransport::poll(TransportEvent& event) {
  if (!running || !channel) return false;

  // Check for client connection request
  if (channel->clientWantsConnect && !clientConnected) {
    channel->clientWantsConnect = false;
    channel->connected = true;
    clientConnected = true;

    event.type = TransportEventType::CONNECT;
    event.clientId = EMBEDDED_CLIENT_ID;
    event.data.clear();

    Logger::info("InMemoryServerTransport: Client " +
                 std::to_string(EMBEDDED_CLIENT_ID) + " connected");
    return true;
  }

  // Check for client disconnect request
  if (channel->clientWantsDisconnect && clientConnected) {
    channel->clientWantsDisconnect = false;
    channel->connected = false;
    clientConnected = false;

    event.type = TransportEventType::DISCONNECT;
    event.clientId = EMBEDDED_CLIENT_ID;
    event.data.clear();

    Logger::info("InMemoryServerTransport: Client " +
                 std::to_string(EMBEDDED_CLIENT_ID) + " disconnected");
    return true;
  }

  // Check for messages from the client
  if (channel->hasClientToServer()) {
    event.type = TransportEventType::RECEIVE;
    event.clientId = EMBEDDED_CLIENT_ID;
    event.data = channel->popClientToServer();
    return true;
  }

  return false;
}

void InMemoryServerTransport::send(uint32_t clientId, const uint8_t* data,
                                    size_t length) {
  if (!running || !channel || clientId != EMBEDDED_CLIENT_ID) return;

  channel->pushServerToClient(data, length);
}

void InMemoryServerTransport::broadcast(const uint8_t* data, size_t length) {
  // In embedded mode, there's only one client
  send(EMBEDDED_CLIENT_ID, data, length);
}

void InMemoryServerTransport::stop() {
  running = false;
  clientConnected = false;
  if (channel) {
    channel->connected = false;
  }
  Logger::info("InMemoryServerTransport: Stopped");
}
