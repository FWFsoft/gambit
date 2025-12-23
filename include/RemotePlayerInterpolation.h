#pragma once

#include <chrono>
#include <deque>
#include <unordered_map>

#include "EventBus.h"
#include "NetworkProtocol.h"
#include "Player.h"

struct PlayerSnapshot {
  float x, y;
  float vx, vy;
  float health;
  uint32_t serverTick;
  std::chrono::steady_clock::time_point receivedTime;
};

class RemotePlayerInterpolation {
 public:
  RemotePlayerInterpolation(uint32_t localPlayerId);

  // Get interpolated state for a remote player
  bool getInterpolatedState(uint32_t playerId, float interpolation,
                            Player& outPlayer) const;

  // Get all remote player IDs
  std::vector<uint32_t> getRemotePlayerIds() const;

 private:
  uint32_t localPlayerId;
  bool localPlayerIdConfirmed;  // Track if we've received our real ID from
                                // server
  std::unordered_map<uint32_t, std::deque<PlayerSnapshot>> snapshotBuffers;
  std::unordered_map<uint32_t, Player>
      remotePlayers;  // Store latest known state

  static constexpr size_t MAX_SNAPSHOTS = 3;  // ~50ms buffer at 60 FPS

  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);
  void onPlayerJoined(uint32_t playerId, uint8_t r, uint8_t g, uint8_t b);
  void onPlayerLeft(uint32_t playerId);
};
