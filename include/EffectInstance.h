#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Effect.h"

// Runtime instance of an active effect on an entity
struct EffectInstance {
  EffectType type;
  uint8_t stacks;           // Current stack count (1 if not stackable)
  float remainingDuration;  // Milliseconds remaining
  uint32_t sourceId;        // Who applied this effect (0 = environment)

  // For Wound/Mend: track ticking state
  float lastTickTime;

  EffectInstance()
      : type(EffectType::Slow),
        stacks(1),
        remainingDuration(0.0f),
        sourceId(0),
        lastTickTime(0.0f) {}

  EffectInstance(EffectType t, uint8_t s, float dur, uint32_t src)
      : type(t),
        stacks(s),
        remainingDuration(dur),
        sourceId(src),
        lastTickTime(0.0f) {}

  bool isExpired() const { return remainingDuration <= 0.0f; }

  // Check if this is a consume-on-use effect (Expose/Guard)
  bool isConsumeOnUse() const {
    return type == EffectType::Expose || type == EffectType::Guard;
  }
};

// Container for all active effects on a single entity
struct ActiveEffects {
  std::vector<EffectInstance> effects;

  // Check if entity has this effect active
  bool hasEffect(EffectType type) const {
    for (const auto& effect : effects) {
      if (effect.type == type) {
        return true;
      }
    }
    return false;
  }

  // Find effect by type (mutable)
  EffectInstance* findEffect(EffectType type) {
    for (auto& effect : effects) {
      if (effect.type == type) {
        return &effect;
      }
    }
    return nullptr;
  }

  // Find effect by type (const)
  const EffectInstance* findEffect(EffectType type) const {
    for (const auto& effect : effects) {
      if (effect.type == type) {
        return &effect;
      }
    }
    return nullptr;
  }

  // Get total stacks of an effect
  uint8_t getStacks(EffectType type) const {
    const EffectInstance* effect = findEffect(type);
    return effect ? effect->stacks : 0;
  }

  // Remove effect by type
  void removeEffect(EffectType type) {
    effects.erase(std::remove_if(effects.begin(), effects.end(),
                                 [type](const EffectInstance& e) {
                                   return e.type == type;
                                 }),
                  effects.end());
  }

  // Remove all buffs
  void removeAllBuffs() {
    effects.erase(std::remove_if(effects.begin(), effects.end(),
                                 [](const EffectInstance& e) {
                                   return EffectRegistry::getCategory(e.type) ==
                                          EffectCategory::Buff;
                                 }),
                  effects.end());
  }

  // Remove all debuffs
  void removeAllDebuffs() {
    effects.erase(std::remove_if(effects.begin(), effects.end(),
                                 [](const EffectInstance& e) {
                                   return EffectRegistry::getCategory(e.type) ==
                                          EffectCategory::Debuff;
                                 }),
                  effects.end());
  }
};
