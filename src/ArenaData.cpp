#include "ArenaData.h"

#include <algorithm>
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

Genus fieldedGenus(Genus territory) {
  if (territory == Genus::None) return Genus::None;
  return hasThugRoster(territory) ? territory : Genus::Venoxarid;
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
    // The Venoxarid grunt: a hero-sized street thug. Fewer and tougher than
    // the swarmers of the other genera, with a slower, bigger tell.
    {"Venoxarid Grunt", Genus::Venoxarid, EnemyTier::Swarmer, 70, 150, 16, 9,
     48, 450, 1500, 25, 1.0f},
    // Retired with the Venoxarid simplification; kept so indices hold.
    {"Venomire", Genus::Venoxarid, EnemyTier::Caster, 95, 115, 18, 12, 330, 600,
     2300, 30, 1.0f},
    {"Serpentide", Genus::Venoxarid, EnemyTier::Brute, 340, 85, 34, 30, 85, 950,
     2900, 100, 1.0f},

    {"Psymite", Genus::Cerebrian, EnemyTier::Swarmer, 24, 205, 12, 5, 36, 330,
     1250, 10, 1.0f},
    {"Cerebrax", Genus::Cerebrian, EnemyTier::Caster, 85, 120, 18, 13, 360, 650,
     2400, 30, 1.0f},
    {"Synaptogon", Genus::Cerebrian, EnemyTier::Brute, 310, 88, 34, 28, 85, 900,
     3000, 100, 1.0f},

    {"Glacili", Genus::Frigidariant, EnemyTier::Swarmer, 32, 185, 12, 5, 36,
     370, 1350, 10, 1.0f},
    {"Cryothor", Genus::Frigidariant, EnemyTier::Caster, 110, 110, 18, 11, 320,
     650, 2500, 30, 1.0f},
    {"Frosthulm", Genus::Frigidariant, EnemyTier::Brute, 400, 78, 34, 28, 85,
     1000, 3100, 100, 1.0f},

    {"Emberclad", Genus::Magnarok, EnemyTier::Swarmer, 26, 200, 12, 7, 36, 350,
     1300, 10, 1.0f},
    {"Pyroclast", Genus::Magnarok, EnemyTier::Caster, 90, 118, 18, 16, 340, 600,
     2300, 30, 1.0f},
    {"Lavaforge", Genus::Magnarok, EnemyTier::Brute, 330, 85, 34, 38, 85, 950,
     2800, 100, 1.0f},

    {"The Matriarch", Genus::None, EnemyTier::Boss, 4200, 92, 62, 55, 130, 1100,
     2300, 500, 2.0f},
};

const EnemyKindDef& enemyDef(EnemyKind kind) {
  int index = static_cast<int>(kind);
  assert(index >= 0 && index < static_cast<int>(EnemyKind::Count));
  return ENEMY_DEFS[index];
}

EnemyKind enemyKindFor(Genus genus, EnemyTier tier) {
  if (tier == EnemyTier::Boss || genus == Genus::None) return EnemyKind::Boss;
  if (genus == Genus::Venoxarid) return EnemyKind::VenoxaridGrunt;
  int index = (static_cast<int>(genus) - 1) * 3 + static_cast<int>(tier);
  return static_cast<EnemyKind>(index);
}

bool hasThugRoster(Genus genus) { return genus == Genus::Venoxarid; }

const EliteTuning& eliteTuning(EnemyKind kind) {
  // The Venoxarid elite is the enforcer: 1.5x the grunt's size, and the
  // pack's heavy now that the brute is gone.
  static const EliteTuning THUG = {4.0f, 2.0f, 1.5f, 0.85f, 1.3f};
  static const EliteTuning REGULAR = {2.2f, 1.3f, 1.15f, 1.0f, 1.0f};
  return kind == EnemyKind::VenoxaridGrunt ? THUG : REGULAR;
}

bool eliteHasOwnActor(EnemyKind kind) {
  return kind == EnemyKind::VenoxaridGrunt;
}

const char* enemyActorName(EnemyKind kind, bool elite) {
  if (kind == EnemyKind::VenoxaridGrunt) {
    return elite ? "VenoxaridElite" : "VenoxaridGrunt";
  }
  return enemyDef(kind).name;
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
     205.0f,
     "Soulblade",
     "Gravon is the blade; the man is its host, with his own health. When "
     "the host falls the blade lies exposed for 15s (-1s per nearby enemy "
     "death), then raises him. An ally who touches the blade carries it "
     "safely on their back.",
     {
         {"Titan's Slash",
          "A slow, heavy, flat swing through the whole half-circle in front "
          "of him. The first heaves the blade off his back.",
          1000.0f, AbilityMode::Tap, 0.0f},
         {"Ravager's Edge",
          "Hold, rooted: throw the blade over into a wide area ahead; its "
          "weight drags him down after it, and he must heave it out again. "
          "Full charge "
          "grants Sharpened and a guaranteed critical hit.",
          5000.0f, AbilityMode::Charge, 1500.0f},
         {"Fury of the Blade",
          "Gain Empowered and Haste. While either is active, hits reduce "
          "Blade Rush's cooldown.",
          14000.0f, AbilityMode::Tap, 0.0f},
         {"Blade Rush", "Slide forward, then stab the first enemy ahead.",
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
          "Hold: bond with an ally or a rock, healing both of you over the "
          "channel (a rock heals Isaac more). Release to be pulled to it.",
          8000.0f, AbilityMode::Charge, 2500.0f},
         {"The Blessing of Isaac",
          "Bless an ally or yourself. The blessed target is healed whenever "
          "Isaac heals anyone.",
          3000.0f, AbilityMode::Tap, 0.0f},
         {"Flash of Sanctity",
          "Blink to the cursor, healing yourself and allies near the exit.",
          9000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Jeff,
     "Jeff",
     "G3F-0427, the Recycler Bot",
     Archetype::Striker,
     260.0f,
     240.0f,
     "Biomass Reactor",
     "Each stack of Resonance (up to 10) makes G3F 3% faster and its damage "
     "5% higher. Resonance fades 20s after the last harvest.",
     {
         {"Thermal Disassembly",
          "A cutting beam that damages the first enemy in line. Firing "
          "builds heat; at full heat it overheats and must reload. Heat "
          "drains when not firing.",
          180.0f, AbilityMode::Tap, 0.0f, "Thermal Beam"},
         {"Acquire Biomass",
          "Hold, then release: vacuum up the remains of slain enemies around "
          "you, +1 Resonance each. Hold longer to reach farther. Any enemy "
          "killed by anyone leaves remains for 12s.",
          800.0f, AbilityMode::Charge, 1400.0f, "Acquire Biomass"},
         {"Reverse of Installation",
          "Fire a wide, piercing beam of radiation that damages every enemy "
          "in its line. Discharges all Resonance: +16 damage per stack.",
          11000.0f, AbilityMode::Tap, 0.0f, "Reverse Install"},
         {"They See Me Rollin'",
          "Reassemble the legs into tank treads: double base run speed and "
          "+2 Haste. Press again, or use Acquire Biomass, to stand back up on legs.",
          1200.0f, AbilityMode::Tap, 0.0f, "Rollin'"},
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
    {CharacterId::Kade,
     "Kade",
     "the Prism",
     Archetype::Playmaker,
     240.0f,
     255.0f,
     "Prismatic Advantage",
     "Deploys reflective prismatic mirrors that remain active until "
     "destroyed or replaced (maximum 3 active).",
     {
         {"Refraction Wall",
          "Hold: a hologram of the mirror appears at the target; circle the "
          "cursor around it to turn the glass. Release: dash to it and slap "
          "the floor, and the mirror stands up. "
          "Anything that strikes a mirror leaves straight out of its glass. "
          "If another mirror exists, a beam joins them into a barrier of "
          "light: Knockback, and +1 Expose to enemies crossing it. "
          "3 charges, each back 5s after use.",
          350.0f, AbilityMode::Charge, 2500.0f, "Refract Wall"},
         {"Focused Blast",
          "Hold: charge an optic blast from the visor and release it toward "
          "the target. Its reach, width and knockback grow with the charge. "
          "Full charge also Stuns for 1s.",
          6000.0f, AbilityMode::Charge, 1500.0f, "Focused Blast"},
         {"Full Release",
          "Remove your beam limiters for 3s: a huge beam you aim freely, "
          "turned by every mirror it strikes. Everything it strikes is Stunned. "
          "Afterwards you are spent: rooted for a moment.",
          16000.0f, AbilityMode::Tap, 0.0f, "Full Release"},
         {"Mirror Step",
          "Leave a mirror where you stand and teleport through the mirror nearest the target.",
          8000.0f, AbilityMode::Tap, 0.0f, "Mirror Step"},
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
          "the first one. Holding a foe: Bomba - a leaping sit-out powerbomb "
          "that drives them into the ground. Charge for distance and size.",
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
    {CharacterId::Nolan,
     "Nolan",
     "the Prodigy",
     Archetype::Striker,
     250.0f,
     262.0f,
     "No guys, I don't need a passive either",
     "(Okay, I'll take it, that's kind of cool.) Knock-ups and knockbacks "
     "last longer for every stack of Expose on the target.",
     {
         {"Look up! Secret Technique: Lower Limb Devastation!",
          "A sweeping kick, then a hop backwards. Briefly knocks up everything "
          "hit and applies +1 Expose.",
          650.0f, AbilityMode::Tap, 0.0f, "Lower Limb"},
         {"[TWIN DRAGON MODE ACTIVATED] Spinning Lotus Crane Tortoise Sky "
          "Splitter",
          "Hold: kick in a full arc, knocking back everything nearby. +1 "
          "Expose. Knockback distance grows with charge.",
          5000.0f, AbilityMode::Charge, 1000.0f, "Sky Splitter"},
         {"Sun Slaying Black-out Kick of the Demon God",
          "Kick an airborne foe into the ground. Stuns it and every enemy "
          "nearby. If Wade is an ally, he gains Resonance.",
          8000.0f, AbilityMode::Tap, 0.0f, "Black-out Kick"},
         {"Anime Shit",
          "Teleport to where an airborne foe will land. +1 Expose to it.",
          4000.0f, AbilityMode::Tap, 0.0f},
     }},
    {CharacterId::Nyx,
     "Nyx",
     "the Hive Mother",
     Archetype::Playmaker,
     230.0f,
     258.0f,
     "Hive Mother",
     "Nyx directs a nanite swarm. On an enemy: Silenced, and +1 Dulled and "
     "+1 Weakened each second. On an ally: Focused, and +1 Sharpened each "
     "second. Cleansing or purging it, or Nyx straying too far, sends it "
     "home. Her abilities radiate from her and from the swarm.",
     {
         {"Nanite Swarm",
          "Send the swarm to the cursor. It attaches to an enemy or ally "
          "there. Enemies that pass through it gain +1 Slow.",
          450.0f, AbilityMode::Tap, 0.0f, "Swarm"},
         {"Plague Bringer",
          "Hold: a wave of nanites. Enemies gain +2 Weakened, allies +1 "
          "Empowered. Up to 3 debuffs on the swarm's target are contaminated "
          "onto every enemy hit.",
          6000.0f, AbilityMode::Charge, 900.0f},
         {"Nanite Cloud",
          "Everything a swarm rides is affected: enemies are Marked, allies "
          "gain Stealth.",
          9000.0f, AbilityMode::Tap, 0.0f, "Cloud"},
         {"Become the Swarm",
          "Set down the CRT and ride an enemy or ally as a swarm, taking half "
          "the damage dealt to them. Again: stream back to the CRT.",
          8000.0f, AbilityMode::Tap, 0.0f, "Swarm Form"},
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
          "A 1, 2, 3 or 4, to the head or the body, in three-punch "
          "combinations. The third punch grants +1 Haste and resets Roll.",
          380.0f, AbilityMode::Tap, 0.0f},
         {"Uppercuts",
          "Hold: plant your feet and throw alternating 5s and 6s. Faster "
          "with Haste.",
          1500.0f, AbilityMode::Channel, 260.0f},
         {"Slip",
          "Go intangible for 0.5s. If struck, counter with a haymaker "
          "shockwave and refresh Slip.",
          7000.0f, AbilityMode::Tap, 0.0f},
         {"Roll",
          "Cleanse Slow, Snare and Stun, then roll under and toward the "
          "cursor. With Resonance: discharge it to roll much farther.",
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

static const InfusionPartInfo PARTS[PART_COUNT] = {
    {"Talon", "+6% damage", 0.06f, 0.0f, 0.0f, 0.0f},
    {"Hide", "+12 max health", 0.0f, 12.0f, 0.0f, 0.0f},
    {"Eye", "Abilities recharge 6% faster", 0.0f, 0.0f, 0.06f, 0.0f},
    {"Sinew", "+4% move speed", 0.0f, 0.0f, 0.0f, 0.04f},
};

const InfusionPartInfo& partInfo(InfusionPart part) {
  int index = static_cast<int>(part);
  assert(index >= 0 && index < PART_COUNT);
  return PARTS[index];
}

// The suit layout: one entry per node, in Talent order. Tier 1 is the single
// "leg" out of the core into each tree; every later tier is a choice.
// Talent behaviour lives in src/Infusions.cpp.
static const SuitNode SUIT_NODES[] = {
    // ---- Offense -----------------------------------------------------------
    {InfusionTree::Offense, 1, Talent::KillerInstinct, "Killer Instinct",
     "+5% critical strike chance."},
    {InfusionTree::Offense, 2, Talent::OpeningStrike, "Opening Strike",
     "Hits on enemies at full health deal 50% more damage."},
    {InfusionTree::Offense, 2, Talent::Relentless, "Relentless",
     "Each basic attack hit adds 3% damage for 2 s, up to 15%."},
    {InfusionTree::Offense, 3, Talent::Affliction, "Affliction",
     "20% more damage to enemies carrying Poison, Wound, Slow, Snare, Stun "
     "or Confusion."},
    {InfusionTree::Offense, 3, Talent::Executioner, "Executioner",
     "35% more damage to enemies below 30% health."},
    {InfusionTree::Offense, 4, Talent::Bloodlust, "Bloodlust",
     "Kills Empower you for 3 s (stacks up to 3)."},
    {InfusionTree::Offense, 4, Talent::FocusFire, "Focus Fire",
     "Your hits make enemies Vulnerable for 2 s, so the whole team hits "
     "them harder."},
    // ---- Defense -----------------------------------------------------------
    {InfusionTree::Defense, 1, Talent::Plating, "Plating",
     "Take 8% less damage."},
    {InfusionTree::Defense, 2, Talent::SecondWind, "Second Wind",
     "Dropping below 35% health grants a shield of 25% max health. Once "
     "every 30 s."},
    {InfusionTree::Defense, 2, Talent::Regrowth, "Regrowth",
     "After 3 s without taking damage, regenerate 3% max health per second."},
    {InfusionTree::Defense, 3, Talent::Guardian, "Guardian",
     "Allies within 250 take 10% less damage."},
    {InfusionTree::Defense, 3, Talent::Thorns, "Thorns",
     "Melee attackers take 8 damage back."},
    {InfusionTree::Defense, 4, Talent::LastStand, "Last Stand",
     "Once per round, a killing blow leaves you at 1 health and untouchable "
     "for 1.5 s."},
    {InfusionTree::Defense, 4, Talent::ShareTheLoad, "Share the Load",
     "You take a quarter of the damage dealt to allies within 300."},
    // ---- Mobility ----------------------------------------------------------
    {InfusionTree::Mobility, 1, Talent::LightFrame, "Light Frame",
     "+6% move speed."},
    {InfusionTree::Mobility, 2, Talent::QuickRecovery, "Quick Recovery",
     "Your SHIFT ability recharges 25% faster."},
    {InfusionTree::Mobility, 2, Talent::Slipstream, "Slipstream",
     "Using your SHIFT ability Hastes you for 2 s."},
    {InfusionTree::Mobility, 3, Talent::HitAndRun, "Hit and Run",
     "Kills give 20% move speed for 2 s."},
    {InfusionTree::Mobility, 3, Talent::Tailwind, "Tailwind",
     "Using your SHIFT ability Hastes allies within 250 for 2 s."},
    {InfusionTree::Mobility, 4, Talent::Blitz, "Blitz",
     "After your SHIFT ability, your next hit within 2 s deals 60% more "
     "damage and Slows."},
    {InfusionTree::Mobility, 4, Talent::EscapeArtist, "Escape Artist",
     "Dropping below 40% health Hastes you twice and makes you immune to "
     "crowd control for 3 s. Once every 20 s."},
    // ---- Genealogy ---------------------------------------------------------
    {InfusionTree::Genealogy, 1, Talent::Bloodline, "Bloodline",
     "Your core ability recharges 15% faster."},
    {InfusionTree::Genealogy, 2, Talent::Purebred, "Purebred",
     "+3% damage for every slotted infusion of your core's genus."},
    {InfusionTree::Genealogy, 2, Talent::HybridVigor, "Hybrid Vigor",
     "+4% damage and 4% less damage taken for every different genus in your "
     "suit."},
    {InfusionTree::Genealogy, 3, Talent::Overcharge, "Overcharge",
     "Your core ability deals 30% more damage."},
    {InfusionTree::Genealogy, 3, Talent::SharedGift, "Shared Gift",
     "Casting your core ability heals allies within 300 for 8% of their max "
     "health."},
    {InfusionTree::Genealogy, 4, Talent::ApexMutation, "Apex Mutation",
     "Genus effects of your core's genus count one level higher."},
    {InfusionTree::Genealogy, 4, Talent::FeedbackLoop, "Feedback Loop",
     "Kills take 0.5 s off your core ability's cooldown."},
};
static_assert(sizeof(SUIT_NODES) / sizeof(SUIT_NODES[0]) == TALENT_COUNT,
              "one suit node per talent");

int suitNodeCount() { return TALENT_COUNT; }

const SuitNode& suitNode(int index) {
  assert(index >= 0 && index < TALENT_COUNT);
  const SuitNode& node = SUIT_NODES[index];
  assert(static_cast<int>(node.talent) == index);
  return node;
}

int treeTierCount(InfusionTree tree) {
  int tiers = 0;
  for (const SuitNode& node : SUIT_NODES) {
    if (node.tree == tree) tiers = std::max(tiers, static_cast<int>(node.tier));
  }
  return tiers;
}

NodeState nodeState(const Genus* nodeGenus, bool coreFilled, int node,
                    int maxTier) {
  assert(nodeGenus != nullptr);
  assert(maxTier >= 1);
  const SuitNode& self = suitNode(node);
  if (nodeGenus[node] != Genus::None) return NodeState::Filled;
  if (!coreFilled) return NodeState::Locked;
  bool parentFilled = self.tier == 1;
  for (int i = 0; i < TALENT_COUNT; i++) {
    const SuitNode& other = SUIT_NODES[i];
    if (other.tree != self.tree || nodeGenus[i] == Genus::None) continue;
    if (other.tier == self.tier) return NodeState::Closed;
    if (other.tier + 1 == self.tier) parentFilled = true;
  }
  if (!parentFilled) return NodeState::Locked;
  return self.tier > maxTier ? NodeState::Later : NodeState::Open;
}

const AbilityInfo& coreAbility(Genus g) {
  int index = static_cast<int>(g);
  assert(index >= 0 && index <= GENUS_COUNT);
  return CORE_ABILITIES[index];
}

// Refined and Exalted effects (specs/infusion-grades.md, section 3). Raw
// builds something, Refined pays it off in a new way, Exalted changes how
// you play. Behaviour in src/Infusions.cpp.
static const InfusionText GRADED_TEXT[GENUS_COUNT + 1][TREE_COUNT][2] = {
    {{{"", ""}, {"", ""}},
     {{"", ""}, {"", ""}},
     {{"", ""}, {"", ""}},
     {{"", ""}, {"", ""}}},
    // Venoxarid
    {
        {{"Rupture",
          "Hitting an enemy carrying 10 or more poison stacks pops them for "
          "burst damage and spreads half of them to enemies nearby."},
         {"Plague Carrier",
          "A poisoned enemy that dies passes its full poison to the two "
          "nearest enemies. Kills chain through a pack."}},
        {{"Corrosion",
          "Poisoned enemies near you are Weakened: they deal less damage to "
          "everyone."},
         {"Symbiote",
          "A poisoned enemy that dies near you heals you and allies nearby "
          "for 2% of max health."}},
        {{"Toxic Wake",
          "Running past an enemy poisons it. You poison by moving."},
         {"Swarm Rider",
          "With 5 or more poisoned enemies nearby you cannot be slowed or "
          "rooted, and your SHIFT recharges 50% faster."}},
        {{"Toxic Catalyst",
          "Your core ability deals double damage to enemies with 3 or more "
          "poison stacks."},
         {"Miasma Heart",
          "You always exude a small poison cloud. You are the plague."}},
    },
    // Cerebrian
    {
        {{"Dominate", "Every 6th hit on the same enemy Confuses it for 2 s."},
         {"Puppeteer",
          "Enemies you Confused deal double damage to other enemies, and "
          "their kills count as yours for loot."}},
        {{"Backlash",
          "When your barrier breaks, enemies close to you are Stunned for "
          "1 s."},
         {"Psychic Link",
          "When your barrier recharges, allies nearby get a barrier of half "
          "its strength."}},
        {{"Ghost Step", "After a dash, enemies lose track of you for 1.5 s."},
         {"Phantom",
          "You walk through enemies like a ghost, and touching one deals "
          "psychic damage."}},
        {{"Mind Echo",
          "1.5 s after your core ability damages enemies, a psychic echo "
          "hits them again for 40%."},
         {"Overmind",
          "30% of damage dealt to an enemy you Confused is also dealt to "
          "every other enemy you Confused nearby."}},
    },
    // Frigidariant
    {
        {{"Splinter",
          "Hitting a frozen enemy ends the freeze and bursts for part of "
          "its max health to enemies around it."},
         {"Glacier",
          "Stand still for half a second: you take 40% less damage and your "
          "hits freeze at once, until you move."}},
        {{"Permafrost",
          "Enemies close to you are always Slowed, so a melee attacker's "
          "first hit roots it."},
         {"Cryo Sanctuary",
          "Hit below half health, an ice dome forms around you: enemies "
          "entering are rooted, allies inside take 20% less damage. Once "
          "every 25 s."}},
        {{"Ice Skate",
          "On your own ice you cannot be slowed or rooted and move 20% "
          "faster."},
         {"Avalanche",
          "Moving for 2 s builds momentum: your next hit freezes the target "
          "and hits every enemy in a cone."}},
        {{"Brittle",
          "Enemies you stun or root are Vulnerable while it lasts: the "
          "whole team hits them harder."},
         {"Diamond Heart",
          "Your core ability freezes what it damages, and your shields do "
          "not expire while a frozen enemy is near."}},
    },
    // Magnarok
    {
        {{"Flashpoint",
          "A burn that reaches 5 stacks detonates: the rest of it is dealt "
          "at once to enemies around the target."},
         {"Inferno Heart",
          "Your hits leave molten ground under the target, and you are "
          "Empowered while you stand on molten ground."}},
        {{"Cauterizing Wounds",
          "Killing a burning enemy clears 30% of the damage Slow Burn is "
          "holding back."},
         {"Heat Sink",
          "Damage you hold back builds Heat; at 100 you release a fire nova "
          "and take less damage for 3 s."}},
        {{"Eruption Dash",
          "Your dash or blink ends in a molten burst where you land."},
         {"Firewalker",
          "While 3 or more burning enemies are near, you are always in "
          "Burning Rush."}},
        {{"Fuel", "Your core ability ignites everything it damages."},
         {"Supernova",
          "Every third core cast also erupts around you and leaves a "
          "molten field."}},
    },
};

const InfusionText& infusionText(Genus g, InfusionTree tree, int grade) {
  int gi = static_cast<int>(g);
  int ti = static_cast<int>(tree);
  assert(gi >= 0 && gi <= GENUS_COUNT && ti >= 0 && ti < TREE_COUNT);
  assert(validGrade(grade));
  if (grade == GRADE_RAW) return INFUSION_TEXT[gi][ti];
  return GRADED_TEXT[gi][ti][grade - 2];
}

const char* gradeName(int grade) {
  switch (grade) {
    case GRADE_RAW:
      return "Raw";
    case GRADE_REFINED:
      return "Refined";
    case GRADE_EXALTED:
      return "Exalted";
  }
  assert(false && "bad grade");
  return "?";
}

Genus treeAttunement(const SuitSlots& suit, InfusionTree tree) {
  assert(suit.nodeGenus != nullptr && suit.nodeGrade != nullptr);
  for (int i = 0; i < TALENT_COUNT; i++) {
    if (SUIT_NODES[i].tree != tree || suit.nodeGenus[i] == Genus::None) {
      continue;
    }
    if (suit.nodeGrade[i] >= GRADE_REFINED) return suit.nodeGenus[i];
  }
  return Genus::None;
}

Genus ascendedGenus(const SuitSlots& suit) {
  assert(suit.nodeGenus != nullptr && suit.nodeGrade != nullptr);
  if (suit.core != Genus::None && suit.coreGrade >= GRADE_EXALTED) {
    return suit.core;
  }
  for (int i = 0; i < TALENT_COUNT; i++) {
    if (suit.nodeGenus[i] != Genus::None &&
        suit.nodeGrade[i] >= GRADE_EXALTED) {
      return suit.nodeGenus[i];
    }
  }
  return Genus::None;
}

SlotBlock slotBlock(const SuitSlots& suit, int node, Genus genus, int grade,
                    int maxTier) {
  assert(genus != Genus::None);
  assert(validGrade(grade));
  Genus ascended = ascendedGenus(suit);
  if (grade >= GRADE_EXALTED && ascended != Genus::None && ascended != genus) {
    return SlotBlock::Ascended;
  }
  if (node == CORE_NODE) {
    return suit.core == Genus::None ? SlotBlock::None : SlotBlock::NodeNotOpen;
  }
  assert(node >= 0 && node < TALENT_COUNT);
  if (nodeState(suit.nodeGenus, suit.core != Genus::None, node, maxTier) !=
      NodeState::Open) {
    return SlotBlock::NodeNotOpen;
  }
  const SuitNode& self = suitNode(node);
  if (grade < tierMinGrade(self.tier)) return SlotBlock::GradeTooLow;
  Genus attuned = treeAttunement(suit, self.tree);
  if (attuned != Genus::None && attuned != genus) return SlotBlock::Attuned;
  return SlotBlock::None;
}

bool canCombine(Genus ascended, Genus genus, int grade) {
  if (genus == Genus::None || grade < GRADE_RAW || grade >= GRADE_EXALTED) {
    return false;
  }
  // One capstone genus per match: no Exalted of another genus once
  // Ascendant.
  if (grade + 1 == GRADE_EXALTED && ascended != Genus::None &&
      ascended != genus) {
    return false;
  }
  return true;
}

}  // namespace Arena
