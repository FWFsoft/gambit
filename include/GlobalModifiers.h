#pragma once

#include "Effect.h"

// Placeholder for future objectives system integration
// Objectives will be able to modify effect durations and intensities globally
class GlobalModifiers {
 public:
  static GlobalModifiers& instance() {
    static GlobalModifiers inst;
    return inst;
  }

  // Future: modify effect durations based on objectives
  // e.g., "All debuffs last 20% longer"
  float getDurationMultiplier(EffectType type) const {
    (void)type;  // Unused for now
    return 1.0f;
  }

  // Future: modify effect intensities based on objectives
  // e.g., "All buffs are 10% stronger"
  float getIntensityMultiplier(EffectType type) const {
    (void)type;  // Unused for now
    return 1.0f;
  }

 private:
  GlobalModifiers() = default;
  GlobalModifiers(const GlobalModifiers&) = delete;
  GlobalModifiers& operator=(const GlobalModifiers&) = delete;
};
