#pragma once

#include <deque>
#include <unordered_map>

#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Player.h"
#include "WorldConfig.h"
#include "WorldItem.h"

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

  // Character selection state
  bool sentCharacterSelection;

  void onLocalInput(const LocalInputEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);

  void reconcile(const StateUpdatePacket& stateUpdate);
};
