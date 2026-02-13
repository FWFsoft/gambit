#pragma once

#include <cstdint>
#include <string>

#include "transport/TransportEvent.h"

class IServerTransport {
 public:
  virtual ~IServerTransport() = default;

  virtual bool initialize(const std::string& address, uint16_t port) = 0;
  virtual bool poll(TransportEvent& event) = 0;
  virtual void send(uint32_t clientId, const uint8_t* data,
                    size_t length) = 0;
  virtual void broadcast(const uint8_t* data, size_t length) = 0;
  virtual void stop() = 0;
};
