#include "transport/InMemoryTransport.h"

#include "Logger.h"

InMemoryTransport::InMemoryTransport(std::shared_ptr<InMemoryChannel> channel)
    : channel(std::move(channel)) {}

bool InMemoryTransport::connect(const std::string& /*host*/, uint16_t /*port*/) {
  if (!channel) {
    Logger::error("InMemoryTransport: No channel provided");
    return false;
  }

  // Signal to the server-side that we want to connect
  channel->clientWantsConnect = true;
  connected = true;
  sentConnectEvent = false;

  Logger::info("InMemoryTransport: Connection requested (embedded mode)");
  return true;
}

void InMemoryTransport::disconnect() {
  if (channel) {
    channel->clientWantsDisconnect = true;
    channel->connected = false;
  }
  connected = false;
  Logger::info("InMemoryTransport: Disconnected");
}

void InMemoryTransport::send(const uint8_t* data, size_t length,
                              bool /*reliable*/) {
  if (!connected || !channel) return;

  channel->pushClientToServer(data, length);
}

bool InMemoryTransport::poll(TransportEvent& event) {
  if (!channel) return false;

  // Check for messages from the server
  if (channel->hasServerToClient()) {
    event.type = TransportEventType::RECEIVE;
    event.data = channel->popServerToClient();
    event.clientId = 0;  // Client doesn't use clientId
    return true;
  }

  return false;
}

bool InMemoryTransport::isConnected() const { return connected && channel; }
