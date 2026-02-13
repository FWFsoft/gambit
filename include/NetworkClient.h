#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "transport/INetworkTransport.h"

class NetworkClient {
 public:
  // Takes ownership of the transport
  explicit NetworkClient(std::unique_ptr<INetworkTransport> transport);
  ~NetworkClient();

  bool connect(const std::string& host, uint16_t port);
  void disconnect();
  void run();
  void send(const std::string& message);
  void send(const std::vector<uint8_t>& data);

 private:
  std::unique_ptr<INetworkTransport> transport;
};
