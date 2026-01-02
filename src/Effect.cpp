#include "Effect.h"

#include "config/EffectConfig.h"

// Helper macro for defining effect definitions
#define DEFINE_EFFECT(TYPE, NAME, CATEGORY, STACK_BEHAVIOR, DURATION, \
                      INTENSITY, MAX_STACKS, OPPOSITE, HAS_OPPOSITE)  \
  {TYPE,                                                              \
   NAME,                                                              \
   CATEGORY,                                                          \
   STACK_BEHAVIOR,                                                    \
   DURATION,                                                          \
   INTENSITY,                                                         \
   MAX_STACKS,                                                        \
   OPPOSITE,                                                          \
   HAS_OPPOSITE,                                                      \
   false,                                                             \
   false,                                                             \
   false,                                                             \
   false,                                                             \
   false,                                                             \
   false,                                                             \
   false,                                                             \
   {SecondaryEffect(), SecondaryEffect()},                            \
   0}

// EffectRegistry implementation
const EffectDefinition EffectRegistry::definitions[EFFECT_TYPE_COUNT] = {
    // Movement effects (0-1)
    DEFINE_EFFECT(EffectType::Slow, "Slow", EffectCategory::Debuff,
                  StackBehavior::Stacks, Config::Effects::SLOW_DURATION,
                  Config::Effects::SLOW_INTENSITY,
                  Config::Effects::SLOW_MAX_STACKS, EffectType::Haste, true),

    DEFINE_EFFECT(EffectType::Haste, "Haste", EffectCategory::Buff,
                  StackBehavior::Stacks, Config::Effects::HASTE_DURATION,
                  Config::Effects::HASTE_INTENSITY,
                  Config::Effects::HASTE_MAX_STACKS, EffectType::Slow, true),

    // Damage modifiers (2-5)
    DEFINE_EFFECT(EffectType::Weakened, "Weakened", EffectCategory::Debuff,
                  StackBehavior::Stacks, Config::Effects::WEAKENED_DURATION,
                  Config::Effects::WEAKENED_INTENSITY,
                  Config::Effects::WEAKENED_MAX_STACKS, EffectType::Empowered,
                  true),

    DEFINE_EFFECT(EffectType::Empowered, "Empowered", EffectCategory::Buff,
                  StackBehavior::Stacks, Config::Effects::EMPOWERED_DURATION,
                  Config::Effects::EMPOWERED_INTENSITY,
                  Config::Effects::EMPOWERED_MAX_STACKS, EffectType::Weakened,
                  true),

    DEFINE_EFFECT(EffectType::Vulnerable, "Vulnerable", EffectCategory::Debuff,
                  StackBehavior::Stacks, Config::Effects::VULNERABLE_DURATION,
                  Config::Effects::VULNERABLE_INTENSITY,
                  Config::Effects::VULNERABLE_MAX_STACKS, EffectType::Fortified,
                  true),

    DEFINE_EFFECT(EffectType::Fortified, "Fortified", EffectCategory::Buff,
                  StackBehavior::Stacks, Config::Effects::FORTIFIED_DURATION,
                  Config::Effects::FORTIFIED_INTENSITY,
                  Config::Effects::FORTIFIED_MAX_STACKS, EffectType::Vulnerable,
                  true),

    // Damage over time (6-7)
    DEFINE_EFFECT(EffectType::Wound, "Wound", EffectCategory::Debuff,
                  StackBehavior::Stacks, Config::Effects::WOUND_DURATION,
                  Config::Effects::WOUND_INTENSITY,
                  Config::Effects::WOUND_MAX_STACKS, EffectType::Mend, true),

    DEFINE_EFFECT(EffectType::Mend, "Mend", EffectCategory::Buff,
                  StackBehavior::Stacks, Config::Effects::MEND_DURATION,
                  Config::Effects::MEND_INTENSITY,
                  Config::Effects::MEND_MAX_STACKS, EffectType::Wound, true),

    // Critical strike (8-9) - placeholder
    DEFINE_EFFECT(EffectType::Dulled, "Dulled", EffectCategory::Debuff,
                  StackBehavior::Stacks, Config::Effects::DULLED_DURATION,
                  Config::Effects::DULLED_INTENSITY,
                  Config::Effects::DULLED_MAX_STACKS, EffectType::Sharpened,
                  true),

    DEFINE_EFFECT(EffectType::Sharpened, "Sharpened", EffectCategory::Buff,
                  StackBehavior::Stacks, Config::Effects::SHARPENED_DURATION,
                  Config::Effects::SHARPENED_INTENSITY,
                  Config::Effects::SHARPENED_MAX_STACKS, EffectType::Dulled,
                  true),

    // Meta effects (10-13)
    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Cursed, "Cursed", EffectCategory::Debuff,
          StackBehavior::Extends, Config::Effects::CURSED_DURATION,
          Config::Effects::CURSED_INTENSITY, Config::Effects::CURSED_MAX_STACKS,
          EffectType::Blessed, true);
      def.blocksBuffs = true;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Blessed, "Blessed", EffectCategory::Buff,
          StackBehavior::Extends, Config::Effects::BLESSED_DURATION,
          Config::Effects::BLESSED_INTENSITY,
          Config::Effects::BLESSED_MAX_STACKS, EffectType::Cursed, true);
      def.blocksDebuffs = true;
      return def;
    }(),

    DEFINE_EFFECT(EffectType::Doomed, "Doomed", EffectCategory::Debuff,
                  StackBehavior::Overrides, Config::Effects::DOOMED_DURATION,
                  Config::Effects::DOOMED_INTENSITY,
                  Config::Effects::DOOMED_MAX_STACKS, EffectType::Anchored,
                  true),

    DEFINE_EFFECT(EffectType::Anchored, "Anchored", EffectCategory::Buff,
                  StackBehavior::Overrides, Config::Effects::ANCHORED_DURATION,
                  Config::Effects::ANCHORED_INTENSITY,
                  Config::Effects::ANCHORED_MAX_STACKS, EffectType::Doomed,
                  true),

    // Targeting/visibility (14-15)
    DEFINE_EFFECT(EffectType::Marked, "Marked", EffectCategory::Debuff,
                  StackBehavior::Extends, Config::Effects::MARKED_DURATION,
                  Config::Effects::MARKED_INTENSITY,
                  Config::Effects::MARKED_MAX_STACKS, EffectType::Stealth,
                  true),

    DEFINE_EFFECT(EffectType::Stealth, "Stealth", EffectCategory::Buff,
                  StackBehavior::Extends, Config::Effects::STEALTH_DURATION,
                  Config::Effects::STEALTH_INTENSITY,
                  Config::Effects::STEALTH_MAX_STACKS, EffectType::Marked,
                  true),

    // Consume-on-damage (16-17)
    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Expose, "Expose", EffectCategory::Debuff,
          StackBehavior::Stacks, Config::Effects::EXPOSE_DURATION,
          Config::Effects::EXPOSE_INTENSITY, Config::Effects::EXPOSE_MAX_STACKS,
          EffectType::Guard, true);
      def.consumeOnDamage = true;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Guard, "Guard", EffectCategory::Buff,
          StackBehavior::Stacks, Config::Effects::GUARD_DURATION,
          Config::Effects::GUARD_INTENSITY, Config::Effects::GUARD_MAX_STACKS,
          EffectType::Expose, true);
      def.consumeOnDamage = true;
      return def;
    }(),

    // Control effects (18-27)
    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Stunned, "Stunned", EffectCategory::Debuff,
          StackBehavior::Overrides, Config::Effects::STUNNED_DURATION,
          Config::Effects::STUNNED_INTENSITY,
          Config::Effects::STUNNED_MAX_STACKS, EffectType::Berserk, true);
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Weakened, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Berserk, "Berserk", EffectCategory::Buff,
          StackBehavior::Overrides, Config::Effects::BERSERK_DURATION,
          Config::Effects::BERSERK_INTENSITY,
          Config::Effects::BERSERK_MAX_STACKS, EffectType::Stunned, true);
      def.immuneToStun = true;
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Empowered, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Snared, "Snared", EffectCategory::Debuff,
          StackBehavior::Extends, Config::Effects::SNARED_DURATION,
          Config::Effects::SNARED_INTENSITY, Config::Effects::SNARED_MAX_STACKS,
          EffectType::Unbounded, true);
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Slow, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Unbounded, "Unbounded", EffectCategory::Buff,
          StackBehavior::Extends, Config::Effects::UNBOUNDED_DURATION,
          Config::Effects::UNBOUNDED_INTENSITY,
          Config::Effects::UNBOUNDED_MAX_STACKS, EffectType::Snared, true);
      def.immuneToMovementImpair = true;  // Immune to Slow/Snare
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Haste, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Confused, "Confused", EffectCategory::Debuff,
          StackBehavior::Extends, Config::Effects::CONFUSED_DURATION,
          Config::Effects::CONFUSED_INTENSITY,
          Config::Effects::CONFUSED_MAX_STACKS, EffectType::Focused, true);
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Dulled, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Focused, "Focused", EffectCategory::Buff,
          StackBehavior::Extends, Config::Effects::FOCUSED_DURATION,
          Config::Effects::FOCUSED_INTENSITY,
          Config::Effects::FOCUSED_MAX_STACKS, EffectType::Confused, true);
      def.immuneToConfused = true;
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Sharpened, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Silenced, "Silenced", EffectCategory::Debuff,
          StackBehavior::Extends, Config::Effects::SILENCED_DURATION,
          Config::Effects::SILENCED_INTENSITY,
          Config::Effects::SILENCED_MAX_STACKS, EffectType::Inspired, true);
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Dulled, 1);
      def.secondaryEffects[1] = SecondaryEffect(EffectType::Weakened, 1);
      def.secondaryEffectCount = 2;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Inspired, "Inspired", EffectCategory::Buff,
          StackBehavior::Extends, Config::Effects::INSPIRED_DURATION,
          Config::Effects::INSPIRED_INTENSITY,
          Config::Effects::INSPIRED_MAX_STACKS, EffectType::Silenced, true);
      def.immuneToSilenced = true;
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Sharpened, 1);
      def.secondaryEffects[1] = SecondaryEffect(EffectType::Empowered, 1);
      def.secondaryEffectCount = 2;
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Grappled, "Grappled", EffectCategory::Debuff,
          StackBehavior::Extends, Config::Effects::GRAPPLED_DURATION,
          Config::Effects::GRAPPLED_INTENSITY,
          Config::Effects::GRAPPLED_MAX_STACKS, EffectType::Freed, true);
      return def;
    }(),

    []() {
      EffectDefinition def = DEFINE_EFFECT(
          EffectType::Freed, "Freed", EffectCategory::Buff,
          StackBehavior::Extends, Config::Effects::FREED_DURATION,
          Config::Effects::FREED_INTENSITY, Config::Effects::FREED_MAX_STACKS,
          EffectType::Grappled, true);
      def.immuneToMovementImpair = true;  // Immune to Slow/Grapple
      def.secondaryEffects[0] = SecondaryEffect(EffectType::Empowered, 1);
      def.secondaryEffectCount = 1;
      return def;
    }(),

    // Special (28)
    DEFINE_EFFECT(EffectType::Resonance, "Resonance", EffectCategory::Neutral,
                  StackBehavior::Stacks, Config::Effects::RESONANCE_DURATION,
                  Config::Effects::RESONANCE_INTENSITY,
                  Config::Effects::RESONANCE_MAX_STACKS, EffectType::Resonance,
                  false),
};

#undef DEFINE_EFFECT

const EffectDefinition& EffectRegistry::get(EffectType type) {
  return definitions[static_cast<uint8_t>(type)];
}

EffectType EffectRegistry::getOpposite(EffectType type) {
  const EffectDefinition& def = get(type);
  return def.hasOpposite ? def.oppositeEffect : type;
}

bool EffectRegistry::areOpposites(EffectType a, EffectType b) {
  return getOpposite(a) == b && getOpposite(b) == a;
}

const char* EffectRegistry::getName(EffectType type) { return get(type).name; }

EffectCategory EffectRegistry::getCategory(EffectType type) {
  return get(type).category;
}
