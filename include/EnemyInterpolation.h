#pragma once

#include <deque>
#include <unordered_map>
#include <vector>

#include "Enemy.h"
#include "EventBus.h"
#include "NetworkProtocol.h"

class AnimationSystem;

struct EnemySnapshot {
  float x, y;
  float vx, vy;
  float health;
  uint8_t state;
  uint64_t timestamp;  // Milliseconds since epoch
};

// Client-side enemy state interpolation
// Mirrors RemotePlayerInterpolation pattern
class EnemyInterpolation {
 public:
  explicit EnemyInterpolation(AnimationSystem* animSystem);

  // Update enemy state from network packet
  void updateEnemyState(const NetworkEnemyState& state);

  // Remove enemy (when died or despawned)
  void removeEnemy(uint32_t enemyId);

  // Get interpolated enemy state for rendering
  bool getInterpolatedState(uint32_t enemyId, float interpolation,
                            Enemy& outEnemy) const;

  // Get all enemy IDs
  std::vector<uint32_t> getEnemyIds() const;

 private:
  AnimationSystem* animationSystem;
  std::unordered_map<uint32_t, Enemy> enemies;
  std::unordered_map<uint32_t, std::deque<EnemySnapshot>> snapshots;

  static constexpr size_t MAX_SNAPSHOTS = 3;  // Same as RemotePlayerInterpolation

  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);
};
