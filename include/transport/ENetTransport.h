#pragma once

#include <enet/enet.h>
#include <cstdint>
#include <string>
#include <vector>

#include "transport/INetworkTransport.h"

class ENetTransport : public INetworkTransport {
 public:
  ENetTransport();
  ~ENetTransport() override;

  bool connect(const std::string& host, uint16_t port) override;
  void disconnect() override;
  void send(const uint8_t* data, size_t length, bool reliable = true) override;
  bool poll(TransportEvent& event) override;
  bool isConnected() const override;

 private:
  ENetHost* client;
  ENetPeer* peer;
  bool connected;
};
