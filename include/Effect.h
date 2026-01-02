#pragma once

#include <cstdint>

// Effect categories
enum class EffectCategory : uint8_t { Buff = 0, Debuff = 1, Neutral = 2 };

// Stack behavior determines how multiple applications combine
enum class StackBehavior : uint8_t {
  Stacks = 0,    // Multiple applications add stacks, refresh all durations
  Extends = 1,   // Multiple applications extend the existing duration
  Overrides = 2  // Multiple applications reset duration (only one active)
};

// All 29 effect types
enum class EffectType : uint8_t {
  // Movement effects (0-1)
  Slow = 0,
  Haste = 1,

  // Damage modifiers (2-5)
  Weakened = 2,
  Empowered = 3,
  Vulnerable = 4,
  Fortified = 5,

  // Damage over time (6-7)
  Wound = 6,
  Mend = 7,

  // Critical strike (8-9)
  Dulled = 8,
  Sharpened = 9,

  // Meta effects (10-13)
  Cursed = 10,
  Blessed = 11,
  Doomed = 12,
  Anchored = 13,

  // Targeting/visibility (14-15)
  Marked = 14,
  Stealth = 15,

  // Consume-on-damage (16-17)
  Expose = 16,
  Guard = 17,

  // Control effects (18-27)
  Stunned = 18,
  Berserk = 19,
  Snared = 20,
  Unbounded = 21,
  Confused = 22,
  Focused = 23,
  Silenced = 24,
  Inspired = 25,
  Grappled = 26,
  Freed = 27,

  // Special (28)
  Resonance = 28
};

constexpr uint8_t EFFECT_TYPE_COUNT = 29;

// Secondary effect definition (effects that are auto-applied)
struct SecondaryEffect {
  EffectType type;
  uint8_t stacks;

  SecondaryEffect() : type(EffectType::Slow), stacks(0) {}
  SecondaryEffect(EffectType t, uint8_t s) : type(t), stacks(s) {}
};

// Effect definition (immutable base properties)
struct EffectDefinition {
  EffectType type;
  const char* name;
  EffectCategory category;
  StackBehavior stackBehavior;

  // Base values (fixed for now, no per-character variation)
  float baseDuration;   // Base duration in milliseconds
  float baseIntensity;  // Base intensity (meaning varies per effect)
  uint8_t maxStacks;    // Max stacks (0 = no limit, 1 = no stacking)

  // Relationships
  EffectType oppositeEffect;  // Effect that cancels this one
  bool hasOpposite;

  // Special flags
  bool consumeOnDamage;         // Consumed when damage taken (Expose/Guard)
  bool blocksBuffs;             // Blocks all buffs (Cursed)
  bool blocksDebuffs;           // Blocks all debuffs (Blessed)
  bool immuneToStun;            // Immune to Stunned (Berserk)
  bool immuneToMovementImpair;  // Immune to movement impairing effects
  bool immuneToConfused;        // Immune to Confused (Focused)
  bool immuneToSilenced;        // Immune to Silenced (Inspired)

  // Secondary effects applied when this effect is applied (up to 2)
  SecondaryEffect secondaryEffects[2];
  uint8_t secondaryEffectCount;
};

// Global effect definitions registry
class EffectRegistry {
 public:
  // Get effect definition by type
  static const EffectDefinition& get(EffectType type);

  // Get opposite effect (returns same type if no opposite exists)
  static EffectType getOpposite(EffectType type);

  // Check if two effects are opposites
  static bool areOpposites(EffectType a, EffectType b);

  // Get effect name
  static const char* getName(EffectType type);

  // Get effect category
  static EffectCategory getCategory(EffectType type);

 private:
  // Array of all 29 effect definitions
  static const EffectDefinition definitions[EFFECT_TYPE_COUNT];
};
