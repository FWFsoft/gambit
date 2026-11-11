#include "ArenaData.h"

#include <cassert>
#include <cmath>

namespace Arena {

const char* genusName(Genus g) {
  switch (g) {
    case Genus::Venoxarid:
      return "Venoxarid";
    case Genus::Cerebrian:
      return "Cerebrian";
    case Genus::Frigidariant:
      return "Frigidariant";
    case Genus::Magnarok:
      return "Magnarok";
    default:
      return "None";
  }
}

Genus genusForDirection(float dx, float dy) {
  // Screen space, y grows downward. Quadrants are split along the diagonals
  // so each genus owns one compass direction from the ship.
  if (std::abs(dx) >= std::abs(dy)) {
    return dx >= 0.0f ? Genus::Magnarok : Genus::Cerebrian;  // east / west
  }
  return dy >= 0.0f ? Genus::Venoxarid : Genus::Frigidariant;  // south / north
}

// Stats follow the alien-arena .yml files (25 / 100 / 300 health, 1.0 / 0.5 /
// 0.35 relative speed) with numbers tuned for this prototype.
static const EnemyKindDef ENEMY_DEFS[static_cast<int>(EnemyKind::Count)] = {
    // name, genus, tier, hp, speed, radius, dmg, range, windup, cooldown,
    // score, scale
    {"Toxlith", Genus::Venoxarid, EnemyTier::Swarmer, 28, 195, 12, 5, 36, 350,
     1300, 10, 1.0f},
    {"Venomire", Genus::Venoxarid, EnemyTier::Caster, 95, 115, 18, 12, 330,
     600, 2300, 30, 1.0f},
    {"Serpentide", Genus::Venoxarid, EnemyTier::Brute, 340, 85, 34, 30, 85,
     950, 2900, 100, 1.0f},

    {"Psymite", Genus::Cerebrian, EnemyTier::Swarmer, 24, 205, 12, 5, 36, 330,
     1250, 10, 1.0f},
    {"Cerebrax", Genus::Cerebrian, EnemyTier::Caster, 85, 120, 18, 13, 360,
     650, 2400, 30, 1.0f},
    {"Synaptogon", Genus::Cerebrian, EnemyTier::Brute, 310, 88, 34, 28, 85,
     900, 3000, 100, 1.0f},

    {"Glacili", Genus::Frigidariant, EnemyTier::Swarmer, 32, 185, 12, 5, 36,
     370, 1350, 10, 1.0f},
    {"Cryothor", Genus::Frigidariant, EnemyTier::Caster, 110, 110, 18, 11, 320,
     650, 2500, 30, 1.0f},
    {"Frosthulm", Genus::Frigidariant, EnemyTier::Brute, 400, 78, 34, 28, 85,
     1000, 3100, 100, 1.0f},

    {"Emberclad", Genus::Magnarok, EnemyTier::Swarmer, 26, 200, 12, 7, 36, 350,
     1300, 10, 1.0f},
    {"Pyroclast", Genus::Magnarok, EnemyTier::Caster, 90, 118, 18, 16, 340,
     600, 2300, 30, 1.0f},
    {"Lavaforge", Genus::Magnarok, EnemyTier::Brute, 330, 85, 34, 38, 85, 950,
     2800, 100, 1.0f},

    {"The Matriarch", Genus::None, EnemyTier::Boss, 4200, 92, 62, 55, 130,
     1100, 2300, 500, 2.0f},
};

const EnemyKindDef& enemyDef(EnemyKind kind) {
  int index = static_cast<int>(kind);
  assert(index >= 0 && index < static_cast<int>(EnemyKind::Count));
  return ENEMY_DEFS[index];
}

EnemyKind enemyKindFor(Genus genus, EnemyTier tier) {
  if (tier == EnemyTier::Boss || genus == Genus::None) return EnemyKind::Boss;
  int index = (static_cast<int>(genus) - 1) * 3 + static_cast<int>(tier);
  return static_cast<EnemyKind>(index);
}

const char* archetypeName(Archetype a) {
  switch (a) {
    case Archetype::Defender:
      return "Defender";
    case Archetype::Striker:
      return "Striker";
    case Archetype::Winger:
      return "Winger";
    case Archetype::Playmaker:
      return "Playmaker";
  }
  return "";
}

// Kit text mirrors the "Character Concepts" design doc. Numbers are prototype
// tuning; the behaviour is implemented in src/CharacterKits.cpp.
static const KitInfo KITS[] = {
    {CharacterId::Suds,
     "Suds",
     "the Brew Brawler",
     Archetype::Defender,
     320.0f,
     240.0f,
     "Frothy",
     "When a nearby creature loses Guard it gains Wound for the damage the "
     "Guard prevented.",
     {
         {"Brew Toss",
          "Lob a brew. Small splash damage; everything hit gains +3 Guard.",
          550.0f, AbilityMode::Tap, 0.0f},
         {"Frothy Cleanse",
          "Hold: belch in a cone. Cleanses Wound and purges Guard. Allies go "
          "Berserk, enemies are Stunned.",
          6000.0f, AbilityMode::Charge, 900.0f},
         {"Round's On Me!", "Toss six brews in quick succession.", 10000.0f,
          AbilityMode::Tap, 0.0f},
         {"What's In This Stuff?",
          "Double Guard and Wound stacks nearby. Allies gain Fortified, "
          "enemies gain Weakened.",
          12000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Gravon,
     "Gravon",
     "the Undying Blade",
     Archetype::Striker,
     240.0f,
     255.0f,
     "Soulblade",
     "Once per round a killing blow leaves only the blade. Survive 5s "
     "(-1s per nearby enemy death) and the host rises again.",
     {
         {"Titan's Slash", "Swing in a wide arc for high damage.", 600.0f,
          AbilityMode::Tap, 0.0f},
         {"Ravager's Edge",
          "Hold: one devastating strike. Full charge grants Sharpened and a "
          "guaranteed critical hit.",
          4000.0f, AbilityMode::Charge, 1400.0f},
         {"Fury of the Blade",
          "Gain Empowered and Haste. While either is active, hits reduce "
          "Blade Rush's cooldown.",
          14000.0f, AbilityMode::Tap, 0.0f},
         {"Blade Rush", "Lunge forward, stabbing everything in the way.",
          7000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Isaac,
     "Isaac",
     "the Beacon",
     Archetype::Winger,
     220.0f,
     260.0f,
     "Radiance",
     "Isaac's healing scales with how many enemies his light touches.",
     {
         {"Ray of Benevolence",
          "Cone of light. Slows enemies and heals nearby allies for each "
          "enemy hit.",
          520.0f, AbilityMode::Tap, 0.0f},
         {"Lifeline",
          "Hold: bond with an ally or the ground, healing over the channel. "
          "Release to be pulled to the target.",
          8000.0f, AbilityMode::Charge, 2500.0f},
         {"The Blessing of Isaac",
          "Bless an ally or yourself. The blessed target is healed whenever "
          "Isaac heals anyone.",
          3000.0f, AbilityMode::Tap, 0.0f},
         {"Flash of Sanctity",
          "Blink to the cursor, healing yourself and allies near the exit.",
          9000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Zephyr,
     "Zephyr",
     "the Galewalker",
     Archetype::Playmaker,
     240.0f,
     265.0f,
     "Wind's Favor",
     "Whenever an ability hits an enemy, gain +1 Haste.",
     {
         {"Wind Slash", "A piercing gust that pushes enemies away.", 500.0f,
          AbilityMode::Tap, 0.0f},
         {"Vortex",
          "Hold: pull enemies toward a point, clustering and Stunning them.",
          9000.0f, AbilityMode::Charge, 1200.0f},
         {"Gale's Embrace",
          "Shield yourself or an ally in air. When it ends, enemies around "
          "the target are flung away.",
          12000.0f, AbilityMode::Tap, 0.0f},
         {"Tempest Dash", "Dash a long distance through enemies.", 6000.0f,
          AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Mina,
     "Mina",
     "the Professional",
     Archetype::Striker,
     230.0f,
     265.0f,
     "Not the Goalie",
     "Explosions going off near Mina grant her +1 Fortified.",
     {
         {"Pass the Ball",
          "Kick a bomb ball in a line. Explodes on the first enemy hit.",
          650.0f, AbilityMode::Tap, 0.0f},
         {"The Long Shot",
          "Hold: lob an oversized bomb. Distance, radius and damage grow "
          "with charge.",
          5000.0f, AbilityMode::Charge, 1300.0f},
         {"GOAL!!!!",
          "Four waves of bomb balls rain on the target area. +1 Vulnerable "
          "per wave.",
          14000.0f, AbilityMode::Tap, 0.0f},
         {"Handball!", "Pick up the ball and run. Gain +2 Haste.", 10000.0f,
          AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Presidente,
     "El Presidente",
     "the Presidente",
     Archetype::Playmaker,
     330.0f,
     238.0f,
     "The Presidente",
     "Grappling a foe grants Resonance. With Resonance every move becomes "
     "its alternate form. Resonance ends when the foe is thrown, slammed or "
     "slips free.",
     {
         {"Lucha Libre",
          "Grab the foe in front of you and lift it overhead. Holding a foe: "
          "Latigo - whirl, knocking enemies back, then throw your foe at the "
          "cursor.",
          600.0f, AbilityMode::Tap, 0.0f},
         {"Pesado",
          "Hold: charge forward shoulder-first, scattering enemies and grabbing "
          "the first one. Holding a foe: Bomba - leap and slam them into the "
          "ground. Charge for distance and size.",
          6500.0f, AbilityMode::Charge, 1000.0f},
         {"Grande Cajones",
          "Flex. Enemies in front lose their buffs and become Vulnerable; you "
          "gain Fortified. Holding a foe: Abrazo de Oso - crushing bear hug "
          "that Stuns (separate cooldown).",
          9000.0f, AbilityMode::Tap, 0.0f},
         {"El Splash de Rana",
          "Leap to the cursor and body-splash, knocking up foes where you "
          "land. Holding a foe: Grande Rana Bomba - land on them for a much "
          "bigger slam.",
          9000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Volgore,
     "Volgore",
     "the Berserker",
     Archetype::Defender,
     340.0f,
     240.0f,
     "Berserker",
     "While Berserk: +5 Fortified, but every hit taken applies +1 Wound.",
     {
         {"Hamstring",
          "Swing low in a small arc. Minor damage and +1 Weakened.", 520.0f,
          AbilityMode::Tap, 0.0f},
         {"Hooked", "Hold: drag every enemy in a cone toward you.", 8000.0f,
          AbilityMode::Charge, 1100.0f},
         {"Ferocious Bellow",
          "Snare enemies in a wide area. Enemies that were already Snared "
          "are Doomed.",
          12000.0f, AbilityMode::Tap, 0.0f},
         {"Rage Unending",
          "Go Berserk. Tap again to end it early and turn your Wounds into "
          "Mend.",
          9000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Wade,
     "Wade",
     "the Old Head",
     Archetype::Striker,
     250.0f,
     262.0f,
     "Stuck in His Ways",
     "Wade told the designers he doesn't need a passive. His punches are "
     "good enough.",
     {
         {"Simple Punches",
          "A 1, 2, 3 or 4. Every third punch grants +1 Haste and resets "
          "Roll.",
          380.0f, AbilityMode::Tap, 0.0f},
         {"Uppercuts",
          "Hold: plant your feet and throw alternating 5s and 6s. Faster "
          "with Haste.",
          1500.0f, AbilityMode::Channel, 260.0f},
         {"Slip",
          "Go intangible for 0.5s. If struck, counter with a haymaker "
          "shockwave and refresh Slip.",
          7000.0f, AbilityMode::Tap, 0.0f},
         {"Roll", "Cleanse Slow, Snare and Stun, then roll toward the cursor.",
          5000.0f, AbilityMode::Tap, 0.0f},
     }},
};

const KitInfo* kitInfo(uint32_t characterId) {
  for (const KitInfo& kit : KITS) {
    if (kit.characterId == characterId) return &kit;
  }
  return nullptr;
}

bool isPlayable(uint32_t characterId) { return kitInfo(characterId) != nullptr; }

const char* treeName(InfusionTree t) {
  switch (t) {
    case InfusionTree::Offense:
      return "Offense";
    case InfusionTree::Defense:
      return "Defense";
    case InfusionTree::Mobility:
      return "Mobility";
    case InfusionTree::Genealogy:
      return "Genealogy";
  }
  return "";
}

// Text follows specs/genera-and-infusion-system.md.
static const AbilityInfo CORE_ABILITIES[GENUS_COUNT + 1] = {
    {"No Core", "Slot your first infusion to gain a core ability.", 0.0f,
     AbilityMode::Tap, 0.0f},
    {"Toxic Miasma",
     "Exude a lingering cloud of poison. Enemies inside are afflicted with "
     "Wound.",
     12000.0f, AbilityMode::Tap, 0.0f},
    {"Mind Flay",
     "A wave of psychic energy. Damages enemies and Confuses them into "
     "attacking each other.",
     11000.0f, AbilityMode::Tap, 0.0f},
    {"Diamond Fortress",
     "Crystalline armor massively reduces damage. Press again to shatter it "
     "for huge area damage.",
     16000.0f, AbilityMode::Tap, 0.0f},
    {"Cauterize",
     "Explode every over-time effect on nearby enemies and yourself. More "
     "stacks, bigger blast.",
     10000.0f, AbilityMode::Tap, 0.0f},
};

static const InfusionText INFUSION_TEXT[GENUS_COUNT + 1][TREE_COUNT] = {
    {{"", ""}, {"", ""}, {"", ""}, {"", ""}},
    {
        {"Toxic Blade",
         "Attacks splash venom: the target and enemies around it gain Wound."},
        {"Acidic Shell",
         "Melee attackers are poisoned. Poisoned enemies deal less damage to "
         "you."},
        {"Speed Siphon", "Move faster for every poisoned enemy nearby."},
        {"Venom Infusion",
         "Your core ability poisons everything it damages."},
    },
    {
        {"Psychic Shockwave",
         "Attacks emit a shockwave that damages enemies around the target."},
        {"Mind Barrier",
         "A recharging psychic shield. While it holds you cannot be "
         "crowd-controlled."},
        {"Phase Shift",
         "Dashes and blinks leave you ghosted: enemies you pass through take "
         "psychic damage."},
        {"Mass Confusion",
         "Core ability damage briefly Confuses the enemies it hits."},
    },
    {
        {"Icy Strikes",
         "Attacks Slow. Hitting Slowed enemies Snares them; hitting Snared "
         "enemies Stuns them."},
        {"Arctic Aura",
         "Melee attackers are Slowed; Slowed attackers are Snared; Snared "
         "attackers are Stunned."},
        {"Glacial Path", "Leave a trail of ice that Slows enemies."},
        {"Unbreakable", "Stunning or Snaring an enemy grants you a shield."},
    },
    {
        {"Eruptive Strikes",
         "Attacks ignite the target, burning it for damage over time."},
        {"Slow Burn",
         "Part of every hit you take is dealt over time instead of at once."},
        {"Burning Rush",
         "Every 12 kills: a burst of speed and a trail of molten ground."},
        {"Cataclysm",
         "Core ability damage leaves molten ground that burns enemies."},
    },
};

const AbilityInfo& coreAbility(Genus g) {
  int index = static_cast<int>(g);
  assert(index >= 0 && index <= GENUS_COUNT);
  return CORE_ABILITIES[index];
}

const InfusionText& infusionText(Genus g, InfusionTree tree) {
  int gi = static_cast<int>(g);
  int ti = static_cast<int>(tree);
  assert(gi >= 0 && gi <= GENUS_COUNT && ti >= 0 && ti < TREE_COUNT);
  return INFUSION_TEXT[gi][ti];
}

}  // namespace Arena
