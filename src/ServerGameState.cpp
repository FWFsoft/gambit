#include "ServerGameState.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "AbilitySystem.h"
#include "CollisionSystem.h"
#include "EnemySystem.h"
#include "MatchDirector.h"
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

  // Initialize enemy system (camp guards come from the map; everything else
  // is spawned by the match director)
  enemySystem = std::make_unique<EnemySystem>(
      world.tiledMap != nullptr ? world.tiledMap->getEnemySpawns() : noSpawns);
  enemySystem->spawnAllEnemies();

  // Initialize effect manager
  effectManager = std::make_unique<EffectManager>();
  Logger::info("EffectManager initialized");

  // Load ship position from map
  if (world.tiledMap != nullptr && world.tiledMap->hasShipPosition()) {
    shipX = world.tiledMap->getShipX();
    shipY = world.tiledMap->getShipY();
    hasShip = true;
    Logger::info("Ship position loaded: (" + std::to_string(shipX) + ", " +
                 std::to_string(shipY) + ")");
  }

  // Combat + match flow
  abilitySystem = std::make_unique<AbilitySystem>(
      players, enemySystem.get(), effectManager.get(), collisionSystem,
      worldWidth, worldHeight, [this](const GameEventPacket& packet) {
        this->server->broadcastPacket(serialize(packet));
      });
  enemySystem->setCombat(abilitySystem.get());

  MatchDirector::Hooks hooks;
  hooks.spawnPickup = [this](uint32_t itemId, float x, float y) {
    spawnWorldItem(itemId, x, y);
  };
  hooks.clearPickups = [this]() { clearArenaPickups(); };
  director = std::make_unique<MatchDirector>(
      players, enemySystem.get(), abilitySystem.get(), shipX, shipY,
      worldWidth, worldHeight, hooks);

  // Damage-over-time ticks show numbers and count toward the source's score.
  effectManager->setDotCallback([this](const EffectManager::DotTick& tick) {
    GameEventPacket number;
    number.kind = tick.isHeal ? GameEventKind::Heal : GameEventKind::Damage;
    number.sourceId = tick.entityId;
    number.sourceIsEnemy = tick.isEnemy;
    number.x = tick.x;
    number.y = tick.y;
    number.value = tick.amount;
    number.param = tick.isEnemy ? 4 : 6;  // 4 = DoT, 2 = on a player
    this->server->broadcastPacket(serialize(number));
    if (tick.isEnemy && !tick.isHeal) {
      auto it = players.find(tick.sourceId);
      if (it != players.end()) it->second.scoreAccum += tick.amount * 0.12f;
    }
  });

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

    // Set up completion callback for rewards and effects
    objectiveSystem->setCompletionCallback(
        [this](uint32_t objectiveId, uint32_t playerId) {
          const Objective* obj = objectiveSystem->getObjective(objectiveId);
          if (!obj) return;

          // Score, ship repair and loot for the match
          auto completer = players.find(playerId);
          if (director && completer != players.end()) {
            director->onObjectiveCompleted(
                completer->second, obj->type == ObjectiveType::AlienScrapyard);
          }

          // Remove Slow debuff from player (applied during pickup phase)
          if ((obj->type == ObjectiveType::AlienScrapyard ||
               obj->type == ObjectiveType::RecoverProbe) &&
              effectManager) {
            effectManager->cleanseDebuff(playerId, EffectType::Slow);
            Logger::info("Removed Slow from player " +
                         std::to_string(playerId) +
                         " on objective completion");
          }

          // CaptureOutpost: disable enemy spawns in radius
          if (obj->type == ObjectiveType::CaptureOutpost && enemySystem) {
            enemySystem->disableSpawnsInRadius(obj->x, obj->y, obj->radius);
          }

          // SalvageMedpacks: spawn healing pickups
          if (obj->type == ObjectiveType::SalvageMedpacks) {
            for (int i = 0; i < 3; i++) {
              float offsetX = (i - 1) * 30.0f;
              spawnWorldItem(1, obj->x + offsetX, obj->y);
            }
            Logger::info("Spawned 3 Health Potions for SalvageMedpacks at (" +
                         std::to_string(obj->x) + ", " +
                         std::to_string(obj->y) + ")");
          }

          // RecoverProbe: spawn a loot item at the deposit point
          if (obj->type == ObjectiveType::RecoverProbe) {
            float lootX = obj->hasDepositPoint ? obj->depositX : obj->x;
            float lootY = obj->hasDepositPoint ? obj->depositY : obj->y;
            spawnWorldItem(1, lootX, lootY);
            Logger::info("Spawned probe reward at (" +
                         std::to_string(lootX) + ", " +
                         std::to_string(lootY) + ")");
          }

          // NoxiousGas: gas disperses, spawn loot at spore location
          if (obj->type == ObjectiveType::NoxiousGas) {
            spawnWorldItem(1, obj->x, obj->y);
            Logger::info("NoxiousGas cleared - loot spawned at (" +
                         std::to_string(obj->x) + ", " +
                         std::to_string(obj->y) + ")");
          }

          // LittleJohn: if guardian still alive, apply Berserk to it
          if (obj->type == ObjectiveType::LittleJohn && enemySystem) {
            if (obj->guardianEnemyId != 0) {
              auto& enemies = enemySystem->getEnemies();
              auto it = enemies.find(obj->guardianEnemyId);
              if (it != enemies.end() &&
                  it->second.state != EnemyState::Dead) {
                effectManager->applyEffect(obj->guardianEnemyId,
                                           EffectType::Berserk, 5, 999999.0f,
                                           0, enemySystem->getEnemies());
                Logger::info("LittleJohn gate opened - guardian " +
                             std::to_string(obj->guardianEnemyId) +
                             " enrages with Berserk x5");
              }
            }
          }

          // DigPitTrap: trap is now armed — no immediate reward
          if (obj->type == ObjectiveType::DigPitTrap) {
            Logger::info("DigPitTrap '" + obj->name +
                         "' armed and waiting for victims");
          }

          // StringTripwire: trap is now armed — no immediate reward
          if (obj->type == ObjectiveType::StringTripwire) {
            Logger::info("StringTripwire '" + obj->name +
                         "' armed and waiting for victims");
          }

          // DerelictTurret: turret repaired — now auto-attacks enemies
          if (obj->type == ObjectiveType::DerelictTurret) {
            Logger::info("DerelictTurret '" + obj->name +
                         "' repaired and online — auto-turret active");
          }

          // SaveTheFrogs: spawn healing pickups at pond
          if (obj->type == ObjectiveType::SaveTheFrogs) {
            float lootX = obj->hasDepositPoint ? obj->depositX : obj->x;
            float lootY = obj->hasDepositPoint ? obj->depositY : obj->y;
            for (int i = 0; i < 2; i++) {
              float offsetX = (i == 0) ? -20.0f : 20.0f;
              spawnWorldItem(1, lootX + offsetX, lootY);
            }
            Logger::info("Spawned frog rescue rewards at pond");
          }
        });

    // Set up stop callback for removing Slow when player stops interacting
    objectiveSystem->setStopCallback(
        [this](uint32_t objectiveId, uint32_t playerId) {
          const Objective* obj = objectiveSystem->getObjective(objectiveId);
          if (!obj) return;

          // Remove Slow debuff when player stops interacting
          if ((obj->type == ObjectiveType::AlienScrapyard ||
               obj->type == ObjectiveType::RecoverProbe) &&
              effectManager) {
            effectManager->cleanseDebuff(playerId, EffectType::Slow);
            Logger::info("Removed Slow from player " +
                         std::to_string(playerId) +
                         " on interaction stop");
          }
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

  // Link LittleJohn guardian enemies now that both systems are initialized
  if (objectiveSystem && enemySystem) {
    initializeLittleJohnGuardians();
  }
}

ServerGameState::~ServerGameState() = default;

void ServerGameState::onClientConnected(const ClientConnectedEvent& e) {
  uint32_t playerId = e.clientId;

  Player player = createPlayer(playerId);
  assignPlayerColor(player, players.size());

  players[playerId] = player;
  director->onPlayerJoined(players[playerId]);

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

  // Send ship position to new client
  if (hasShip) {
    ShipLocationPacket shipPacket;
    shipPacket.x = shipX;
    shipPacket.y = shipY;
    server->send(e.clientId, serialize(shipPacket));
  }
}

void ServerGameState::onClientDisconnected(const ClientDisconnectedEvent& e) {
  uint32_t playerId = e.clientId;

  players.erase(playerId);
  director->onPlayerLeft(playerId);
  effectManager->clearEntity(playerId, false);

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

    case PacketType::AttackEnemy:
      // Legacy auto-attack packet. Combat now runs through AbilitySystem
      // from the buttons carried in ClientInput.
      break;

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

      auto playerIt = players.find(e.clientId);
      if (playerIt != players.end()) {
        director->onCharacterSelected(playerIt->second, charPacket.characterId);
        Logger::info("Player " + std::to_string(e.clientId) +
                     " selected character ID " +
                     std::to_string(charPacket.characterId));
      }
      break;
    }

    case PacketType::ItemPickupRequest:
      processItemPickupRequest(e.clientId, e.data, e.size);
      break;

    case PacketType::ObjectiveInteract:
      processObjectiveInteract(e.clientId, e.data, e.size);
      break;

    case PacketType::InfusionSlot: {
      if (e.size < 3) break;
      InfusionSlotPacket packet = deserializeInfusionSlot(e.data, e.size);
      auto playerIt = players.find(e.clientId);
      if (playerIt != players.end()) {
        director->onSlotInfusion(playerIt->second,
                                 static_cast<Arena::Genus>(packet.genus),
                                 packet.slot);
      }
      break;
    }

    case PacketType::PlayerReady: {
      if (e.size < 2) break;
      PlayerReadyPacket packet = deserializePlayerReady(e.data, e.size);
      auto playerIt = players.find(e.clientId);
      if (playerIt != players.end()) {
        director->onReady(playerIt->second, packet.ready != 0);
      }
      break;
    }

    case PacketType::DebugCommand: {
      if (e.size < 3) break;
      DebugCommandPacket packet = deserializeDebugCommand(e.data, e.size);
      auto playerIt = players.find(e.clientId);
      if (playerIt != players.end()) {
        director->onDebugCommand(
            playerIt->second, static_cast<DebugCommandKind>(packet.command),
            packet.arg);
      }
      break;
    }

    default:
      Logger::info("Unknown packet type: " +
                   std::to_string(static_cast<int>(type)));
      break;
  }
}

void ServerGameState::processClientInput(uint32_t clientId, const uint8_t* data,
                                         size_t size) {
  if (size < 15) {
    Logger::info("Invalid ClientInput packet size");
    return;
  }
  ClientInputPacket input = deserializeClientInput(data, size);

  uint32_t playerId = clientId;
  auto playerIt = players.find(playerId);
  assert(playerIt != players.end());

  Player& player = playerIt->second;

  // Validate input sequence (prevent replay attacks)
  if (input.inputSequence <= player.lastInputSequence &&
      player.lastInputSequence != 0) {
    return;
  }
  player.lastInputSequence = input.inputSequence;

  // Aim and ability buttons are consumed by AbilitySystem on the next tick.
  abilitySystem->setInput(player, input.buttons, input.aimX, input.aimY);

  // No walking while dead, still choosing a character, or mid-dash.
  if (player.isDead() || player.characterId == 0 || player.forcedMs > 0.0f) {
    return;
  }

  MovementInput movementInput(input.moveLeft, input.moveRight, input.moveUp,
                              input.moveDown, Config::Timing::TARGET_DELTA_MS,
                              worldWidth, worldHeight, collisionSystem);

  // speedMult already folds in effects, kit and infusion bonuses; the same
  // value is sent to the client so prediction matches.
  MovementModifiers modifiers;
  modifiers.speedMultiplier = player.speedMult;
  modifiers.canMove =
      effectManager->calculateModifiers(playerId, false).canMove;

  applyInput(player, movementInput, modifiers);
}

void ServerGameState::onUpdate(const UpdateEvent& e) {
  serverTick++;

  // React to everything that died since the last tick: score, loot, kit
  // hooks, objectives, and telling the clients.
  handleEnemyDeaths();

  // Match flow: phases, spawns, the zone, the ship, revives.
  director->update(e.deltaTime);

  // Player abilities, projectiles, ground zones.
  abilitySystem->update(e.deltaTime);

  // Enemy AI (attacks go through AbilitySystem's damage pipeline).
  enemySystem->update(e.deltaTime, players, effectManager.get());

  // Effects (DoT/HoT, duration ticking)
  effectManager->update(e.deltaTime, players, enemySystem->getEnemies(),
                        enemySystem.get());

  // Objective side effects: gas damage, LittleJohn activation
  if (objectiveSystem && effectManager) {
    updateObjectiveSideEffects(e.deltaTime);
  }

  // Update objective system (handles interaction timers)
  if (objectiveSystem) {
    objectiveSystem->update(e.deltaTime);

    // Check if players moved out of objective range
    auto interactions =
        objectiveSystem->getPlayerInteractions();  // copy — stopInteraction
                                                   // mutates
    for (const auto& [playerId, objId] : interactions) {
      auto playerIt = players.find(playerId);
      const Objective* obj = objectiveSystem->getObjective(objId);
      if (playerIt != players.end() && obj) {
        if (!obj->isInRange(playerIt->second.x, playerIt->second.y)) {
          objectiveSystem->stopInteraction(playerId);
        }
      }
    }
  }

  processArenaPickups();

  // Broadcast state update every frame
  broadcastStateUpdate();
}

void ServerGameState::handleEnemyDeaths() {
  auto deaths = enemySystem->takeDeaths();
  for (const auto& death : deaths) {
    const Enemy* enemy = enemySystem->find(death.enemyId);
    if (enemy) {
      director->onEnemyKilled(*enemy, death.killerId);
      abilitySystem->onEnemyDied(*enemy, death.killerId);
      if (objectiveSystem) objectiveSystem->onEnemyDeath(enemy->x, enemy->y);
    }

    EnemyDiedPacket deathPacket;
    deathPacket.enemyId = death.enemyId;
    deathPacket.killerId = death.killerId;
    server->broadcastPacket(serialize(deathPacket));
  }
}

void ServerGameState::processArenaPickups() {
  if (worldItems.empty()) return;
  const float radius = Arena::Tuning::PICKUP_RADIUS;

  std::vector<std::pair<uint32_t, uint32_t>> taken;  // worldItemId, playerId
  for (const auto& [worldItemId, item] : worldItems) {
    if (item.itemId < Arena::PickupId::InfusionBase) continue;
    for (auto& [playerId, player] : players) {
      float dx = player.x - item.x;
      float dy = player.y - item.y;
      if (dx * dx + dy * dy > radius * radius) continue;
      if (director->onPickup(player, item.itemId)) {
        taken.emplace_back(worldItemId, playerId);
        break;
      }
    }
  }
  for (const auto& [worldItemId, playerId] : taken) {
    worldItems.erase(worldItemId);
    ItemPickedUpPacket packet;
    packet.worldItemId = worldItemId;
    packet.playerId = playerId;
    server->broadcastPacket(serialize(packet));
  }
}

void ServerGameState::clearArenaPickups() {
  std::vector<uint32_t> ids;
  for (const auto& [worldItemId, item] : worldItems) {
    if (item.itemId >= Arena::PickupId::InfusionBase) ids.push_back(worldItemId);
  }
  for (uint32_t worldItemId : ids) {
    worldItems.erase(worldItemId);
    ItemPickedUpPacket packet;
    packet.worldItemId = worldItemId;
    packet.playerId = 0;  // nobody: it just disappears
    server->broadcastPacket(serialize(packet));
  }
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
    ps.maxHealth = player.maxHealth;
    ps.shield = player.shield;
    ps.characterId = static_cast<uint8_t>(player.characterId);
    ps.flags = player.stateFlags;
    ps.speedMult = player.speedMult;
    ps.aimX = player.aimX;
    ps.aimY = player.aimY;
    ps.chargeSlot =
        player.chargeSlot >= 0 ? static_cast<uint8_t>(player.chargeSlot) : 255;
    ps.charge = 0;
    if (player.chargeSlot >= 0) {
      const Arena::KitInfo* kit = Arena::kitInfo(player.characterId);
      if (kit && player.chargeSlot < Arena::KIT_SLOT_COUNT) {
        const Arena::AbilityInfo& ability = kit->abilities[player.chargeSlot];
        float charge = ability.mode == Arena::AbilityMode::Charge
                           ? player.chargeMs / ability.maxChargeMs
                           : 1.0f;
        ps.charge = static_cast<uint8_t>(
            std::min(255.0f, std::max(0.0f, charge) * 255.0f));
      }
    }
    for (int slot = 0; slot < Arena::SLOT_COUNT; slot++) {
      ps.cooldowns[slot] = static_cast<uint16_t>(
          std::min(65535.0f, player.cooldownMs[slot] / 10.0f));
    }
    ps.resource = player.resource;
    packet.players.push_back(ps);
  }

  server->broadcastPacket(serialize(packet));

  // Enemy state
  {
    EnemyStateUpdatePacket enemyPacket;
    for (const auto& [id, enemy] : enemySystem->getEnemies()) {
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
      state.flags = enemy.netFlags;
      state.windup = 0;
      if (enemy.state == EnemyState::Attack && enemy.windupTotalMs > 0.0f) {
        float progress = 1.0f - enemy.windupMs / enemy.windupTotalMs;
        state.windup = static_cast<uint8_t>(
            std::min(255.0f, std::max(0.0f, progress) * 255.0f));
      }
      enemyPacket.enemies.push_back(state);
    }
    server->broadcastPacket(serialize(enemyPacket));
  }

  // Match state and effects change slowly: ten times a second is plenty.
  if (serverTick % 6 == 0) {
    server->broadcastPacket(serialize(director->buildState()));
    broadcastEffects();
  }
}

void ServerGameState::broadcastEffects() {
  auto sendFor = [this](uint32_t id, bool isEnemy, const ActiveEffects& effects) {
    EffectUpdatePacket packet;
    packet.targetId = id;
    packet.isEnemy = isEnemy;
    for (const auto& effect : effects.effects) {
      NetworkEffect ne;
      ne.effectType = static_cast<uint8_t>(effect.type);
      ne.stacks = effect.stacks;
      ne.remainingDuration = effect.remainingDuration;
      packet.effects.push_back(ne);
    }
    server->broadcastPacket(serialize(packet));
  };

  // Anything that had effects last time gets one more (possibly empty)
  // update so clients drop effects that have expired.
  std::vector<uint32_t> nowPlayers;
  for (const auto& [id, player] : players) {
    const ActiveEffects& effects = effectManager->getPlayerEffects(id);
    bool had = std::find(playersWithEffects.begin(), playersWithEffects.end(),
                         id) != playersWithEffects.end();
    if (effects.effects.empty() && !had) continue;
    sendFor(id, false, effects);
    if (!effects.effects.empty()) nowPlayers.push_back(id);
  }
  playersWithEffects.swap(nowPlayers);

  std::vector<uint32_t> nowEnemies;
  for (const auto& [id, enemy] : enemySystem->getEnemies()) {
    const ActiveEffects& effects = effectManager->getEnemyEffects(id);
    bool had = std::find(enemiesWithEffects.begin(), enemiesWithEffects.end(),
                         id) != enemiesWithEffects.end();
    if (effects.effects.empty() && !had) continue;
    sendFor(id, true, effects);
    if (!effects.effects.empty()) nowEnemies.push_back(id);
  }
  enemiesWithEffects.swap(nowEnemies);
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

  Logger::info("ObjectiveInteract: player " + std::to_string(playerId) +
               " pos=(" + std::to_string(player.x) + "," +
               std::to_string(player.y) + ")");

  // Try to interact with nearest objective
  if (objectiveSystem->tryInteract(playerId, player.x, player.y)) {
    Logger::info("Player " + std::to_string(playerId) +
                 " started objective interaction");

    // Apply Slow debuff for AlienScrapyard interactions
    if (effectManager) {
      // Check if the player is now interacting with an AlienScrapyard
      const auto& interactions = objectiveSystem->getPlayerInteractions();
      auto interIt = interactions.find(playerId);
      if (interIt != interactions.end()) {
        const Objective* obj =
            objectiveSystem->getObjective(interIt->second);
        if (obj && (obj->type == ObjectiveType::AlienScrapyard ||
                    obj->type == ObjectiveType::RecoverProbe)) {
          effectManager->applyEffect(playerId, EffectType::Slow, 1, 999999.0f,
                                     0, players);
          Logger::info("Applied Slow to player " + std::to_string(playerId) +
                       " for " + objectiveTypeToString(obj->type) +
                       " interaction");
        }
      }
    }
  } else {
    Logger::info("Player " + std::to_string(playerId) +
                 " failed to interact: pos=(" + std::to_string(player.x) +
                 "," + std::to_string(player.y) +
                 ") not in range or already interacting");
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
  // SaveTheFrogs reuses enemiesRequired/Killed fields for frog counts
  if (obj->type == ObjectiveType::SaveTheFrogs) {
    packet.enemiesRequired = obj->frogsRequired;
    packet.enemiesKilled = obj->frogsDeposited;
  } else {
    packet.enemiesRequired = obj->enemiesRequired;
    packet.enemiesKilled = obj->enemiesKilled;
  }
  packet.depositX = obj->depositX;
  packet.depositY = obj->depositY;

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
    if (obj.type == ObjectiveType::SaveTheFrogs) {
      packet.enemiesRequired = obj.frogsRequired;
      packet.enemiesKilled = obj.frogsDeposited;
    } else {
      packet.enemiesRequired = obj.enemiesRequired;
      packet.enemiesKilled = obj.enemiesKilled;
    }
    packet.depositX = obj.depositX;
    packet.depositY = obj.depositY;

    server->send(clientId, serialize(packet));
  }

  Logger::info("Sent " + std::to_string(objectives.size()) +
               " objectives to new client");
}

void ServerGameState::updateObjectiveSideEffects(float deltaTime) {
  auto& objectives = objectiveSystem->getObjectivesMutable();

  for (auto& obj : objectives) {
    // NoxiousGas: apply Wound to players in zone while not completed
    if (obj.type == ObjectiveType::NoxiousGas &&
        obj.state != ObjectiveState::Completed) {
      obj.gasDamageTimer += deltaTime;
      if (obj.gasDamageTimer >= 2000.0f) {  // Tick every 2 seconds
        obj.gasDamageTimer = 0.0f;
        for (auto& [playerId, player] : players) {
          if (player.isAlive() && obj.isInRange(player.x, player.y)) {
            effectManager->applyEffect(playerId, EffectType::Wound, 1,
                                       4000.0f, 0, players);
          }
        }
      }
    }

    // DigPitTrap: once completed, fire Snared+Wound at any entity that enters
    if (obj.type == ObjectiveType::DigPitTrap &&
        obj.state == ObjectiveState::Completed && !obj.pitTrapArmed) {
      // Collect entity IDs in range first to avoid mutation during iteration
      std::vector<uint32_t> trappedPlayers;
      std::vector<uint32_t> trappedEnemies;

      for (const auto& [playerId, player] : players) {
        if (player.isAlive() && obj.isInRange(player.x, player.y)) {
          trappedPlayers.push_back(playerId);
        }
      }
      if (enemySystem) {
        for (const auto& [enemyId, enemy] : enemySystem->getEnemies()) {
          if (enemy.state != EnemyState::Dead &&
              obj.isInRange(enemy.x, enemy.y)) {
            trappedEnemies.push_back(enemyId);
          }
        }
      }

      if (!trappedPlayers.empty() || !trappedEnemies.empty()) {
        obj.pitTrapArmed = true;
        Logger::info("DigPitTrap '" + obj.name + "' triggered!");

        for (uint32_t pid : trappedPlayers) {
          effectManager->applyEffect(pid, EffectType::Snared, 1, 5000.0f, 0,
                                     players);
          effectManager->applyEffect(pid, EffectType::Wound, 1, 5000.0f, 0,
                                     players);
          Logger::info("  Player " + std::to_string(pid) + " caught in trap");
        }
        if (enemySystem) {
          for (uint32_t eid : trappedEnemies) {
            effectManager->applyEffect(eid, EffectType::Snared, 1, 5000.0f, 0,
                                       enemySystem->getEnemies());
            effectManager->applyEffect(eid, EffectType::Wound, 1, 5000.0f, 0,
                                       enemySystem->getEnemies());
            Logger::info("  Enemy " + std::to_string(eid) + " caught in trap");
          }
        }
      }
    }

    // StringTripwire: once completed, fire Wound+Snared at any entity that
    // enters (affects friendlies too, one-time trigger like DigPitTrap)
    if (obj.type == ObjectiveType::StringTripwire &&
        obj.state == ObjectiveState::Completed && !obj.tripwireArmed) {
      std::vector<uint32_t> trappedPlayers;
      std::vector<uint32_t> trappedEnemies;

      for (const auto& [playerId, player] : players) {
        if (player.isAlive() && obj.isInRange(player.x, player.y)) {
          trappedPlayers.push_back(playerId);
        }
      }
      if (enemySystem) {
        for (const auto& [enemyId, enemy] : enemySystem->getEnemies()) {
          if (enemy.state != EnemyState::Dead &&
              obj.isInRange(enemy.x, enemy.y)) {
            trappedEnemies.push_back(enemyId);
          }
        }
      }

      if (!trappedPlayers.empty() || !trappedEnemies.empty()) {
        obj.tripwireArmed = true;
        Logger::info("StringTripwire '" + obj.name + "' triggered!");

        for (uint32_t pid : trappedPlayers) {
          effectManager->applyEffect(pid, EffectType::Wound, 1, 5000.0f, 0,
                                     players);
          effectManager->applyEffect(pid, EffectType::Snared, 1, 5000.0f, 0,
                                     players);
          Logger::info("  Player " + std::to_string(pid) +
                       " caught in tripwire");
        }
        if (enemySystem) {
          for (uint32_t eid : trappedEnemies) {
            effectManager->applyEffect(eid, EffectType::Wound, 1, 5000.0f, 0,
                                       enemySystem->getEnemies());
            effectManager->applyEffect(eid, EffectType::Snared, 1, 5000.0f, 0,
                                       enemySystem->getEnemies());
            Logger::info("  Enemy " + std::to_string(eid) +
                         " caught in tripwire");
          }
        }
      }
    }

    // DerelictTurret: once repaired, auto-attack enemies in radius
    if (obj.type == ObjectiveType::DerelictTurret &&
        obj.state == ObjectiveState::Completed && enemySystem) {
      obj.turretActive = true;
      obj.turretDamageTimer += deltaTime;
      if (obj.turretDamageTimer >= 3000.0f) {  // Fire every 3 seconds
        obj.turretDamageTimer = 0.0f;
        for (auto& [enemyId, enemy] : enemySystem->getEnemies()) {
          if (enemy.state != EnemyState::Dead &&
              obj.isInRange(enemy.x, enemy.y)) {
            effectManager->applyEffect(enemyId, EffectType::Wound, 2, 3000.0f,
                                       0, enemySystem->getEnemies());
            Logger::info("DerelictTurret '" + obj.name + "' fired at enemy " +
                         std::to_string(enemyId));
          }
        }
      }
    }

    // LittleJohn: activate guardian when a player enters the zone
    if (obj.type == ObjectiveType::LittleJohn &&
        obj.state == ObjectiveState::Inactive && obj.guardianEnemyId != 0) {
      for (const auto& [playerId, player] : players) {
        if (player.isAlive() && obj.isInRange(player.x, player.y)) {
          // Player entered gate zone — activate guardian and start objective
          auto& enemies = enemySystem->getEnemies();
          auto it = enemies.find(obj.guardianEnemyId);
          if (it != enemies.end()) {
            it->second.passive = false;
            Logger::info("LittleJohn guardian " +
                         std::to_string(obj.guardianEnemyId) +
                         " activated by player " + std::to_string(playerId));
          }
          obj.state = ObjectiveState::InProgress;
          broadcastObjectiveState(obj.id);
          break;
        }
      }
    }
  }
}

void ServerGameState::initializeLittleJohnGuardians() {
  auto& objectives = objectiveSystem->getObjectivesMutable();
  auto& enemies = enemySystem->getEnemies();

  for (auto& obj : objectives) {
    if (obj.type != ObjectiveType::LittleJohn) continue;

    // Find nearest enemy within the objective radius and mark it as guardian
    uint32_t nearestId = 0;
    float nearestDistSq = std::numeric_limits<float>::max();

    for (auto& [enemyId, enemy] : enemies) {
      if (enemy.state == EnemyState::Dead) continue;
      float dx = enemy.x - obj.x;
      float dy = enemy.y - obj.y;
      float distSq = dx * dx + dy * dy;
      float searchRadius = obj.radius * 2.0f;  // Search a bit wider than zone
      if (distSq < searchRadius * searchRadius && distSq < nearestDistSq) {
        nearestDistSq = distSq;
        nearestId = enemyId;
      }
    }

    if (nearestId != 0) {
      obj.guardianEnemyId = nearestId;
      enemies[nearestId].passive = true;
      Logger::info("LittleJohn '" + obj.name + "' linked to guardian enemy " +
                   std::to_string(nearestId));
    } else {
      Logger::info("LittleJohn '" + obj.name +
                   "' has no nearby enemy to use as guardian");
    }
  }
}
