#include "ClientPrediction.h"

#include <cmath>

#include "Camera.h"
#include "CharacterRegistry.h"
#include "CharacterSelectionState.h"
#include "DamageNumberSystem.h"
#include "GameStateManager.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "config/PlayerConfig.h"
#include "config/ScreenConfig.h"

namespace {
// Movement modifiers the server last reported for the local player. Using
// the same numbers keeps prediction in step with slows, hastes and roots.
MovementModifiers modifiersFor(const Player& player) {
  bool rooted = (player.stateFlags & PlayerFlags::Rooted) != 0;
  return MovementModifiers(player.speedMult, !rooted);
}
}  // namespace

ClientPrediction::ClientPrediction(NetworkClient* client,
                                   uint32_t localPlayerId,
                                   const WorldConfig& world)
    : client(client),
      localPlayerId(localPlayerId),
      worldWidth(world.width),
      worldHeight(world.height),
      collisionSystem(world.collisionSystem),
      localInputSequence(0),
      sentCharacterSelection(false) {
  // Initialize local player
  localPlayer.id = localPlayerId;
  localPlayer.x =
      Config::Player::DEFAULT_SPAWN_X;  // Default (will be updated by server)
  localPlayer.y = Config::Player::DEFAULT_SPAWN_Y;
  localPlayer.vx = 0.0f;
  localPlayer.vy = 0.0f;
  localPlayer.health = Config::Player::MAX_HEALTH;
  localPlayer.r = 255;
  localPlayer.g = 255;
  localPlayer.b = 255;

  // Subscribe to local input events
  EventBus::instance().subscribe<LocalInputEvent>(
      [this](const LocalInputEvent& e) { onLocalInput(e); });

  // Subscribe to network packet events
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
      });
}

void ClientPrediction::sendToServer(const std::vector<uint8_t>& data) {
  client->send(data);
}

void ClientPrediction::onLocalInput(const LocalInputEvent& e) {
  // The cursor's world position is its offset from whatever the screen is
  // centred on: the local player while the camera follows them (exact, even
  // between rendered frames), the camera itself once it has been unlocked.
  float zoom = Config::Screen::WORLD_ZOOM;
  float viewX = localPlayer.x;
  float viewY = localPlayer.y;
  if (camera) {
    zoom = camera->zoom;
    if (!camera->isFollowing()) {
      viewX = camera->x;
      viewY = camera->y;
    }
  }
  aimX = viewX + (e.mouseX - Config::Screen::WIDTH / 2.0f) / zoom;
  aimY = viewY + (e.mouseY - Config::Screen::HEIGHT / 2.0f) / zoom + AIM_HEIGHT;
  localPlayer.aimX = aimX;
  localPlayer.aimY = aimY;

  LocalInputEvent inputCopy = e;
  inputCopy.inputSequence = localInputSequence++;

  // Predict movement only when the server would also move us: alive, in the
  // match, and not mid-dash.
  bool canPredict = localPlayer.isAlive() && localPlayer.characterId != 0 &&
                    (localPlayer.stateFlags & PlayerFlags::MoveLocked) == 0;
  if (canPredict) {
    MovementInput input(e.moveLeft, e.moveRight, e.moveUp, e.moveDown, 16.67f,
                        worldWidth, worldHeight, collisionSystem);
    applyInput(localPlayer, input, modifiersFor(localPlayer));

    inputHistory.push_back(inputCopy);
    if (inputHistory.size() > 60) {
      inputHistory.pop_front();
    }
  } else {
    inputHistory.clear();
  }

  // Inputs are sent even while dead so aim and buttons stay current.
  ClientInputPacket packet;
  packet.inputSequence = inputCopy.inputSequence;
  packet.moveLeft = e.moveLeft;
  packet.moveRight = e.moveRight;
  packet.moveUp = e.moveUp;
  packet.moveDown = e.moveDown;
  packet.buttons = e.buttons;
  packet.aimX = aimX;
  packet.aimY = aimY;
  client->send(serialize(packet));
}

void ClientPrediction::onNetworkPacketReceived(
    const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);

  if (type == PacketType::StateUpdate) {
    StateUpdatePacket stateUpdate = deserializeStateUpdate(e.data, e.size);
    reconcile(stateUpdate);

    // Send character selection to server after first state update (ensures
    // connection is established)
    if (!sentCharacterSelection &&
        CharacterSelectionState::instance().hasSelection()) {
      uint32_t selectedId =
          CharacterSelectionState::instance().getSelectedCharacterId();
      const CharacterDefinition* character =
          CharacterRegistry::instance().getCharacter(selectedId);

      if (character) {
        CharacterSelectedPacket charPacket;
        charPacket.characterId = selectedId;
        client->send(serialize(charPacket));
        Logger::info("Sent character selection to server: " + character->name +
                     " (ID: " + std::to_string(selectedId) + ")");
        sentCharacterSelection = true;

        // Also update local player's characterId
        localPlayer.characterId = selectedId;
      }
    }
  } else if (type == PacketType::PlayerJoined) {
    PlayerJoinedPacket packet = deserializePlayerJoined(e.data, e.size);

    // The first PlayerJoined packet we receive is for our local player
    // (server sends PlayerJoined for the connecting client first)
    // Update our player ID to match what the server assigned
    if (localPlayer.r == 255 && localPlayer.g == 255 && localPlayer.b == 255) {
      // Still have default white color, so this must be our player
      localPlayerId = packet.playerId;
      localPlayer.id = packet.playerId;

      // DON'T override r,g,b from server if we have a character selection
      if (!CharacterSelectionState::instance().hasSelection()) {
        localPlayer.r = packet.r;
        localPlayer.g = packet.g;
        localPlayer.b = packet.b;
        Logger::info("Local player ID: " + std::to_string(localPlayerId) +
                     ", color: " + std::to_string(packet.r) + "," +
                     std::to_string(packet.g) + "," + std::to_string(packet.b));
      } else {
        Logger::info("Local player ID: " + std::to_string(localPlayerId) +
                     ", color preserved from character selection");
      }
    }
  } else if (type == PacketType::PlayerDied) {
    PlayerDiedPacket packet = deserializePlayerDied(e.data, e.size);
    if (packet.playerId == localPlayerId) {
      Logger::info("Local player died");
      // Death state will be reconciled via StateUpdate packets
    }
  } else if (type == PacketType::PlayerRespawned) {
    PlayerRespawnedPacket packet = deserializePlayerRespawned(e.data, e.size);
    if (packet.playerId == localPlayerId) {
      Logger::info("Local player respawned at (" + std::to_string(packet.x) +
                   ", " + std::to_string(packet.y) + ")");
      // Clear input history since we teleported
      inputHistory.clear();
      // Position and health will be reconciled via StateUpdate packets
    }
  } else if (type == PacketType::InventoryUpdate) {
    InventoryUpdatePacket packet = deserializeInventoryUpdate(e.data, e.size);
    if (packet.playerId == localPlayerId) {
      // Update local player inventory
      for (int i = 0; i < INVENTORY_SIZE; i++) {
        localPlayer.inventory[i].itemId = packet.inventory[i].itemId;
        localPlayer.inventory[i].quantity = packet.inventory[i].quantity;
      }

      // Update local player equipment
      for (int i = 0; i < EQUIPMENT_SLOTS; i++) {
        localPlayer.equipment[i].itemId = packet.equipment[i].itemId;
        localPlayer.equipment[i].quantity = packet.equipment[i].quantity;
      }

      Logger::info("Inventory updated from server");
    }
  } else if (type == PacketType::ItemSpawned) {
    ItemSpawnedPacket packet = deserializeItemSpawned(e.data, e.size);

    WorldItem worldItem(packet.worldItemId, packet.itemId, packet.x, packet.y,
                        0.0f);
    worldItems[packet.worldItemId] = worldItem;

    Logger::debug("World item " + std::to_string(packet.worldItemId) +
                  " spawned");
  } else if (type == PacketType::ItemPickedUp) {
    ItemPickedUpPacket packet = deserializeItemPickedUp(e.data, e.size);

    // Get item info before erasing
    auto it = worldItems.find(packet.worldItemId);
    if (it != worldItems.end()) {
      uint32_t itemId = it->second.itemId;
      worldItems.erase(it);

      // If local player picked it up, show notification
      if (packet.playerId == localPlayerId) {
        ItemPickedUpEvent event;
        event.itemId = itemId;
        event.quantity = 1;
        EventBus::instance().publish(event);
      }
    }

    Logger::debug("World item " + std::to_string(packet.worldItemId) +
                  " picked up by player " + std::to_string(packet.playerId));
  }
}

void ClientPrediction::reconcile(const StateUpdatePacket& stateUpdate) {
  // Find our player in the state update
  const PlayerState* serverState = nullptr;
  for (const auto& player : stateUpdate.players) {
    if (player.playerId == localPlayerId) {
      serverState = &player;
      break;
    }
  }

  if (!serverState) {
    return;  // We're not in the game yet
  }

  // Update server tick
  localPlayer.lastServerTick = stateUpdate.serverTick;

  // Rewind to server state
  localPlayer.x = serverState->x;
  localPlayer.y = serverState->y;
  localPlayer.vx = serverState->vx;
  localPlayer.vy = serverState->vy;
  localPlayer.health = serverState->health;
  localPlayer.r = serverState->r;
  localPlayer.g = serverState->g;
  localPlayer.b = serverState->b;

  // Combat state mirrored from the server
  localPlayer.maxHealth = serverState->maxHealth;
  localPlayer.shield = serverState->shield;
  localPlayer.stateFlags = serverState->flags;
  localPlayer.speedMult = serverState->speedMult;
  localPlayer.resource = serverState->resource;
  localPlayer.chargeSlot =
      serverState->chargeSlot == 255 ? -1 : serverState->chargeSlot;
  localPlayer.chargeMs = serverState->charge;  // 0-255 progress on the client
  for (int slot = 0; slot < Arena::SLOT_COUNT; slot++) {
    localPlayer.cooldownMs[slot] = serverState->cooldowns[slot] * 10.0f;
  }
  localPlayer.characterId = serverState->characterId;
  const Arena::KitInfo* kit = Arena::kitInfo(localPlayer.characterId);
  localPlayer.moveSpeed = kit ? kit->moveSpeed : Config::Player::SPEED;

  // While the server is moving us (dash, blink, round teleport) there is
  // nothing to predict: follow it exactly.
  if ((serverState->flags & PlayerFlags::MoveLocked) != 0 ||
      localPlayer.isDead()) {
    inputHistory.clear();
    return;
  }

  // Re-apply inputs the server has not processed yet
  MovementModifiers modifiers = modifiersFor(localPlayer);
  for (const auto& inputEvent : inputHistory) {
    if (inputEvent.inputSequence <= serverState->lastInputSequence) continue;
    MovementInput input(inputEvent.moveLeft, inputEvent.moveRight,
                        inputEvent.moveUp, inputEvent.moveDown, 16.67f,
                        worldWidth, worldHeight, collisionSystem);
    applyInput(localPlayer, input, modifiers);
  }

  // Remove old inputs from history (server has processed these)
  while (!inputHistory.empty() &&
         inputHistory.front().inputSequence <= serverState->lastInputSequence) {
    inputHistory.pop_front();
  }
}

void ClientPrediction::setupObjectiveEventHandlers() {
  // Subscribe to InteractInputEvent for objective interaction (F key)
  EventBus::instance().subscribe<InteractInputEvent>(
      [this](const InteractInputEvent&) {
        GameState currentState = GameStateManager::instance().getCurrentState();
        if (currentState != GameState::Playing) {
          return;
        }

        if (!localPlayer.isAlive()) {
          return;
        }

        // Send interaction request to server (server will check if player is
        // near objective) objectiveId = 0 means "nearest objective"
        ObjectiveInteractPacket packet;
        packet.objectiveId = 0;
        client->send(serialize(packet));
        Logger::debug("Sent objective interact request");
      });

  // Subscribe to NetworkPacketReceivedEvent for ObjectiveState packets
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        if (e.size == 0) return;

        PacketType type = static_cast<PacketType>(e.data[0]);

        if (type == PacketType::ShipLocation) {
          ShipLocationPacket packet = deserializeShipLocation(e.data, e.size);
          shipX = packet.x;
          shipY = packet.y;
          hasShip = true;
          Logger::debug("Received ship location at (" +
                        std::to_string(static_cast<int>(shipX)) + ", " +
                        std::to_string(static_cast<int>(shipY)) + ")");
          return;
        }

        if (type == PacketType::ObjectiveState) {
          ObjectiveStatePacket packet =
              deserializeObjectiveState(e.data, e.size);

          // Store objective in ClientPrediction for world-space rendering
          updateObjective(packet);

          // Publish ObjectiveUpdatedEvent for UISystem to display
          ObjectiveUpdatedEvent event;
          event.objectiveId = packet.objectiveId;
          event.state = packet.objectiveState;
          event.progress = packet.progress;

          // Generate name from objective type
          if (packet.objectiveType == 0) {  // AlienScrapyard
            if (packet.objectiveState ==
                static_cast<uint8_t>(ObjectiveState::ReadyToDeposit)) {
              event.name = "Alien Scrapyard (Return to deposit)";
            } else {
              event.name = "Alien Scrapyard";
            }
          } else if (packet.objectiveType == 1) {  // CaptureOutpost
            event.name = "Capture Outpost";
          } else if (packet.objectiveType == 2) {  // SalvageMedpacks
            event.name = "Salvage Medpacks";
          } else if (packet.objectiveType == 3) {  // RecoverProbe
            event.name = "Recover Probe";
          } else if (packet.objectiveType == 4) {  // SaveTheFrogs
            event.name = "Save the Frogs";
          } else if (packet.objectiveType == 5) {  // NoxiousGas
            event.name = "Noxious Gas";
          } else if (packet.objectiveType == 6) {  // LittleJohn
            event.name = "Little John";
          } else if (packet.objectiveType == 7) {  // DigPitTrap
            event.name = "Dig Pit Trap";
          } else if (packet.objectiveType == 8) {  // StringTripwire
            event.name = "String Tripwire";
          } else if (packet.objectiveType == 9) {  // DerelictTurret
            event.name = "Derelict Turret";
          } else {
            event.name = "Unknown Objective";
          }

          Logger::debug("Objective: " + event.name + " at (" +
                        std::to_string(static_cast<int>(packet.x)) + ", " +
                        std::to_string(static_cast<int>(packet.y)) + ")");

          EventBus::instance().publish(event);

          Logger::debug("Received objective state: " + event.name +
                        " state=" + std::to_string(packet.objectiveState) +
                        " progress=" + std::to_string(packet.progress));
        }
      });
}

void ClientPrediction::updateObjective(const ObjectiveStatePacket& packet) {
  ClientObjective& obj = objectives[packet.objectiveId];
  obj.id = packet.objectiveId;
  obj.type = static_cast<ObjectiveType>(packet.objectiveType);
  obj.state = static_cast<ObjectiveState>(packet.objectiveState);
  obj.x = packet.x;
  obj.y = packet.y;
  obj.radius = packet.radius;
  obj.progress = packet.progress;
  obj.enemiesRequired = packet.enemiesRequired;
  obj.enemiesKilled = packet.enemiesKilled;
  obj.depositX = packet.depositX;
  obj.depositY = packet.depositY;

  Logger::debug("Updated objective " + std::to_string(packet.objectiveId) +
                " at (" + std::to_string(packet.x) + ", " +
                std::to_string(packet.y) +
                ") radius=" + std::to_string(packet.radius));
}
