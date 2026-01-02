#include "ClientPrediction.h"

#include <cmath>

#include "CharacterRegistry.h"
#include "CharacterSelectionState.h"
#include "DamageNumberSystem.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "NetworkClient.h"
#include "config/PlayerConfig.h"

ClientPrediction::ClientPrediction(NetworkClient* client,
                                   uint32_t localPlayerId,
                                   const WorldConfig& world)
    : client(client),
      localPlayerId(localPlayerId),
      worldWidth(world.width),
      worldHeight(world.height),
      collisionSystem(world.collisionSystem),
      localInputSequence(0) {
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

void ClientPrediction::onLocalInput(const LocalInputEvent& e) {
  // Skip input processing if player is dead
  if (localPlayer.isDead()) {
    return;
  }

  // Apply input immediately to local player (instant response)
  MovementInput input(e.moveLeft, e.moveRight, e.moveUp, e.moveDown, 16.67f,
                      worldWidth, worldHeight, collisionSystem);
  applyInput(localPlayer, input);

  // Store input in history for reconciliation
  LocalInputEvent inputCopy = e;
  inputCopy.inputSequence = localInputSequence++;
  inputHistory.push_back(inputCopy);

  // Keep only last 60 inputs (1 second at 60 FPS)
  if (inputHistory.size() > 60) {
    inputHistory.pop_front();
  }

  // Serialize and send to server
  ClientInputPacket packet;
  packet.inputSequence = inputCopy.inputSequence;
  packet.moveLeft = e.moveLeft;
  packet.moveRight = e.moveRight;
  packet.moveUp = e.moveUp;
  packet.moveDown = e.moveDown;

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
    static bool sentCharacterSelection = false;
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

  // Calculate prediction error
  float dx = localPlayer.x - serverState->x;
  float dy = localPlayer.y - serverState->y;
  float error = std::sqrt(dx * dx + dy * dy);

  // Tiger Style: Warn if prediction error is large
  if (error > 50.0f) {
    Logger::info("Large prediction error: " + std::to_string(error) +
                 " pixels");
  }

  // Rewind to server state
  localPlayer.x = serverState->x;
  localPlayer.y = serverState->y;
  localPlayer.vx = serverState->vx;
  localPlayer.vy = serverState->vy;

  // Check for health decrease (player took damage)
  float oldHealth = localPlayer.health;
  localPlayer.health = serverState->health;

  if (localPlayer.health < oldHealth && localPlayer.isAlive()) {
    // Player took damage - publish event for damage numbers
    float damageTaken = oldHealth - localPlayer.health;
    DamageReceivedEvent damageEvent;
    damageEvent.x = localPlayer.x;
    damageEvent.y = localPlayer.y;
    damageEvent.damageAmount = damageTaken;
    EventBus::instance().publish(damageEvent);
  }
  localPlayer.r = serverState->r;
  localPlayer.g = serverState->g;
  localPlayer.b = serverState->b;

  // Find inputs that happened AFTER the server state
  // (These are inputs the server hasn't processed yet)
  std::deque<LocalInputEvent> inputsToReplay;
  for (const auto& input : inputHistory) {
    // Only replay inputs the server hasn't processed yet
    if (input.inputSequence > serverState->lastInputSequence) {
      inputsToReplay.push_back(input);
    }
  }

  // Re-apply all inputs that happened after the server state
  for (const auto& inputEvent : inputsToReplay) {
    MovementInput input(inputEvent.moveLeft, inputEvent.moveRight,
                        inputEvent.moveUp, inputEvent.moveDown, 16.67f,
                        worldWidth, worldHeight, collisionSystem);
    applyInput(localPlayer, input);
  }

  // Remove old inputs from history (server has processed these)
  while (!inputHistory.empty() &&
         inputHistory.front().inputSequence <= serverState->lastInputSequence) {
    inputHistory.pop_front();
  }
}
