#pragma once

#include <cstdlib>
#include <memory>
#include <unordered_map>

#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Player.h"
#include "WorldConfig.h"

class NetworkServer;
class CollisionSystem;
class EnemySystem;

class ServerGameState {
 public:
  ServerGameState(NetworkServer* server, const WorldConfig& world);
  ~ServerGameState();

  EnemySystem* getEnemySystem() { return enemySystem.get(); }

 private:
  NetworkServer* server;
  float worldWidth;
  float worldHeight;
  const CollisionSystem* collisionSystem;
  std::unordered_map<uint32_t, Player> players;
  std::unordered_map<ENetPeer*, uint32_t> peerToPlayerId;
  uint32_t serverTick;
  std::unique_ptr<EnemySystem> enemySystem;

  void onClientConnected(const ClientConnectedEvent& e);
  void onClientDisconnected(const ClientDisconnectedEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);
  void onUpdate(const UpdateEvent& e);

  void processClientInput(ENetPeer* peer, const uint8_t* data, size_t size);
  void broadcastStateUpdate();

  // Helper methods for player spawning
  Player createPlayer(uint32_t playerId);
  bool findValidSpawnPosition(float& x, float& y);
  void assignPlayerColor(Player& player, size_t playerCount);
};
