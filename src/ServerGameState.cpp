#include "ServerGameState.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "CollisionSystem.h"
#include "EnemySystem.h"
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
      serverTick(0) {
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
  peerToPlayerId[e.peer] = playerId;

  Logger::info("Player " + std::to_string(playerId) + " joined");

  // Send new player's own PlayerJoined packet to them FIRST
  // (so they recognize it as their local player before receiving existing
  // players)
  PlayerJoinedPacket newPlayerPacket;
  newPlayerPacket.playerId = playerId;
  newPlayerPacket.r = player.r;
  newPlayerPacket.g = player.g;
  newPlayerPacket.b = player.b;
  server->send(e.peer, serialize(newPlayerPacket));

  // Then send existing players to new client
  for (const auto& [existingId, existingPlayer] : players) {
    if (existingId != playerId) {
      PlayerJoinedPacket existingPacket;
      existingPacket.playerId = existingId;
      existingPacket.r = existingPlayer.r;
      existingPacket.g = existingPlayer.g;
      existingPacket.b = existingPlayer.b;
      server->send(e.peer, serialize(existingPacket));
      Logger::info("Sent existing player " + std::to_string(existingId) +
                   " to new player " + std::to_string(playerId));
    }
  }

  // Broadcast new player join to all clients (new client will ignore duplicate)
  server->broadcastPacket(serialize(newPlayerPacket));
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

  PacketType type = static_cast<PacketType>(e.data[0]);

  switch (type) {
    case PacketType::ClientInput:
      processClientInput(e.peer, e.data, e.size);
      break;

    case PacketType::AttackEnemy: {
      if (e.size < 9) {
        Logger::info("Invalid AttackEnemy packet size");
        break;
      }

      AttackEnemyPacket attackPacket = deserializeAttackEnemy(e.data, e.size);

      // Get player ID from peer
      auto it = peerToPlayerId.find(e.peer);
      if (it == peerToPlayerId.end()) {
        Logger::info("Received AttackEnemy from unknown peer");
        break;
      }
      uint32_t playerId = it->second;

      // Apply damage (server-authoritative)
      if (enemySystem) {
        enemySystem->damageEnemy(attackPacket.enemyId, attackPacket.damage,
                                 playerId);
      }

      break;
    }

    default:
      Logger::info("Unknown packet type: " +
                   std::to_string(static_cast<int>(type)));
      break;
  }
}

void ServerGameState::processClientInput(ENetPeer* peer, const uint8_t* data,
                                         size_t size) {
  ClientInputPacket input = deserializeClientInput(data, size);

  auto it = peerToPlayerId.find(peer);
  assert(it != peerToPlayerId.end());

  uint32_t playerId = it->second;
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
  applyInput(player, movementInput);
}

void ServerGameState::onUpdate(const UpdateEvent& e) {
  serverTick++;

  // Update enemy AI
  if (enemySystem) {
    enemySystem->update(e.deltaTime, players);
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
    // Fallback: use world center
    player.x = worldWidth / 2.0f;
    player.y = worldHeight / 2.0f;
    Logger::info("Player " + std::to_string(playerId) +
                 " spawned at world center (no spawn points)");
  }

  if (!findValidSpawnPosition(player.x, player.y)) {
    Logger::error("Failed to find valid spawn position");
  }

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
        float testX =
            worldWidth / 2.0f + radius * std::cos(angle * 3.14159f / 180.0f);
        float testY =
            worldHeight / 2.0f + radius * std::sin(angle * 3.14159f / 180.0f);
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
    // Fallback: world center
    player.x = worldWidth / 2.0f;
    player.y = worldHeight / 2.0f;
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
