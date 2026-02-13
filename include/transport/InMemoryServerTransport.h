#pragma once

#include <memory>

#include "InMemoryChannel.h"
#include "transport/IServerTransport.h"

// InMemoryServerTransport: Server-side transport for embedded server mode
// Communicates with InMemoryTransport via a shared InMemoryChannel.

class InMemoryServerTransport : public IServerTransport {
 public:
  explicit InMemoryServerTransport(std::shared_ptr<InMemoryChannel> channel);
  ~InMemoryServerTransport() override = default;

  bool initialize(const std::string& address, uint16_t port) override;
  bool poll(TransportEvent& event) override;
  void send(uint32_t clientId, const uint8_t* data, size_t length) override;
  void broadcast(const uint8_t* data, size_t length) override;
  void stop() override;

 private:
  std::shared_ptr<InMemoryChannel> channel;
  bool running = false;
  bool clientConnected = false;

  static constexpr uint32_t EMBEDDED_CLIENT_ID = 1;
};
