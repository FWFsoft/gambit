#include "ServerGameState.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "CollisionSystem.h"
#include "EnemySystem.h"
#include "ItemRegistry.h"
#include "Logger.h"
#include "NetworkServer.h"
#include "TiledMap.h"
#include "config/GameplayConfig.h"
#include "config/PlayerConfig.h"
#include "config/TimingConfig.h"

ServerGameState::ServerGameState(NetworkServer* server,
                                 const WorldConfig& world)
    : server(server),
      worldWidth(world.width),
      worldHeight(world.height),
      collisionSystem(world.collisionSystem),
      playerSpawns(nullptr),
      serverTick(0),
      nextWorldItemId(1) {
  // Initialize player spawns
  if (world.tiledMap != nullptr && !world.tiledMap->getPlayerSpawns().empty()) {
    playerSpawns = &world.tiledMap->getPlayerSpawns();
    Logger::info("Using " + std::to_string(playerSpawns->size()) +
                 " player spawn points");
  }

  // Initialize enemy system
  if (world.tiledMap != nullptr) {
    enemySystem =
        std::make_unique<EnemySystem>(world.tiledMap->getEnemySpawns());
    enemySystem->spawnAllEnemies();
  }

  // Initialize effect manager
  effectManager = std::make_unique<EffectManager>();
  Logger::info("EffectManager initialized");

  // Initialize objective system
  if (world.tiledMap != nullptr) {
    objectiveSystem = std::make_unique<ObjectiveSystem>();
    objectiveSystem->initialize(world.tiledMap->getObjectives());

    // Set up callbacks for state changes
    objectiveSystem->setStateCallback(
        [this](uint32_t objectiveId, ObjectiveState newState) {
          broadcastObjectiveState(objectiveId);
        });

    objectiveSystem->setProgressCallback(
        [this](uint32_t objectiveId, float progress) {
          broadcastObjectiveState(objectiveId);
        });
  }

  // Subscribe to client connection events
  EventBus::instance().subscribe<ClientConnectedEvent>(
      [this](const ClientConnectedEvent& e) { onClientConnected(e); });

  EventBus::instance().subscribe<ClientDisconnectedEvent>(
      [this](const ClientDisconnectedEvent& e) { onClientDisconnected(e); });

  // Subscribe to network packet events
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
      });

  // Subscribe to update events
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });
}

ServerGameState::~ServerGameState() = default;

void ServerGameState::onClientConnected(const ClientConnectedEvent& e) {
  uint32_t playerId = e.clientId;

  Player player = createPlayer(playerId);
  assignPlayerColor(player, players.size());

  players[playerId] = player;

  Logger::info("Player " + std::to_string(playerId) + " joined");

  // Send new player's own PlayerJoined packet to them FIRST
  // (so they recognize it as their local player before receiving existing
  // players)
  PlayerJoinedPacket newPlayerPacket;
  newPlayerPacket.playerId = playerId;
  newPlayerPacket.r = player.r;
  newPlayerPacket.g = player.g;
  newPlayerPacket.b = player.b;
  server->send(e.clientId, serialize(newPlayerPacket));

  // Then send existing players to new client
  for (const auto& [existingId, existingPlayer] : players) {
    if (existingId != playerId) {
      PlayerJoinedPacket existingPacket;
      existingPacket.playerId = existingId;
      existingPacket.r = existingPlayer.r;
      existingPacket.g = existingPlayer.g;
      existingPacket.b = existingPlayer.b;
      server->send(e.clientId, serialize(existingPacket));
      Logger::info("Sent existing player " + std::to_string(existingId) +
                   " to new player " + std::to_string(playerId));
    }
  }

  // Broadcast new player join to all clients (new client will ignore duplicate)
  server->broadcastPacket(serialize(newPlayerPacket));

  // Send all objectives to the new client
  if (objectiveSystem) {
    broadcastAllObjectives(e.clientId);
  }
}

void ServerGameState::onClientDisconnected(const ClientDisconnectedEvent& e) {
  uint32_t playerId = e.clientId;

  players.erase(playerId);

  Logger::info("Player " + std::to_string(playerId) + " left");

  // Broadcast player left to all clients
  PlayerLeftPacket packet;
  packet.playerId = playerId;

  server->broadcastPacket(serialize(packet));
}

void ServerGameState::onNetworkPacketReceived(
    const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;

  // In embedded mode, server and client share the same EventBus.
  // Skip client-side events (clientId == 0) — only process real client messages.
  if (e.clientId == 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);

  switch (type) {
    case PacketType::ClientInput:
      processClientInput(e.clientId, e.data, e.size);
      break;

    case PacketType::AttackEnemy: {
      if (e.size < 9) {
        Logger::info("Invalid AttackEnemy packet size");
        break;
      }

      AttackEnemyPacket attackPacket = deserializeAttackEnemy(e.data, e.size);

      uint32_t playerId = e.clientId;

      // Apply damage (server-authoritative)
      if (enemySystem && effectManager) {
        // Calculate modified damage based on player and enemy effects
        float damage = attackPacket.damage;

        // Apply player's damage dealt modifiers (Empowered/Weakened)
        auto playerMods = effectManager->calculateModifiers(playerId, false);
        damage *= playerMods.damageDealtMultiplier;

        // Apply enemy's damage taken modifiers and consume Expose/Guard
        effectManager->consumeOnDamage(attackPacket.enemyId, true, damage);

        Logger::debug("Attack damage: " + std::to_string(attackPacket.damage) +
                      " → modified: " + std::to_string(damage) +
                      " (player mult: " +
                      std::to_string(playerMods.damageDealtMultiplier) + ")");

        // Apply final damage
        enemySystem->damageEnemy(attackPacket.enemyId, damage, playerId);

        // Apply effect based on character selection
        if (true) {
          auto playerIt = players.find(playerId);
          if (playerIt != players.end()) {
            uint32_t characterId = playerIt->second.characterId;
            EffectType effectToApply;
            const char* effectName;

            // Map character IDs to effects (testing multiple effects)
            // Character IDs 1-19, map to different effects
            switch (characterId) {
              case 1:  // Eliana
                effectToApply = EffectType::Slow;
                effectName = "Slow";
                break;
              case 2:  // Fagan
                effectToApply = EffectType::Weakened;
                effectName = "Weakened";
                break;
              case 3:  // Gravon
                effectToApply = EffectType::Vulnerable;
                effectName = "Vulnerable";
                break;
              case 4:  // Isaac
                effectToApply = EffectType::Wound;
                effectName = "Wound (DoT)";
                break;
              case 5:  // Jeff
                effectToApply = EffectType::Haste;
                effectName = "Haste";
                break;
              case 6:  // Kade
                effectToApply = EffectType::Empowered;
                effectName = "Empowered";
                break;
              case 7:  // Lilith
                effectToApply = EffectType::Fortified;
                effectName = "Fortified";
                break;
              case 8:  // MILES
                effectToApply = EffectType::Mend;
                effectName = "Mend (HoT)";
                break;
              case 9:  // Mina
                effectToApply = EffectType::Cursed;
                effectName = "Cursed";
                break;
              case 10:  // Mordryn
                effectToApply = EffectType::Blessed;
                effectName = "Blessed";
                break;
              case 11:  // Namora
                effectToApply = EffectType::Marked;
                effectName = "Marked";
                break;
              case 12:  // Nolan
                effectToApply = EffectType::Stealth;
                effectName = "Stealth";
                break;
              case 13:  // Nyx
                effectToApply = EffectType::Expose;
                effectName = "Expose";
                break;
              case 14:  // Presidente
                effectToApply = EffectType::Guard;
                effectName = "Guard";
                break;
              case 15:  // Stitches
                effectToApply = EffectType::Stunned;
                effectName = "Stunned";
                break;
              case 16:  // Suds
                effectToApply = EffectType::Berserk;
                effectName = "Berserk";
                break;
              case 17:  // Valthor
                effectToApply = EffectType::Snared;
                effectName = "Snared";
                break;
              case 18:  // Volgore
                effectToApply = EffectType::Unbounded;
                effectName = "Unbounded";
                break;
              case 19:  // Wade
                effectToApply = EffectType::Confused;
                effectName = "Confused";
                break;
              default:  // No character selected - use Slow
                effectToApply = EffectType::Slow;
                effectName = "Slow (default)";
                break;
            }

            Logger::info("Player " + std::to_string(playerId) + " (Character " +
                         std::to_string(characterId) + ") applying " +
                         std::string(effectName) + " to enemy " +
                         std::to_string(attackPacket.enemyId));

            effectManager->applyEffect(attackPacket.enemyId, effectToApply, 1,
                                       3000.0f, playerId,
                                       enemySystem->getEnemies());
          }
        }
      }

      break;
    }

    case PacketType::UseItem:
      processUseItem(e.clientId, e.data, e.size);
      break;

    case PacketType::EquipItem:
      processEquipItem(e.clientId, e.data, e.size);
      break;

    case PacketType::CharacterSelected: {
      if (e.size < 5) {
        Logger::info("Invalid CharacterSelected packet size");
        break;
      }

      CharacterSelectedPacket charPacket =
          deserializeCharacterSelected(e.data, e.size);

      uint32_t playerId = e.clientId;
      {
        auto playerIt = players.find(playerId);
        if (playerIt != players.end()) {
          playerIt->second.characterId = charPacket.characterId;
          Logger::info("Player " + std::to_string(playerId) +
                       " selected character ID " +
                       std::to_string(charPacket.characterId));
        }
      }
      break;
    }

    case PacketType::ItemPickupRequest:
      processItemPickupRequest(e.clientId, e.data, e.size);
      break;

    case PacketType::ObjectiveInteract:
      processObjectiveInteract(e.clientId, e.data, e.size);
      break;

    default:
      Logger::info("Unknown packet type: " +
                   std::to_string(static_cast<int>(type)));
      break;
  }
}

void ServerGameState::processClientInput(uint32_t clientId, const uint8_t* data,
                                         size_t size) {
  ClientInputPacket input = deserializeClientInput(data, size);

  uint32_t playerId = clientId;
  auto playerIt = players.find(playerId);
  assert(playerIt != players.end());

  Player& player = playerIt->second;

  // Ignore input from dead players
  if (player.isDead()) {
    return;
  }

  // Validate input sequence (prevent replay attacks)
  if (input.inputSequence <= player.lastInputSequence) {
    Logger::info("Received old input sequence from player " +
                 std::to_string(playerId) + ", ignoring");
    return;
  }
  player.lastInputSequence = input.inputSequence;

  MovementInput movementInput(input.moveLeft, input.moveRight, input.moveUp,
                              input.moveDown, Config::Timing::TARGET_DELTA_MS,
                              worldWidth, worldHeight, collisionSystem);

  // Apply effect modifiers to movement
  MovementModifiers modifiers;
  if (effectManager) {
    auto statMods = effectManager->calculateModifiers(playerId, false);
    modifiers.speedMultiplier = statMods.movementSpeedMultiplier;
    modifiers.canMove = statMods.canMove;
  }

  applyInput(player, movementInput, modifiers);
}

void ServerGameState::onUpdate(const UpdateEvent& e) {
  serverTick++;

  // Check for enemy deaths and spawn loot (BEFORE update clears the vector)
  checkEnemyLootDrops();

  // Notify objective system about enemy deaths (for CaptureOutpost)
  if (enemySystem && objectiveSystem) {
    const auto& deaths = enemySystem->getDiedThisFrame();
    for (const auto& death : deaths) {
      const auto& enemies = enemySystem->getEnemies();
      auto enemyIt = enemies.find(death.enemyId);
      if (enemyIt != enemies.end()) {
        objectiveSystem->onEnemyDeath(enemyIt->second.x, enemyIt->second.y);
      }
    }
  }

  // Update enemy AI
  if (enemySystem) {
    enemySystem->update(e.deltaTime, players, effectManager.get());
  }

  // Update effects (DoT/HoT, duration ticking)
  if (effectManager && enemySystem) {
    effectManager->update(e.deltaTime, players, enemySystem->getEnemies(),
                          enemySystem.get());
  }

  // Update objective system (handles interaction timers)
  if (objectiveSystem) {
    objectiveSystem->update(e.deltaTime);
  }

  // Check for player deaths
  checkPlayerDeaths();

  // Check for player respawns
  handlePlayerRespawns();

  // Broadcast state update every frame
  broadcastStateUpdate();
}

void ServerGameState::broadcastStateUpdate() {
  StateUpdatePacket packet;
  packet.serverTick = serverTick;

  for (const auto& [id, player] : players) {
    PlayerState ps;
    ps.playerId = player.id;
    ps.x = player.x;
    ps.y = player.y;
    ps.vx = player.vx;
    ps.vy = player.vy;
    ps.health = player.health;
    ps.r = player.r;
    ps.g = player.g;
    ps.b = player.b;
    ps.lastInputSequence = player.lastInputSequence;
    packet.players.push_back(ps);
  }

  server->broadcastPacket(serialize(packet));

  // Broadcast enemy state
  if (enemySystem) {
    const auto& enemies = enemySystem->getEnemies();

    EnemyStateUpdatePacket enemyPacket;
    for (const auto& [id, enemy] : enemies) {
      NetworkEnemyState state;
      state.id = enemy.id;
      state.type = static_cast<uint8_t>(enemy.type);
      state.state = static_cast<uint8_t>(enemy.state);
      state.x = enemy.x;
      state.y = enemy.y;
      state.vx = enemy.vx;
      state.vy = enemy.vy;
      state.health = enemy.health;
      state.maxHealth = enemy.maxHealth;

      enemyPacket.enemies.push_back(state);
    }

    server->broadcastPacket(serialize(enemyPacket));

    // Broadcast enemy deaths
    const auto& deaths = enemySystem->getDiedThisFrame();
    for (const auto& death : deaths) {
      EnemyDiedPacket deathPacket;
      deathPacket.enemyId = death.enemyId;
      deathPacket.killerId = death.killerId;

      server->broadcastPacket(serialize(deathPacket));

      Logger::debug(
          "Broadcast EnemyDied: enemy=" + std::to_string(death.enemyId) +
          " killer=" + std::to_string(death.killerId));
    }
  }

  // Broadcast effect updates for all entities with active effects
  if (effectManager && enemySystem) {
    // Player effects
    for (const auto& [id, player] : players) {
      const ActiveEffects& effects = effectManager->getPlayerEffects(id);
      if (!effects.effects.empty()) {
        EffectUpdatePacket packet;
        packet.targetId = id;
        packet.isEnemy = false;

        for (const auto& effect : effects.effects) {
          NetworkEffect ne;
          ne.effectType = static_cast<uint8_t>(effect.type);
          ne.stacks = effect.stacks;
          ne.remainingDuration = effect.remainingDuration;
          packet.effects.push_back(ne);
        }

        server->broadcastPacket(serialize(packet));
      }
    }

    // Enemy effects
    const auto& enemies = enemySystem->getEnemies();
    for (const auto& [id, enemy] : enemies) {
      const ActiveEffects& effects = effectManager->getEnemyEffects(id);
      if (!effects.effects.empty()) {
        EffectUpdatePacket packet;
        packet.targetId = id;
        packet.isEnemy = true;

        for (const auto& effect : effects.effects) {
          NetworkEffect ne;
          ne.effectType = static_cast<uint8_t>(effect.type);
          ne.stacks = effect.stacks;
          ne.remainingDuration = effect.remainingDuration;
          packet.effects.push_back(ne);
        }

        server->broadcastPacket(serialize(packet));
      }
    }
  }
}

Player ServerGameState::createPlayer(uint32_t playerId) {
  Player player;
  player.id = playerId;
  player.vx = 0;
  player.vy = 0;
  player.health = Config::Player::MAX_HEALTH;
  player.lastInputSequence = 0;

  // Find spawn position
  if (playerSpawns && !playerSpawns->empty()) {
    // Use round-robin spawn point selection based on player ID
    size_t spawnIndex = playerId % playerSpawns->size();
    const PlayerSpawn& spawn = (*playerSpawns)[spawnIndex];
    player.x = spawn.x;
    player.y = spawn.y;

    Logger::info("Player " + std::to_string(playerId) + " spawned at " +
                 spawn.name + " (" + std::to_string(player.x) + ", " +
                 std::to_string(player.y) + ")");
  } else {
    // Fallback: use world center (map is centered at origin)
    player.x = 0.0f;
    player.y = 0.0f;
    Logger::info("Player " + std::to_string(playerId) +
                 " spawned at world center (no spawn points)");
  }

  if (!findValidSpawnPosition(player.x, player.y)) {
    Logger::error("Failed to find valid spawn position");
  }

  // Add test items to inventory for demonstration
  player.inventory[0] = ItemStack(1, 5);    // 5x Health Potion
  player.inventory[1] = ItemStack(2, 3);    // 3x Greater Health Potion
  player.inventory[2] = ItemStack(3, 1);    // Iron Sword
  player.inventory[3] = ItemStack(6, 1);    // Leather Armor
  player.inventory[5] = ItemStack(4, 1);    // Steel Sword
  player.inventory[10] = ItemStack(9, 10);  // 10x Mana Potion

  // Equip items
  player.equipment[EQUIPMENT_WEAPON_SLOT] = ItemStack(5, 1);  // Dragon Sword
  player.equipment[EQUIPMENT_ARMOR_SLOT] = ItemStack(7, 1);   // Iron Armor

  return player;
}

bool ServerGameState::findValidSpawnPosition(float& x, float& y) {
  if (collisionSystem &&
      !collisionSystem->isPositionValid(x, y, Config::Player::RADIUS)) {
    Logger::info("Default spawn invalid, searching...");

    for (float radius = Config::Gameplay::SPAWN_SEARCH_RADIUS_INCREMENT;
         radius < Config::Gameplay::SPAWN_SEARCH_MAX_RADIUS;
         radius += Config::Gameplay::SPAWN_SEARCH_RADIUS_INCREMENT) {
      for (float angle = 0; angle < 360.0f;
           angle += Config::Gameplay::SPAWN_SEARCH_ANGLE_INCREMENT) {
        float testX = 0.0f + radius * std::cos(angle * 3.14159f / 180.0f);
        float testY = 0.0f + radius * std::sin(angle * 3.14159f / 180.0f);
        if (collisionSystem->isPositionValid(testX, testY,
                                             Config::Player::RADIUS)) {
          x = testX;
          y = testY;
          return true;
        }
      }
    }
    return false;
  }
  return true;
}

void ServerGameState::assignPlayerColor(Player& player, size_t playerCount) {
  int colorIndex = playerCount % Config::Player::MAX_PLAYERS;
  player.r = Config::Player::COLORS[colorIndex].r;
  player.g = Config::Player::COLORS[colorIndex].g;
  player.b = Config::Player::COLORS[colorIndex].b;
}

void ServerGameState::checkPlayerDeaths() {
  for (auto& [id, player] : players) {
    // Skip already dead players
    if (player.deathTime > 0.0f) {
      continue;
    }

    // Check if player just died
    if (player.health <= 0.0f) {
      player.health = 0.0f;
      player.deathTime = static_cast<float>(serverTick);
      player.vx = 0.0f;
      player.vy = 0.0f;

      Logger::info("Player " + std::to_string(player.id) + " died at tick " +
                   std::to_string(serverTick));

      // Broadcast death event
      PlayerDiedPacket packet;
      packet.playerId = player.id;
      server->broadcastPacket(serialize(packet));
    }
  }
}

void ServerGameState::handlePlayerRespawns() {
  float respawnDelayTicks =
      Config::Gameplay::PLAYER_RESPAWN_DELAY / Config::Timing::TARGET_DELTA_MS;

  for (auto& [id, player] : players) {
    // Skip alive players
    if (player.deathTime == 0.0f) {
      continue;
    }

    // Check if respawn delay has elapsed
    float ticksSinceDeath = serverTick - player.deathTime;
    if (ticksSinceDeath >= respawnDelayTicks) {
      respawnPlayer(player);
    }
  }
}

void ServerGameState::respawnPlayer(Player& player) {
  // Reset health
  player.health = Config::Player::MAX_HEALTH;
  player.deathTime = 0.0f;
  player.vx = 0.0f;
  player.vy = 0.0f;

  // Select spawn point (round-robin)
  if (playerSpawns && !playerSpawns->empty()) {
    size_t spawnIndex = player.id % playerSpawns->size();
    const PlayerSpawn& spawn = (*playerSpawns)[spawnIndex];
    player.x = spawn.x;
    player.y = spawn.y;

    // Validate spawn position and search for valid position if blocked
    if (!findValidSpawnPosition(player.x, player.y)) {
      Logger::error("Failed to find valid respawn position near " + spawn.name);
    }

    Logger::info("Player " + std::to_string(player.id) + " respawned at " +
                 spawn.name + " (" + std::to_string(player.x) + ", " +
                 std::to_string(player.y) + ")");
  } else {
    // Fallback: world center (map is centered at origin)
    player.x = 0.0f;
    player.y = 0.0f;
    if (!findValidSpawnPosition(player.x, player.y)) {
      Logger::error("Failed to find valid respawn position");
    }

    Logger::info("Player " + std::to_string(player.id) +
                 " respawned at world center (no spawn points)");
  }

  // Broadcast respawn event
  PlayerRespawnedPacket packet;
  packet.playerId = player.id;
  packet.x = player.x;
  packet.y = player.y;
  server->broadcastPacket(serialize(packet));
}

void ServerGameState::processUseItem(uint32_t clientId, const uint8_t* data,
                                     size_t size) {
  if (size < 2) {
    Logger::info("Invalid UseItem packet size");
    return;
  }

  UseItemPacket packet = deserializeUseItem(data, size);

  uint32_t playerId = clientId;

  auto playerIt = players.find(playerId);
  if (playerIt == players.end()) {
    Logger::info("Player not found for UseItem");
    return;
  }

  Player& player = playerIt->second;

  // Validate slot index
  if (packet.slotIndex >= INVENTORY_SIZE) {
    Logger::info("Invalid inventory slot: " + std::to_string(packet.slotIndex));
    return;
  }

  ItemStack& stack = player.inventory[packet.slotIndex];
  if (stack.isEmpty()) {
    Logger::info("Attempted to use empty inventory slot");
    return;
  }

  // Get item definition
  const ItemDefinition* item = ItemRegistry::instance().getItem(stack.itemId);
  if (!item) {
    Logger::error("Invalid item ID in inventory: " +
                  std::to_string(stack.itemId));
    return;
  }

  // Only consumables can be used
  if (item->type != ItemType::Consumable) {
    Logger::info("Attempted to use non-consumable item");
    return;
  }

  // Apply consumable effect (healing)
  if (item->healAmount > 0) {
    // Don't allow using healing items at max health
    if (player.health >= Config::Player::MAX_HEALTH) {
      Logger::info("Player " + std::to_string(playerId) + " tried to use " +
                   item->name + " at full health");
      return;
    }

    float oldHealth = player.health;
    player.health =
        std::min(player.health + item->healAmount, Config::Player::MAX_HEALTH);
    Logger::info("Player " + std::to_string(playerId) + " used " + item->name +
                 " (healed from " + std::to_string(oldHealth) + " to " +
                 std::to_string(player.health) + ")");
  }

  // Consume one item
  stack.quantity--;
  if (stack.quantity <= 0) {
    stack = ItemStack();  // Clear slot
  }

  // Broadcast inventory update
  broadcastInventoryUpdate(playerId);
}

void ServerGameState::processEquipItem(uint32_t clientId, const uint8_t* data,
                                       size_t size) {
  if (size < 3) {
    Logger::info("Invalid EquipItem packet size");
    return;
  }

  EquipItemPacket packet = deserializeEquipItem(data, size);

  uint32_t playerId = clientId;

  auto playerIt = players.find(playerId);
  if (playerIt == players.end()) {
    Logger::info("Player not found for EquipItem");
    return;
  }

  Player& player = playerIt->second;

  // Validate equipment slot (255 = unequip)
  if (packet.equipmentSlot != 255 && packet.equipmentSlot >= EQUIPMENT_SLOTS) {
    Logger::info("Invalid equipment slot: " +
                 std::to_string(packet.equipmentSlot));
    return;
  }

  // Unequip case
  if (packet.equipmentSlot == 255) {
    // Find first empty inventory slot
    int emptySlot = player.findEmptySlot();
    if (emptySlot == -1) {
      Logger::info("No empty inventory slot for unequip");
      return;
    }

    // Move from equipment to inventory (use inventorySlot as equipment index)
    if (packet.inventorySlot >= EQUIPMENT_SLOTS) {
      Logger::info("Invalid equipment index for unequip");
      return;
    }

    ItemStack& equipSlot = player.equipment[packet.inventorySlot];
    if (!equipSlot.isEmpty()) {
      player.inventory[emptySlot] = equipSlot;
      equipSlot = ItemStack();
      Logger::info("Player " + std::to_string(playerId) +
                   " unequipped item to slot " + std::to_string(emptySlot));
    }

    broadcastInventoryUpdate(playerId);
    return;
  }

  // Equip case: validate inventory slot
  if (packet.inventorySlot >= INVENTORY_SIZE) {
    Logger::info("Invalid inventory slot: " +
                 std::to_string(packet.inventorySlot));
    return;
  }

  ItemStack& invStack = player.inventory[packet.inventorySlot];
  if (invStack.isEmpty()) {
    Logger::info("Attempted to equip from empty inventory slot");
    return;
  }

  // Validate item type matches equipment slot
  const ItemDefinition* item =
      ItemRegistry::instance().getItem(invStack.itemId);
  if (!item) {
    Logger::error("Invalid item ID: " + std::to_string(invStack.itemId));
    return;
  }

  if (packet.equipmentSlot == EQUIPMENT_WEAPON_SLOT &&
      item->type != ItemType::Weapon) {
    Logger::info("Attempted to equip non-weapon in weapon slot");
    return;
  }

  if (packet.equipmentSlot == EQUIPMENT_ARMOR_SLOT &&
      item->type != ItemType::Armor) {
    Logger::info("Attempted to equip non-armor in armor slot");
    return;
  }

  // Swap: if equipment slot has item, move it to inventory
  ItemStack& equipSlot = player.equipment[packet.equipmentSlot];
  if (!equipSlot.isEmpty()) {
    // Swap
    ItemStack temp = equipSlot;
    equipSlot = invStack;
    invStack = temp;
    Logger::info("Player " + std::to_string(playerId) + " swapped equipment");
  } else {
    // Simple equip
    equipSlot = invStack;
    invStack = ItemStack();
    Logger::info("Player " + std::to_string(playerId) + " equipped " +
                 item->name);
  }

  broadcastInventoryUpdate(playerId);
}

void ServerGameState::broadcastInventoryUpdate(uint32_t playerId) {
  auto playerIt = players.find(playerId);
  if (playerIt == players.end()) {
    return;
  }

  const Player& player = playerIt->second;

  InventoryUpdatePacket packet;
  packet.playerId = playerId;

  // Copy inventory
  for (int i = 0; i < INVENTORY_SIZE; i++) {
    packet.inventory[i].itemId = player.inventory[i].itemId;
    packet.inventory[i].quantity = player.inventory[i].quantity;
  }

  // Copy equipment
  for (int i = 0; i < EQUIPMENT_SLOTS; i++) {
    packet.equipment[i].itemId = player.equipment[i].itemId;
    packet.equipment[i].quantity = player.equipment[i].quantity;
  }

  server->broadcastPacket(serialize(packet));
}

// World item management

void ServerGameState::checkEnemyLootDrops() {
  if (!enemySystem) {
    return;
  }

  const auto& deaths = enemySystem->getDiedThisFrame();
  for (const auto& death : deaths) {
    // Simple loot table: All enemies drop Health Potion (itemId=1)
    uint32_t lootItemId = 1;

    const auto& enemies = enemySystem->getEnemies();
    auto enemyIt = enemies.find(death.enemyId);
    if (enemyIt != enemies.end()) {
      const Enemy& enemy = enemyIt->second;
      spawnWorldItem(lootItemId, enemy.x, enemy.y);

      Logger::info("Enemy " + std::to_string(death.enemyId) + " dropped item " +
                   std::to_string(lootItemId));
    }
  }
}

void ServerGameState::spawnWorldItem(uint32_t itemId, float x, float y) {
  uint32_t worldItemId = nextWorldItemId++;

  WorldItem worldItem(worldItemId, itemId, x, y,
                      static_cast<float>(serverTick));
  worldItems[worldItemId] = worldItem;

  // Broadcast spawn to all clients
  ItemSpawnedPacket packet;
  packet.worldItemId = worldItemId;
  packet.itemId = itemId;
  packet.x = x;
  packet.y = y;

  server->broadcastPacket(serialize(packet));

  Logger::debug("Spawned world item " + std::to_string(worldItemId) + " at (" +
                std::to_string(x) + ", " + std::to_string(y) + ")");
}

void ServerGameState::processItemPickupRequest(uint32_t clientId,
                                               const uint8_t* data,
                                               size_t size) {
  if (size < 5) {
    Logger::info("Invalid ItemPickupRequest packet size");
    return;
  }

  ItemPickupRequestPacket packet = deserializeItemPickupRequest(data, size);

  uint32_t playerId = clientId;

  auto playerIt = players.find(playerId);
  if (playerIt == players.end()) {
    return;
  }
  Player& player = playerIt->second;

  // Validate world item exists
  auto itemIt = worldItems.find(packet.worldItemId);
  if (itemIt == worldItems.end()) {
    Logger::debug("World item " + std::to_string(packet.worldItemId) +
                  " does not exist (already picked up?)");
    return;
  }
  const WorldItem& worldItem = itemIt->second;

  // Validate distance (anti-cheat)
  constexpr float PICKUP_RADIUS = 32.0f;
  float dx = player.x - worldItem.x;
  float dy = player.y - worldItem.y;
  float distanceSquared = dx * dx + dy * dy;

  if (distanceSquared > PICKUP_RADIUS * PICKUP_RADIUS) {
    Logger::info("Player " + std::to_string(playerId) + " too far from item " +
                 std::to_string(packet.worldItemId));
    return;
  }

  // Validate inventory space
  int emptySlot = player.findEmptySlot();
  if (emptySlot == -1) {
    Logger::info("Player " + std::to_string(playerId) +
                 " inventory full, cannot pick up item");
    return;
  }

  const ItemDefinition* itemDef =
      ItemRegistry::instance().getItem(worldItem.itemId);
  if (!itemDef) {
    Logger::error("Invalid item ID: " + std::to_string(worldItem.itemId));
    return;
  }

  // Check for stacking (consumables)
  if (itemDef->maxStackSize > 1) {
    int existingSlot = player.findItemStack(worldItem.itemId);
    if (existingSlot != -1) {
      ItemStack& stack = player.inventory[existingSlot];
      if (stack.quantity < itemDef->maxStackSize) {
        stack.quantity++;

        Logger::info("Player " + std::to_string(playerId) + " picked up " +
                     itemDef->name + " (stacked to " +
                     std::to_string(stack.quantity) + ")");

        // Remove from world
        worldItems.erase(packet.worldItemId);

        // Broadcast pickup
        ItemPickedUpPacket pickupPacket;
        pickupPacket.worldItemId = packet.worldItemId;
        pickupPacket.playerId = playerId;
        server->broadcastPacket(serialize(pickupPacket));

        // Update inventory
        broadcastInventoryUpdate(playerId);
        return;
      }
    }
  }

  // Add to inventory (new stack or non-stackable item)
  player.inventory[emptySlot] = ItemStack(worldItem.itemId, 1);

  Logger::info("Player " + std::to_string(playerId) + " picked up " +
               itemDef->name);

  // Remove from world
  worldItems.erase(packet.worldItemId);

  // Broadcast pickup
  ItemPickedUpPacket pickupPacket;
  pickupPacket.worldItemId = packet.worldItemId;
  pickupPacket.playerId = playerId;
  server->broadcastPacket(serialize(pickupPacket));

  // Update inventory
  broadcastInventoryUpdate(playerId);
}

void ServerGameState::processObjectiveInteract(uint32_t clientId,
                                               const uint8_t* data,
                                               size_t size) {
  if (size < 5) {
    Logger::info("Invalid ObjectiveInteract packet size");
    return;
  }

  ObjectiveInteractPacket packet = deserializeObjectiveInteract(data, size);

  uint32_t playerId = clientId;

  auto playerIt = players.find(playerId);
  if (playerIt == players.end()) {
    return;
  }
  const Player& player = playerIt->second;

  if (!objectiveSystem) {
    return;
  }

  // Try to interact with nearest objective
  if (objectiveSystem->tryInteract(playerId, player.x, player.y)) {
    Logger::info("Player " + std::to_string(playerId) +
                 " started objective interaction");
  } else {
    Logger::debug("Player " + std::to_string(playerId) +
                  " failed to interact with objective (not in range or already "
                  "in progress)");
  }
}

void ServerGameState::broadcastObjectiveState(uint32_t objectiveId) {
  if (!objectiveSystem) {
    return;
  }

  const Objective* obj = objectiveSystem->getObjective(objectiveId);
  if (!obj) {
    return;
  }

  ObjectiveStatePacket packet;
  packet.objectiveId = obj->id;
  packet.objectiveType = static_cast<uint8_t>(obj->type);
  packet.objectiveState = static_cast<uint8_t>(obj->state);
  packet.x = obj->x;
  packet.y = obj->y;
  packet.radius = obj->radius;
  packet.progress = obj->getProgress();
  packet.enemiesRequired = obj->enemiesRequired;
  packet.enemiesKilled = obj->enemiesKilled;

  server->broadcastPacket(serialize(packet));

  Logger::debug("Broadcast objective state: id=" + std::to_string(obj->id) +
                " state=" + objectiveStateToString(obj->state) +
                " progress=" + std::to_string(obj->getProgress()));
}

void ServerGameState::broadcastAllObjectives(uint32_t clientId) {
  if (!objectiveSystem) {
    return;
  }

  const auto& objectives = objectiveSystem->getObjectives();
  for (const auto& obj : objectives) {
    ObjectiveStatePacket packet;
    packet.objectiveId = obj.id;
    packet.objectiveType = static_cast<uint8_t>(obj.type);
    packet.objectiveState = static_cast<uint8_t>(obj.state);
    packet.x = obj.x;
    packet.y = obj.y;
    packet.radius = obj.radius;
    packet.progress = obj.getProgress();
    packet.enemiesRequired = obj.enemiesRequired;
    packet.enemiesKilled = obj.enemiesKilled;

    server->send(clientId, serialize(packet));
  }

  Logger::info("Sent " + std::to_string(objectives.size()) +
               " objectives to new client");
}
