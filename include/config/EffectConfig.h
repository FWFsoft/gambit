#pragma once

namespace Config {
namespace Effects {

// Movement effects
constexpr float SLOW_DURATION = 3000.0f;  // 3 seconds
constexpr float SLOW_INTENSITY = 0.20f;   // 20% speed reduction per stack
constexpr uint8_t SLOW_MAX_STACKS = 5;

constexpr float HASTE_DURATION = 3000.0f;  // 3 seconds
constexpr float HASTE_INTENSITY = 0.20f;   // 20% speed increase per stack
constexpr uint8_t HASTE_MAX_STACKS = 5;

// Damage modifiers
constexpr float WEAKENED_DURATION = 4000.0f;  // 4 seconds
constexpr float WEAKENED_INTENSITY = 0.15f;   // 15% damage reduction per stack
constexpr uint8_t WEAKENED_MAX_STACKS = 5;

constexpr float EMPOWERED_DURATION = 4000.0f;  // 4 seconds
constexpr float EMPOWERED_INTENSITY = 0.15f;   // 15% damage increase per stack
constexpr uint8_t EMPOWERED_MAX_STACKS = 5;

constexpr float VULNERABLE_DURATION = 5000.0f;  // 5 seconds
constexpr float VULNERABLE_INTENSITY =
    0.10f;  // 10% more damage taken per stack
constexpr uint8_t VULNERABLE_MAX_STACKS = 5;

constexpr float FORTIFIED_DURATION = 5000.0f;  // 5 seconds
constexpr float FORTIFIED_INTENSITY = 0.10f;  // 10% less damage taken per stack
constexpr uint8_t FORTIFIED_MAX_STACKS = 5;

// Damage over time
constexpr float WOUND_DURATION = 6000.0f;  // 6 seconds
constexpr float WOUND_INTENSITY = 5.0f;    // 5 damage per tick per stack
constexpr uint8_t WOUND_MAX_STACKS = 10;

constexpr float MEND_DURATION = 6000.0f;  // 6 seconds
constexpr float MEND_INTENSITY = 5.0f;    // 5 healing per tick per stack
constexpr uint8_t MEND_MAX_STACKS = 10;

// Critical strike (placeholder - not fully implemented)
constexpr float DULLED_DURATION = 4000.0f;  // 4 seconds
constexpr float DULLED_INTENSITY = 0.0f;    // TODO: crit system
constexpr uint8_t DULLED_MAX_STACKS = 1;

constexpr float SHARPENED_DURATION = 4000.0f;  // 4 seconds
constexpr float SHARPENED_INTENSITY = 0.0f;    // TODO: crit system
constexpr uint8_t SHARPENED_MAX_STACKS = 1;

// Meta effects
constexpr float CURSED_DURATION = 8000.0f;  // 8 seconds (extends)
constexpr float CURSED_INTENSITY = 0.0f;    // No intensity (blocks buffs)
constexpr uint8_t CURSED_MAX_STACKS = 1;

constexpr float BLESSED_DURATION = 8000.0f;  // 8 seconds (extends)
constexpr float BLESSED_INTENSITY = 0.0f;    // No intensity (blocks debuffs)
constexpr uint8_t BLESSED_MAX_STACKS = 1;

constexpr float DOOMED_DURATION = 10000.0f;  // 10 seconds (overrides)
constexpr float DOOMED_INTENSITY = 0.0f;     // No intensity (prevents cleanse)
constexpr uint8_t DOOMED_MAX_STACKS = 1;

constexpr float ANCHORED_DURATION = 10000.0f;  // 10 seconds (overrides)
constexpr float ANCHORED_INTENSITY = 0.0f;     // No intensity (prevents purge)
constexpr uint8_t ANCHORED_MAX_STACKS = 1;

// Targeting/visibility
constexpr float MARKED_DURATION = 5000.0f;  // 5 seconds (extends)
constexpr float MARKED_INTENSITY = 0.0f;    // No intensity (enemy targeting)
constexpr uint8_t MARKED_MAX_STACKS = 1;

constexpr float STEALTH_DURATION = 5000.0f;  // 5 seconds (extends)
constexpr float STEALTH_INTENSITY = 0.0f;    // No intensity (invisibility)
constexpr uint8_t STEALTH_MAX_STACKS = 1;

// Consume-on-damage
constexpr float EXPOSE_DURATION = 0.0f;    // Consumed on use (no duration)
constexpr float EXPOSE_INTENSITY = 0.15f;  // 15% damage increase per stack
constexpr uint8_t EXPOSE_MAX_STACKS = 5;

constexpr float GUARD_DURATION = 0.0f;    // Consumed on use (no duration)
constexpr float GUARD_INTENSITY = 0.10f;  // 10% damage reduction per stack
constexpr uint8_t GUARD_MAX_STACKS = 5;

// Control effects
constexpr float STUNNED_DURATION = 2000.0f;  // 2 seconds (overrides)
constexpr float STUNNED_INTENSITY = 0.0f;    // No intensity (can't move or act)
constexpr uint8_t STUNNED_MAX_STACKS = 1;

constexpr float BERSERK_DURATION = 6000.0f;  // 6 seconds (overrides)
constexpr float BERSERK_INTENSITY = 0.0f;    // No intensity (immune to stun)
constexpr uint8_t BERSERK_MAX_STACKS = 1;

constexpr float SNARED_DURATION = 3000.0f;  // 3 seconds (extends)
constexpr float SNARED_INTENSITY = 0.0f;    // No intensity (can't move)
constexpr uint8_t SNARED_MAX_STACKS = 1;

constexpr float UNBOUNDED_DURATION = 4000.0f;  // 4 seconds (extends)
constexpr float UNBOUNDED_INTENSITY =
    0.0f;  // No intensity (immune to slow/snare)
constexpr uint8_t UNBOUNDED_MAX_STACKS = 1;

constexpr float CONFUSED_DURATION = 4000.0f;  // 4 seconds (extends)
constexpr float CONFUSED_INTENSITY = 0.0f;    // No intensity (attacks allies)
constexpr uint8_t CONFUSED_MAX_STACKS = 1;

constexpr float FOCUSED_DURATION = 6000.0f;  // 6 seconds (extends)
constexpr float FOCUSED_INTENSITY = 0.0f;  // No intensity (immune to confused)
constexpr uint8_t FOCUSED_MAX_STACKS = 1;

constexpr float SILENCED_DURATION = 3000.0f;  // 3 seconds (extends)
constexpr float SILENCED_INTENSITY =
    0.0f;  // No intensity (can't use abilities)
constexpr uint8_t SILENCED_MAX_STACKS = 1;

constexpr float INSPIRED_DURATION = 6000.0f;  // 6 seconds (extends)
constexpr float INSPIRED_INTENSITY = 0.0f;  // No intensity (immune to silenced)
constexpr uint8_t INSPIRED_MAX_STACKS = 1;

constexpr float GRAPPLED_DURATION = 2000.0f;  // 2 seconds (extends)
constexpr float GRAPPLED_INTENSITY = 0.0f;    // No intensity (movement linked)
constexpr uint8_t GRAPPLED_MAX_STACKS = 1;

constexpr float FREED_DURATION = 4000.0f;  // 4 seconds (extends)
constexpr float FREED_INTENSITY =
    0.0f;  // No intensity (immune to slow/grapple)
constexpr uint8_t FREED_MAX_STACKS = 1;

// Special
constexpr float RESONANCE_DURATION = 0.0f;   // Consumed on use (no duration)
constexpr float RESONANCE_INTENSITY = 0.0f;  // Character-specific behavior
constexpr uint8_t RESONANCE_MAX_STACKS = 10;

}  // namespace Effects
}  // namespace Config
