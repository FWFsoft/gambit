#pragma once

#include <cstdint>
#include <string>

#include "transport/TransportEvent.h"

class INetworkTransport {
 public:
  virtual ~INetworkTransport() = default;

  virtual bool connect(const std::string& host, uint16_t port) = 0;
  virtual void disconnect() = 0;
  virtual void send(const uint8_t* data, size_t length,
                    bool reliable = true) = 0;
  virtual bool poll(TransportEvent& event) = 0;
  virtual bool isConnected() const = 0;
};
