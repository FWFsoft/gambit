#pragma once

#include <deque>
#include <unordered_map>
#include <vector>

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
  float depositX, depositY;
};

class NetworkClient;
class CollisionSystem;
struct Camera;

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

  bool hasShipLocation() const { return hasShip; }
  float getShipX() const { return shipX; }
  float getShipY() const { return shipY; }

  // Update an objective from network packet
  void updateObjective(const ObjectiveStatePacket& packet);

  // Set up objective event handlers (InteractInputEvent + ObjectiveState
  // packets)
  void setupObjectiveEventHandlers();

  // Reset character selection state (for returning to character select)
  void resetCharacterSelection() { sentCharacterSelection = false; }

  uint32_t getLocalPlayerId() const { return localPlayerId; }
  // Cursor position in world space as last sent to the server.
  float getAimX() const { return aimX; }
  float getAimY() const { return aimY; }
  // The view the cursor is aimed through. Without one (tests, headless) the
  // view is assumed to be centred on the local player.
  void setCamera(const Camera* camera) { this->camera = camera; }
  // Send any packet to the server (HUD actions: ready, infusions, debug).
  void sendToServer(const std::vector<uint8_t>& data);

  // Height above the feet that the cursor is assumed to point at, so that
  // clicking on an enemy's body aims at the ground point it stands on.
  static constexpr float AIM_HEIGHT = 26.0f;

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

  // Ship (home base) position received from server
  float shipX = 0.0f;
  float shipY = 0.0f;
  bool hasShip = false;

  const Camera* camera = nullptr;  // not owned; may be null
  float aimX = 0.0f;
  float aimY = 0.0f;

  // Character selection state
  bool sentCharacterSelection;

  void onLocalInput(const LocalInputEvent& e);
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);

  void reconcile(const StateUpdatePacket& stateUpdate);
};
