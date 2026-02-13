#pragma once

#include <enet/enet.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "transport/IServerTransport.h"

class ENetServerTransport : public IServerTransport {
 public:
  ENetServerTransport();
  ~ENetServerTransport() override;

  bool initialize(const std::string& address, uint16_t port) override;
  bool poll(TransportEvent& event) override;
  void send(uint32_t clientId, const uint8_t* data, size_t length) override;
  void broadcast(const uint8_t* data, size_t length) override;
  void stop() override;

 private:
  ENetHost* server;
  std::unordered_map<uint32_t, ENetPeer*> clientPeers;
  uint32_t nextClientId;
};
