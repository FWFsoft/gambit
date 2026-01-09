#pragma once

#include <deque>
#include <unordered_map>

#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Objective.h"
#include "Player.h"
#include "WorldConfig.h"
#include "WorldItem.h"

// Client-side objective data for rendering
struct ClientObjective {
  uint32_t id;
  ObjectiveType type;
  ObjectiveState state;
  float x, y;
  float radius;
  float progress;
  int enemiesRequired;
  int enemiesKilled;
};

class NetworkClient;
class CollisionSystem;

class ClientPrediction {
 public:
  ClientPrediction(NetworkClient* client, uint32_t localPlayerId,
                   const WorldConfig& world);

  const Player& getLocalPlayer() const { return localPlayer; }
  Player& getLocalPlayerMutable() { return localPlayer; }

  const std::unordered_map<uint32_t, WorldItem>& getWorldItems() const {
    return worldItems;
  }

  // Get all objectives for rendering
  const std::unordered_map<uint32_t, ClientObjective>& getObjectives() const {
    return objectives;
  }

  // Update an objective from network packet
  void updateObjective(const ObjectiveStatePacket& packet);

  // Reset character selection state (for returning to character select)
  void resetCharacterSelection() { sentCharacterSelection = false; }

 private:
  NetworkClient* client;
  uint32_t localPlayerId;
  Player localPlayer;
  float worldWidth;
  float worldHeight;
  const CollisionSystem* collisionSystem;

  std::deque<LocalInputEvent> inputHistory;
  uint32_t localInputSequence;

  // World items (items lying on the ground)
  std::unordered_map<uint32_t, WorldItem> worldItems;

  // Objectives received from server
  std::unordered_map<uint32_t, ClientObjective> objectives;

  // Character selection state
  bool sentCharacterSelection;

  void onLocalInput(const LocalInputEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);

  void reconcile(const StateUpdatePacket& stateUpdate);
};
