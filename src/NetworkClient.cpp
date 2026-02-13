#include "NetworkClient.h"

#include "EventBus.h"
#include "Logger.h"

NetworkClient::NetworkClient(std::unique_ptr<INetworkTransport> transport)
    : transport(std::move(transport)) {}

NetworkClient::~NetworkClient() {
  if (transport && transport->isConnected()) {
    transport->disconnect();
  }
}

bool NetworkClient::connect(const std::string& host, uint16_t port) {
  return transport->connect(host, port);
}

void NetworkClient::disconnect() {
  if (transport) {
    transport->disconnect();
  }
}

void NetworkClient::run() {
  if (!transport || !transport->isConnected()) return;

  TransportEvent event;
  while (transport->poll(event)) {
    if (event.type == TransportEventType::RECEIVE) {
      EventBus::instance().publish(NetworkPacketReceivedEvent{
          0, event.data.data(), event.data.size()});
    }
  }
}

void NetworkClient::send(const std::string& message) {
  if (!transport || !transport->isConnected()) return;
  transport->send(reinterpret_cast<const uint8_t*>(message.c_str()),
                  message.length() + 1, true);
}

void NetworkClient::send(const std::vector<uint8_t>& data) {
  if (!transport || !transport->isConnected()) return;
  transport->send(data.data(), data.size(), true);
}
