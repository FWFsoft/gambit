#pragma once

#include <memory>

#include "InMemoryChannel.h"
#include "transport/INetworkTransport.h"

// InMemoryTransport: Client-side transport for embedded server mode
// Communicates with InMemoryServerTransport via a shared InMemoryChannel.

class InMemoryTransport : public INetworkTransport {
 public:
  explicit InMemoryTransport(std::shared_ptr<InMemoryChannel> channel);
  ~InMemoryTransport() override = default;

  bool connect(const std::string& host, uint16_t port) override;
  void disconnect() override;
  void send(const uint8_t* data, size_t length, bool reliable = true) override;
  bool poll(TransportEvent& event) override;
  bool isConnected() const override;

 private:
  std::shared_ptr<InMemoryChannel> channel;
  bool connected = false;
  bool sentConnectEvent = false;
};
