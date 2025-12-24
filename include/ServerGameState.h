#pragma once

#include <cstdlib>
#include <unordered_map>

#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Player.h"

class NetworkServer;

class ServerGameState {
 public:
  ServerGameState(NetworkServer* server, float worldWidth, float worldHeight);

 private:
  NetworkServer* server;
  float worldWidth;
  float worldHeight;
  std::unordered_map<uint32_t, Player> players;
  std::unordered_map<ENetPeer*, uint32_t> peerToPlayerId;
  uint32_t serverTick;

  void onClientConnected(const ClientConnectedEvent& e);
  void onClientDisconnected(const ClientDisconnectedEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);
  void onUpdate(const UpdateEvent& e);

  void processClientInput(ENetPeer* peer, const uint8_t* data, size_t size);
  void broadcastStateUpdate();
};
