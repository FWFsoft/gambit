#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "transport/IServerTransport.h"

class NetworkServer {
 public:
  // Takes ownership of the transport
  explicit NetworkServer(std::unique_ptr<IServerTransport> transport);
  ~NetworkServer();

  bool initialize(const std::string& address, uint16_t port);
  void run();
  void poll();
  void stop();

  void broadcastPacket(const std::vector<uint8_t>& data);
  void send(uint32_t clientId, const std::vector<uint8_t>& data);

 private:
  std::unique_ptr<IServerTransport> transport;
  bool running;
};
