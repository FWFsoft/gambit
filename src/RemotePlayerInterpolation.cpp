#include "RemotePlayerInterpolation.h"

#include "AnimationAssetLoader.h"
#include "AnimationSystem.h"
#include "Logger.h"

RemotePlayerInterpolation::RemotePlayerInterpolation(
    uint32_t localPlayerId, AnimationSystem* animationSystem)
    : localPlayerId(localPlayerId),
      localPlayerIdConfirmed(false),
      animationSystem(animationSystem) {
  // Subscribe to network packet events
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
      });
}

void RemotePlayerInterpolation::onNetworkPacketReceived(
    const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);

  if (type == PacketType::StateUpdate) {
    StateUpdatePacket stateUpdate = deserializeStateUpdate(e.data, e.size);
    auto now = std::chrono::steady_clock::now();

    // Store snapshots for all remote players (not local player)
    for (const auto& playerState : stateUpdate.players) {
      if (playerState.playerId == localPlayerId) {
        continue;  // Skip local player (handled by ClientPrediction)
      }

      // Skip if we haven't received PlayerJoined for this player yet
      auto playerIt = remotePlayers.find(playerState.playerId);
      if (playerIt == remotePlayers.end()) {
        static std::unordered_map<uint32_t, int> skippedCounts;
        if (skippedCounts[playerState.playerId]++ < 3) {
          Logger::debug("Skipping StateUpdate for unknown player " +
                        std::to_string(playerState.playerId) +
                        " (waiting for PlayerJoined)");
        }
        continue;  // Will be initialized when PlayerJoined arrives
      }

      // Create snapshot
      PlayerSnapshot snapshot;
      snapshot.x = playerState.x;
      snapshot.y = playerState.y;
      snapshot.vx = playerState.vx;
      snapshot.vy = playerState.vy;
      snapshot.health = playerState.health;
      snapshot.serverTick = stateUpdate.serverTick;
      snapshot.receivedTime = now;

      // Add to buffer
      auto& buffer = snapshotBuffers[playerState.playerId];
      buffer.push_back(snapshot);

      // Keep only last MAX_SNAPSHOTS
      if (buffer.size() > MAX_SNAPSHOTS) {
        buffer.pop_front();
      }

      // Update latest known state
      Player& remotePlayer = playerIt->second;

      remotePlayer.id = playerState.playerId;
      remotePlayer.x = playerState.x;
      remotePlayer.y = playerState.y;
      remotePlayer.vx = playerState.vx;
      remotePlayer.vy = playerState.vy;
      remotePlayer.health = playerState.health;
      remotePlayer.r = playerState.r;
      remotePlayer.g = playerState.g;
      remotePlayer.b = playerState.b;

      // Update animation state based on velocity
      if (remotePlayer.animationController) {
        std::string prevAnim =
            remotePlayer.animationController->getCurrentAnimationName();
        remotePlayer.animationController->updateAnimationState(remotePlayer.vx,
                                                               remotePlayer.vy);
        std::string newAnim =
            remotePlayer.animationController->getCurrentAnimationName();

        // Log when animation changes (throttled)
        static std::unordered_map<uint32_t, std::string> lastLoggedAnim;
        if (lastLoggedAnim[playerState.playerId] != newAnim) {
          Logger::debug("Remote player " +
                        std::to_string(playerState.playerId) +
                        " anim: " + prevAnim + " -> " + newAnim +
                        " (vx=" + std::to_string(remotePlayer.vx) +
                        ", vy=" + std::to_string(remotePlayer.vy) + ")");
          lastLoggedAnim[playerState.playerId] = newAnim;
        }
      }
    }
  } else if (type == PacketType::PlayerJoined) {
    PlayerJoinedPacket packet = deserializePlayerJoined(e.data, e.size);

    // The first PlayerJoined packet we receive is for our local player
    // Update our local player ID to match what the server assigned
    if (!localPlayerIdConfirmed) {
      localPlayerId = packet.playerId;
      localPlayerIdConfirmed = true;
      Logger::debug("Confirmed local player ID: " +
                    std::to_string(localPlayerId));
    } else if (packet.playerId != localPlayerId) {
      // This is a different player (remote player)
      onPlayerJoined(packet.playerId, packet.r, packet.g, packet.b);
    } else {
      Logger::debug("Ignoring duplicate PlayerJoined for self");
    }
  } else if (type == PacketType::PlayerLeft) {
    PlayerLeftPacket packet = deserializePlayerLeft(e.data, e.size);
    if (packet.playerId != localPlayerId) {
      onPlayerLeft(packet.playerId);
    }
  } else if (type == PacketType::PlayerDied) {
    PlayerDiedPacket packet = deserializePlayerDied(e.data, e.size);
    if (packet.playerId != localPlayerId) {
      Logger::info("Remote player " + std::to_string(packet.playerId) +
                   " died");
      // Death state will be reconciled via StateUpdate packets
    }
  } else if (type == PacketType::PlayerRespawned) {
    PlayerRespawnedPacket packet = deserializePlayerRespawned(e.data, e.size);
    if (packet.playerId != localPlayerId) {
      Logger::info("Remote player " + std::to_string(packet.playerId) +
                   " respawned");
      // Position and health will be reconciled via StateUpdate packets
    }
  }
}

void RemotePlayerInterpolation::onPlayerJoined(uint32_t playerId, uint8_t r,
                                               uint8_t g, uint8_t b) {
  // Initialize remote player
  Player player;
  player.id = playerId;
  player.x = 400.0f;  // Will be updated by first StateUpdate
  player.y = 300.0f;
  player.vx = 0.0f;
  player.vy = 0.0f;
  player.health = 100.0f;
  player.r = r;
  player.g = g;
  player.b = b;

  // Initialize animations
  AnimationAssetLoader::loadPlayerAnimations(*player.getAnimationController(),
                                             "assets/player_animated.png");

  remotePlayers[playerId] = player;

  // Register with animation system if available
  if (animationSystem) {
    animationSystem->registerEntity(&remotePlayers[playerId]);
  }

  Logger::info("Remote player " + std::to_string(playerId) +
               " joined (color: " + std::to_string(r) + "," +
               std::to_string(g) + "," + std::to_string(b) + ")");
}

void RemotePlayerInterpolation::onPlayerLeft(uint32_t playerId) {
  // Unregister from animation system before erasing
  if (animationSystem) {
    auto it = remotePlayers.find(playerId);
    if (it != remotePlayers.end()) {
      animationSystem->unregisterEntity(&it->second);
    }
  }

  remotePlayers.erase(playerId);
  snapshotBuffers.erase(playerId);

  Logger::info("Remote player " + std::to_string(playerId) + " left");
}

bool RemotePlayerInterpolation::getInterpolatedState(uint32_t playerId,
                                                     float interpolation,
                                                     Player& outPlayer) const {
  // Check if we know about this player
  auto playerIt = remotePlayers.find(playerId);
  if (playerIt == remotePlayers.end()) {
    return false;  // Unknown player
  }

  // Get snapshot buffer
  auto bufferIt = snapshotBuffers.find(playerId);
  if (bufferIt == snapshotBuffers.end() || bufferIt->second.empty()) {
    // No snapshots yet, use latest known state
    outPlayer = playerIt->second;
    return true;
  }

  const auto& buffer = bufferIt->second;

  // If we only have one snapshot, use it directly
  if (buffer.size() == 1) {
    const PlayerSnapshot& snapshot = buffer.front();
    outPlayer = playerIt->second;
    outPlayer.x = snapshot.x;
    outPlayer.y = snapshot.y;
    outPlayer.vx = snapshot.vx;
    outPlayer.vy = snapshot.vy;
    outPlayer.health = snapshot.health;
    return true;
  }

  // Interpolate between last two snapshots
  const PlayerSnapshot& from = buffer[buffer.size() - 2];
  const PlayerSnapshot& to = buffer[buffer.size() - 1];

  // Linear interpolation (lerp)
  // Note: interpolation comes from RenderEvent (0.0-1.0 sub-frame position)
  float t = interpolation;
  t = std::clamp(t, 0.0f, 1.0f);

  outPlayer = playerIt->second;
  outPlayer.x = from.x + (to.x - from.x) * t;
  outPlayer.y = from.y + (to.y - from.y) * t;
  outPlayer.vx = from.vx + (to.vx - from.vx) * t;
  outPlayer.vy = from.vy + (to.vy - from.vy) * t;
  outPlayer.health = from.health + (to.health - from.health) * t;

  return true;
}

std::vector<uint32_t> RemotePlayerInterpolation::getRemotePlayerIds() const {
  std::vector<uint32_t> ids;
  ids.reserve(remotePlayers.size());

  for (const auto& [id, player] : remotePlayers) {
    ids.push_back(id);
  }

  return ids;
}
