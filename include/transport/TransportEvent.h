#pragma once

#include <cstdint>
#include <vector>

enum class TransportEventType {
  CONNECT,
  RECEIVE,
  DISCONNECT,
  NONE
};

struct TransportEvent {
  TransportEventType type = TransportEventType::NONE;
  uint32_t clientId = 0;
  std::vector<uint8_t> data;
};
