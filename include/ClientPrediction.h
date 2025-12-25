#pragma once

#include <deque>

#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Player.h"
#include "WorldConfig.h"

class NetworkClient;
class CollisionSystem;

class ClientPrediction {
 public:
  ClientPrediction(NetworkClient* client, uint32_t localPlayerId,
                   const WorldConfig& world);

  const Player& getLocalPlayer() const { return localPlayer; }

 private:
  NetworkClient* client;
  uint32_t localPlayerId;
  Player localPlayer;
  float worldWidth;
  float worldHeight;
  const CollisionSystem* collisionSystem;

  std::deque<LocalInputEvent> inputHistory;
  uint32_t localInputSequence;

  void onLocalInput(const LocalInputEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);

  void reconcile(const StateUpdatePacket& stateUpdate);
};
