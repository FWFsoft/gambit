#include "ServerGameState.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "CollisionSystem.h"
#include "Logger.h"
#include "NetworkServer.h"

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

  // Broadcast join
  PlayerJoinedPacket packet;
  packet.playerId = playerId;
  packet.r = player.r;
  packet.g = player.g;
  packet.b = player.b;
  server->broadcastPacket(serialize(packet));
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
                              input.moveDown, 16.67f, worldWidth, worldHeight,
                              collisionSystem);
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
  player.health = 100.0f;
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
      !collisionSystem->isPositionValid(x, y, PLAYER_RADIUS)) {
    Logger::info("Default spawn invalid, searching...");

    for (float radius = 50.0f; radius < 500.0f; radius += 50.0f) {
      for (float angle = 0; angle < 360.0f; angle += 45.0f) {
        float testX =
            worldWidth / 2.0f + radius * std::cos(angle * 3.14159f / 180.0f);
        float testY =
            worldHeight / 2.0f + radius * std::sin(angle * 3.14159f / 180.0f);
        if (collisionSystem->isPositionValid(testX, testY, PLAYER_RADIUS)) {
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
  const uint8_t colors[4][3] = {
      {255, 0, 0},   // Red
      {0, 255, 0},   // Green
      {0, 0, 255},   // Blue
      {255, 255, 0}  // Yellow
  };
  int colorIndex = playerCount % 4;
  player.r = colors[colorIndex][0];
  player.g = colors[colorIndex][1];
  player.b = colors[colorIndex][2];
}
