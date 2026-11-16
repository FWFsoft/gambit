#pragma once

#include <cstdint>

// Shared (client + server) game data for the playable prototype: genera,
// enemy kinds, character kits and infusion descriptions. Pure data, no
// behaviour. Behaviour lives in AbilitySystem / CharacterKits / EnemySystem /
// MatchDirector on the server; the client only reads this for HUD and
// rendering.

namespace Arena {

// ---------------------------------------------------------------------------
// Genera
// ---------------------------------------------------------------------------

enum class Genus : uint8_t {
  None = 0,
  Venoxarid = 1,     // poison
  Cerebrian = 2,     // psychic
  Frigidariant = 3,  // ice
  Magnarok = 4       // fire
};
constexpr int GENUS_COUNT = 4;

const char* genusName(Genus g);

// The four genera each own a quadrant of the map around the ship.
// Angle is atan2(dy, dx) from the ship in screen space (y down).
Genus genusForDirection(float dx, float dy);

// The genus whose aliens actually spawn in a genus's territory. Only genera
// with real models are fielded; until the other three get theirs, every
// territory is held by Venoxarid thugs. The territory still decides which
// infusion drops there (docs/design/DECISIONS.md, 2026-11-15).
Genus fieldedGenus(Genus territory);

// ---------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------

enum class EnemyTier : uint8_t { Swarmer = 0, Caster = 1, Brute = 2, Boss = 3 };

// Order matters: (genus - 1) * 3 + tier for the twelve regular kinds.
//
// The roster is being simplified one genus at a time, to one street thug
// per genus plus its elite (docs/design/DECISIONS.md, 2026-11-15).
// Venoxarid is done: every Venoxarid spawn is a VenoxaridGrunt, and
// Venomire and Serpentide are retired. Their slots stay so the numbering
// (and the network value of every other kind) does not move.
enum class EnemyKind : uint8_t {
  VenoxaridGrunt = 0,
  Venomire = 1,    // retired: never spawned
  Serpentide = 2,  // retired: never spawned
  Psymite = 3,
  Cerebrax = 4,
  Synaptogon = 5,
  Glacili = 6,
  Cryothor = 7,
  Frosthulm = 8,
  Emberclad = 9,
  Pyroclast = 10,
  Lavaforge = 11,
  Boss = 12,
  Count = 13
};

struct EnemyKindDef {
  const char* name;
  Genus genus;
  EnemyTier tier;
  float maxHealth;
  float speed;        // pixels / second
  float radius;       // collision + hit radius
  float damage;       // per hit
  float attackRange;  // distance at which it starts an attack
  float windupMs;     // telegraph time before the hit lands
  float cooldownMs;   // time between attacks
  int score;          // points for the kill
  float drawScale;    // sprite scale on the client
};

const EnemyKindDef& enemyDef(EnemyKind kind);
// The kind a genus fields for a tier. A simplified genus fields its grunt
// for every tier.
EnemyKind enemyKindFor(Genus genus, EnemyTier tier);
// True for a genus on the simplified roster (a grunt and its elite).
bool hasThugRoster(Genus genus);

// What being elite does to an enemy. A thug's elite is its own, bigger
// character with its own model; any other elite is the same body, tougher.
struct EliteTuning {
  float health;       // times the kind's health
  float damage;       // times the kind's damage
  float size;         // times the kind's radius (and, for thugs, height)
  float speed;        // times the kind's speed
  float attackRange;  // times the kind's attack range
};
const EliteTuning& eliteTuning(EnemyKind kind);

// The baked sprite-forge actor (assets/sprites/actors/) that draws this
// enemy. An elite thug has its own sheet, already drawn at elite size.
const char* enemyActorName(EnemyKind kind, bool elite);
bool eliteHasOwnActor(EnemyKind kind);

// ---------------------------------------------------------------------------
// Abilities / kits
// ---------------------------------------------------------------------------

enum class AbilitySlot : uint8_t {
  Primary = 0,    // LMB
  Secondary = 1,  // RMB (hold)
  Special = 2,    // Q
  Mobility = 3,   // LSHIFT
  Core = 4        // E (granted by the first infusion)
};
constexpr int SLOT_COUNT = 5;
constexpr int KIT_SLOT_COUNT = 4;

enum class AbilityMode : uint8_t {
  Tap = 0,     // fires on press; primary repeats while held
  Charge = 1,  // hold to charge, fires on release
  Channel = 2  // fires repeatedly while held
};

struct AbilityInfo {
  const char* name;
  const char* description;
  float cooldownMs;
  AbilityMode mode;
  float maxChargeMs;  // Charge: time to full charge. Channel: repeat interval.
  // Shown on the HUD button when the name is too long for it (two lines of
  // about nine characters). nullptr: the name is short enough.
  const char* shortName = nullptr;
};

enum class Archetype : uint8_t { Defender, Striker, Winger, Playmaker };
const char* archetypeName(Archetype a);

struct KitInfo {
  uint32_t characterId;  // matches CharacterRegistry ids
  const char* name;
  const char* title;
  Archetype archetype;
  float maxHealth;
  float moveSpeed;
  const char* passiveName;
  const char* passiveDescription;
  AbilityInfo abilities[KIT_SLOT_COUNT];
};

// nullptr when the character has no playable kit yet.
const KitInfo* kitInfo(uint32_t characterId);
bool isPlayable(uint32_t characterId);

// Character ids used by code (CharacterRegistry is the source of truth).
namespace CharacterId {
constexpr uint32_t Gravon = 3;
constexpr uint32_t Isaac = 4;
constexpr uint32_t Mina = 9;
constexpr uint32_t Nolan = 12;
constexpr uint32_t Presidente = 14;
constexpr uint32_t Suds = 16;
constexpr uint32_t Volgore = 18;
constexpr uint32_t Wade = 19;
constexpr uint32_t Zephyr = 20;
}  // namespace CharacterId

// Wade's ability forms. The server picks one per cast (Player::castVariant)
// and sends it with the AbilityCast event; the client maps it to a clip.
// Boxing numbers: 1 jab, 2 cross, 3 lead hook, 4 rear hook, 5 lead
// uppercut, 6 rear uppercut. "Body" is the same punch thrown downstairs.
namespace WadeForm {
// Simple Punches (slot 0)
constexpr uint8_t Jab = 0;        // 1
constexpr uint8_t Cross = 1;      // 2
constexpr uint8_t LeadHook = 2;   // 3
constexpr uint8_t RearHook = 3;   // 4
constexpr uint8_t CrossBody = 4;  // 2 to the body
constexpr uint8_t HookBody = 5;   // 3 to the body
constexpr uint8_t PunchCount = 6;
// Uppercuts (slot 1)
constexpr uint8_t LeadUppercut = 0;  // 5
constexpr uint8_t RearUppercut = 1;  // 6
// Slip (slot 2)
constexpr uint8_t SlipChamber = 0;
constexpr uint8_t Haymaker = 1;  // the counter
// Roll (slot 3)
constexpr uint8_t Roll = 0;
constexpr uint8_t ResonanceRoll = 1;
}  // namespace WadeForm

// ---------------------------------------------------------------------------
// Infusions
// ---------------------------------------------------------------------------

enum class InfusionTree : uint8_t {
  Offense = 0,
  Defense = 1,
  Mobility = 2,
  Genealogy = 3
};
constexpr int TREE_COUNT = 4;
constexpr int MAX_TREE_LEVEL = 3;

const char* treeName(InfusionTree t);

struct InfusionText {
  const char* name;
  const char* description;
};

// Core ability granted by the first infusion (activated with E).
const AbilityInfo& coreAbility(Genus g);
// Effect granted by slotting an infusion of `g` into `tree`.
const InfusionText& infusionText(Genus g, InfusionTree tree);

// World item ids used for arena pickups (reuses ItemSpawned/ItemPickedUp).
namespace PickupId {
constexpr uint32_t InfusionBase = 1000;  // + Genus value (1..4)
constexpr uint32_t Scrap = 2000;
constexpr uint32_t Medkit = 2001;
inline bool isInfusion(uint32_t id) { return id > 1000 && id <= 1004; }
inline Genus infusionGenus(uint32_t id) {
  return static_cast<Genus>(id - InfusionBase);
}
}  // namespace PickupId

// ---------------------------------------------------------------------------
// Match tuning
// ---------------------------------------------------------------------------

enum class MatchPhase : uint8_t {
  Lobby = 0,         // waiting for players to pick characters
  Round = 1,         // out in the field, zone shrinking
  Intermission = 2,  // back at the ship: slot infusions, ready up
  BossFight = 3,     // final round
  Victory = 4,
  Defeat = 5
};

namespace Tuning {
constexpr int ROUND_COUNT = 3;  // regular rounds before the boss
constexpr float ROUND_SECONDS[ROUND_COUNT] = {150.0f, 165.0f, 180.0f};
// The zone is the round's reach: wide enough that each round opens a new
// ring of points of interest, and it holds for most of the round so there is
// time to explore. It only closes at the end, as the run home.
constexpr float ZONE_START_RADIUS[ROUND_COUNT] = {2000.0f, 2500.0f, 2900.0f};
constexpr float ZONE_END_RADIUS = 300.0f;
constexpr float ZONE_HOLD_FRACTION = 0.75f;  // of the round before shrinking
constexpr float ZONE_DAMAGE_PER_SECOND = 0.05f;  // fraction of max health
constexpr float BOSS_ZONE_RADIUS = 950.0f;
constexpr float INTERMISSION_SECONDS = 40.0f;
constexpr float LOBBY_COUNTDOWN_SECONDS = 3.0f;

// Enemy pressure. During a round the enemies live in camps at the map's
// points of interest (placed in arena.tmx); nothing spawns around the
// players. The only other pressure is raids on the ship. The living-enemy
// target below is for the boss fight only:
//   (BASE + PER_ROUND * round) * (0.55 + 0.45 * players) * ramp
constexpr float ENEMIES_BASE = 4.0f;
constexpr float ENEMIES_PER_ROUND = 4.0f;
constexpr float ENEMIES_RAMP = 0.6f;
constexpr float SPAWN_INTERVAL_MS = 3200.0f;  // boss fight top-up interval
// Objectives this far from the ship pay a second infusion.
constexpr float FAR_OBJECTIVE_DISTANCE = 1900.0f;
constexpr float RAID_FIRST_MS = 45000.0f;
constexpr float RAID_INTERVAL_MS = 45000.0f;
constexpr float SHIP_DAMAGE_SCALE = 0.5f;  // hull armor vs. enemy hits

constexpr float SHIP_MAX_HEALTH = 1500.0f;
constexpr float SHIP_RADIUS = 110.0f;       // enemies attack inside this
constexpr float SHIP_SAFE_RADIUS = 300.0f;  // "inside for the night"
// Walk anything you are hauling (scrap, a probe) inside this to deliver it.
constexpr float SHIP_DROP_RADIUS = 190.0f;
constexpr float SHIP_REPAIR_PER_SCRAP = 180.0f;
constexpr int SHIP_TURRET_SCRAP = 3;  // scrap banked before turrets come online

// Standing by the ship patches you up, at the cost of time on the clock.
constexpr float SHIP_HEAL_PER_SECOND = 0.04f;  // fraction of max health

constexpr float REVIVE_SECONDS = 3.0f;
constexpr float REVIVE_RADIUS = 70.0f;
constexpr float REVIVE_HEALTH_FRACTION = 0.4f;

constexpr float PLAYER_RADIUS = 16.0f;
constexpr float PICKUP_RADIUS = 40.0f;

constexpr int SCORE_OBJECTIVE = 150;
constexpr int SCORE_SCRAP = 100;
constexpr int SCORE_REVIVE = 120;
constexpr int SCORE_DEATH = -100;
constexpr int SCORE_BOSS = 500;
}  // namespace Tuning

}  // namespace Arena
