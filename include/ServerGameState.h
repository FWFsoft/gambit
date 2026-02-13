#pragma once

#include <cstdlib>
#include <memory>
#include <unordered_map>

#include "EffectManager.h"
#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Objective.h"
#include "ObjectiveSystem.h"
#include "Player.h"
#include "PlayerSpawn.h"
#include "WorldConfig.h"
#include "WorldItem.h"

class NetworkServer;
class CollisionSystem;
class EnemySystem;

class ServerGameState {
 public:
  ServerGameState(NetworkServer* server, const WorldConfig& world);
  ~ServerGameState();

  EnemySystem* getEnemySystem() { return enemySystem.get(); }
  EffectManager* getEffectManager() { return effectManager.get(); }
  ObjectiveSystem* getObjectiveSystem() { return objectiveSystem.get(); }

 private:
  NetworkServer* server;
  float worldWidth;
  float worldHeight;
  const CollisionSystem* collisionSystem;
  const std::vector<PlayerSpawn>* playerSpawns;
  std::unordered_map<uint32_t, Player> players;
  uint32_t serverTick;
  std::unique_ptr<EnemySystem> enemySystem;
  std::unique_ptr<EffectManager> effectManager;
  std::unique_ptr<ObjectiveSystem> objectiveSystem;

  // World item management
  std::unordered_map<uint32_t, WorldItem> worldItems;
  uint32_t nextWorldItemId;

  void onClientConnected(const ClientConnectedEvent& e);
  void onClientDisconnected(const ClientDisconnectedEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);
  void onUpdate(const UpdateEvent& e);

  void processClientInput(uint32_t clientId, const uint8_t* data, size_t size);
  void processUseItem(uint32_t clientId, const uint8_t* data, size_t size);
  void processEquipItem(uint32_t clientId, const uint8_t* data, size_t size);
  void broadcastStateUpdate();
  void broadcastInventoryUpdate(uint32_t playerId);

  // Helper methods for player spawning
  Player createPlayer(uint32_t playerId);
  bool findValidSpawnPosition(float& x, float& y);
  void assignPlayerColor(Player& player, size_t playerCount);

  // Death and respawn methods
  void checkPlayerDeaths();
  void handlePlayerRespawns();
  void respawnPlayer(Player& player);

  // World item management methods
  void checkEnemyLootDrops();
  void spawnWorldItem(uint32_t itemId, float x, float y);
  void processItemPickupRequest(uint32_t clientId, const uint8_t* data,
                                size_t size);
  void processObjectiveInteract(uint32_t clientId, const uint8_t* data,
                                size_t size);

  // Objective management methods
  void broadcastObjectiveState(uint32_t objectiveId);
  void broadcastAllObjectives(uint32_t clientId);
};
