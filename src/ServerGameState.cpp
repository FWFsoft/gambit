#include "ServerGameState.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "CollisionSystem.h"
#include "Logger.h"
#include "NetworkServer.h"
#include "config/GameplayConfig.h"
#include "config/PlayerConfig.h"
#include "config/TimingConfig.h"

ServerGameState::ServerGameState(NetworkServer* server,
                                 const WorldConfig& world)
    : server(server),
      worldWidth(world.width),
      worldHeight(world.height),
      collisionSystem(world.collisionSystem),
      serverTick(0) {
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

  if (type == PacketType::ClientInput) {
    processClientInput(e.peer, e.data, e.size);
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
}

Player ServerGameState::createPlayer(uint32_t playerId) {
  Player player;
  player.id = playerId;
  player.vx = 0;
  player.vy = 0;
  player.health = Config::Player::MAX_HEALTH;
  player.lastInputSequence = 0;

  // Find spawn position
  player.x = worldWidth / 2.0f;
  player.y = worldHeight / 2.0f;
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
