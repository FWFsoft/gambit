#include "RemotePlayerInterpolation.h"
#include "Logger.h"

RemotePlayerInterpolation::RemotePlayerInterpolation(uint32_t localPlayerId)
    : localPlayerId(localPlayerId) {

    // Subscribe to network packet events
    EventBus::instance().subscribe<NetworkPacketReceivedEvent>([this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
    });
}

void RemotePlayerInterpolation::onNetworkPacketReceived(const NetworkPacketReceivedEvent& e) {
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
            Player& remotePlayer = remotePlayers[playerState.playerId];
            remotePlayer.id = playerState.playerId;
            remotePlayer.x = playerState.x;
            remotePlayer.y = playerState.y;
            remotePlayer.vx = playerState.vx;
            remotePlayer.vy = playerState.vy;
            remotePlayer.health = playerState.health;
            remotePlayer.r = playerState.r;
            remotePlayer.g = playerState.g;
            remotePlayer.b = playerState.b;
        }
    }
    else if (type == PacketType::PlayerJoined) {
        PlayerJoinedPacket packet = deserializePlayerJoined(e.data, e.size);
        if (packet.playerId != localPlayerId) {
            onPlayerJoined(packet.playerId, packet.r, packet.g, packet.b);
        }
    }
    else if (type == PacketType::PlayerLeft) {
        PlayerLeftPacket packet = deserializePlayerLeft(e.data, e.size);
        if (packet.playerId != localPlayerId) {
            onPlayerLeft(packet.playerId);
        }
    }
}

void RemotePlayerInterpolation::onPlayerJoined(uint32_t playerId, uint8_t r, uint8_t g, uint8_t b) {
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

    remotePlayers[playerId] = player;

    Logger::info("Remote player " + std::to_string(playerId) + " joined (color: " +
                std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + ")");
}

void RemotePlayerInterpolation::onPlayerLeft(uint32_t playerId) {
    remotePlayers.erase(playerId);
    snapshotBuffers.erase(playerId);

    Logger::info("Remote player " + std::to_string(playerId) + " left");
}

bool RemotePlayerInterpolation::getInterpolatedState(uint32_t playerId, float interpolation, Player& outPlayer) const {
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
