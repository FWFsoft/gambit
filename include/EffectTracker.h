#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Effect.h"
#include "EffectInstance.h"
#include "EventBus.h"

// Client-side effect tracker for visual display
class EffectTracker {
 public:
  EffectTracker();

  // Get active effects for an entity
  const std::vector<EffectInstance>& getEffects(uint32_t entityId,
                                                bool isEnemy) const;

  // Check if entity has any effects
  bool hasEffects(uint32_t entityId, bool isEnemy) const;

 private:
  void onNetworkPacketReceived(const NetworkPacketReceivedEvent& e);

  std::unordered_map<uint32_t, std::vector<EffectInstance>> playerEffects;
  std::unordered_map<uint32_t, std::vector<EffectInstance>> enemyEffects;

  static const std::vector<EffectInstance> emptyEffects;
};
