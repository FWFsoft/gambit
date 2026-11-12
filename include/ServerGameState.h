#pragma once

#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <vector>

#include "EnemySpawn.h"
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
class AbilitySystem;
class MatchDirector;

class ServerGameState {
 public:
  ServerGameState(NetworkServer* server, const WorldConfig& world);
  ~ServerGameState();

  EnemySystem* getEnemySystem() { return enemySystem.get(); }
  EffectManager* getEffectManager() { return effectManager.get(); }
  ObjectiveSystem* getObjectiveSystem() { return objectiveSystem.get(); }
  AbilitySystem* getAbilitySystem() { return abilitySystem.get(); }
  MatchDirector* getMatchDirector() { return director.get(); }
  std::unordered_map<uint32_t, Player>& getPlayers() { return players; }

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
  std::unique_ptr<AbilitySystem> abilitySystem;
  std::unique_ptr<MatchDirector> director;
  std::vector<EnemySpawn> noSpawns;  // used when no map is loaded
  // Entities that had effects in the last effect broadcast, so one final
  // empty update can clear them on clients.
  std::vector<uint32_t> playersWithEffects;
  std::vector<uint32_t> enemiesWithEffects;

  // Ship (home base) position loaded from map
  float shipX = 0.0f;
  float shipY = 0.0f;
  bool hasShip = false;

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

  // Arena match glue
  void handleEnemyDeaths();
  void processArenaPickups();
  void clearArenaPickups();
  void broadcastEffects();
  // Effect lab: add server-side allies that stand beside `owner`.
  void spawnTrainingAllies(const Player& owner);

  // World item management methods
  void spawnWorldItem(uint32_t itemId, float x, float y);
  void processItemPickupRequest(uint32_t clientId, const uint8_t* data,
                                size_t size);
  void processObjectiveInteract(uint32_t clientId, const uint8_t* data,
                                size_t size);

  // Objective management methods
  void broadcastObjectiveState(uint32_t objectiveId);
  void broadcastAllObjectives(uint32_t clientId);

  // Per-frame objective side effects (gas damage, LittleJohn activation)
  void updateObjectiveSideEffects(float deltaTime);

  // Link LittleJohn objectives to their guardian enemies
  void initializeLittleJohnGuardians();
};
