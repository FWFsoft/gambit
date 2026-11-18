#pragma once

#include <cmath>
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
constexpr uint32_t Kade = 6;
constexpr uint32_t Mina = 9;
constexpr uint32_t Nolan = 12;
constexpr uint32_t Nyx = 13;
constexpr uint32_t Presidente = 14;
constexpr uint32_t Suds = 16;
constexpr uint32_t Volgore = 18;
constexpr uint32_t Wade = 19;
constexpr uint32_t Zephyr = 20;
}  // namespace CharacterId

// Nyx's mirrored state (Player::kitMode): the swarm in the low nibble, Nyx
// herself in the high nibble. kitX/kitY is the swarm, propX/propY the CRT.
namespace NyxMode {
// Swarm (kitMode & 0x0F)
constexpr uint8_t SwarmHome = 0;      // with Nyx, around her body
constexpr uint8_t SwarmFlying = 1;    // on its way somewhere
constexpr uint8_t SwarmParked = 2;    // hanging at a point
constexpr uint8_t SwarmOnEnemy = 3;   // attached: the foe is Silenced
constexpr uint8_t SwarmOnAlly = 4;    // attached: the ally is Focused
constexpr uint8_t SwarmReturning = 5;
// Nyx (kitMode >> 4)
constexpr uint8_t Body = 0;       // walking, the CRT on her shoulder
constexpr uint8_t Dropping = 1;   // setting the CRT down, about to dissolve
constexpr uint8_t Flying = 2;     // a cloud, on the way to her target
constexpr uint8_t OnEnemy = 3;    // a cloud riding a foe
constexpr uint8_t OnAlly = 4;     // a cloud riding an ally
constexpr uint8_t Homing = 5;     // a cloud, streaming back to the CRT
constexpr uint8_t Reforming = 6;  // body again, lifting the CRT
inline uint8_t swarm(uint8_t mode) { return mode & 0x0F; }
inline uint8_t self(uint8_t mode) { return mode >> 4; }
inline uint8_t pack(uint8_t swarm, uint8_t self) {
  return static_cast<uint8_t>((self << 4) | (swarm & 0x0F));
}
}  // namespace NyxMode

// Kade's mirrored state (Player::kitMode). His mirrors are zones of
// variant KadeMode::MirrorZone owned by him; the barrier joins them in the
// order they were set down (oldest to newest).
namespace KadeMode {
constexpr uint8_t Ready = 0;
constexpr uint8_t Releasing = 1;  // Full Release: the beam is running
constexpr uint8_t Spent = 2;      // after it: hunched, recovering
constexpr uint8_t MirrorZone = 3;  // Zone variant for a mirror
constexpr int MAX_MIRRORS = 3;
// Refraction Wall is charge based: one charge per mirror, MAX_MIRRORS of
// them, each recharging in turn. Mirrored in Player::resource as
// charges * 64 + the next charge's progress (0-63).
constexpr float CHARGE_MS = 5000.0f;
inline int charges(uint8_t resource) { return resource / 64; }
inline float rechargeProgress(uint8_t resource) {
  return static_cast<float>(resource % 64) / 63.0f;
}
// Refraction Wall is held: the press pins the mirror at the target, and the
// cursor's direction from that point turns its glass (8 ways, as the sprite
// has). With the cursor on the pin the glass faces on, away from Kade.
// Whatever strikes a mirror leaves it straight out of the glass.
constexpr float AIM_DEADZONE = 24.0f;
inline void mirrorFacing(float pinX, float pinY, float aimX, float aimY,
                         float kadeX, float kadeY, float& dirX, float& dirY) {
  float dx = aimX - pinX, dy = aimY - pinY;
  if (dx * dx + dy * dy < AIM_DEADZONE * AIM_DEADZONE) {
    dx = pinX - kadeX;
    dy = pinY - kadeY;
  }
  if (dx * dx + dy * dy < 1.0e-4f) {
    dx = 0.0f;
    dy = 1.0f;
  }
  const float step = 0.78539816f;  // 45 degrees
  float a = std::round(std::atan2(dy, dx) / step) * step;
  dirX = std::cos(a);
  dirY = std::sin(a);
}
}  // namespace KadeMode

// Gravon's mirrored state (Player::kitMode). The player is the blade: while
// the host stands they are one; when he falls the player *is* the blade, on
// the floor or on an ally's back. propX/propY is where the host fell, and
// kitX holds the carrier's player id.
namespace GravonMode {
// Form (kitMode & 0x0F)
constexpr uint8_t Host = 0;     // the man, blade in hand
constexpr uint8_t Exposed = 1;  // the blade alone on the floor: can be hit
constexpr uint8_t Carried = 2;  // the blade on an ally's back: safe
constexpr uint8_t FuryBit = 0x10;     // Fury of the Blade is running
constexpr uint8_t ChargedBit = 0x20;  // Ravager's Edge is at full charge
constexpr uint8_t DrawnBit = 0x40;    // the blade is out (else on his back)
inline uint8_t form(uint8_t mode) { return mode & 0x0F; }
}  // namespace GravonMode

// Gravon's ability forms (Player::castVariant, sent with AbilityCast).
namespace GravonForm {
// slot 0, Titan's Slash
constexpr uint8_t SlashForehand = 0;  // two hands, right to left
constexpr uint8_t SlashBackhand = 1;  // two hands, left to right
constexpr uint8_t SlashHeave = 2;     // off his back, one hand, backhand
constexpr uint8_t SlashHeaveRun = 3;  // up from the drag, one hand, forehand
constexpr uint8_t RavagerFull = 1;    // slot 1: the fully charged slam
constexpr uint8_t RushStab = 1;       // slot 3: the second beat, the thrust
}  // namespace GravonForm

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

const char* treeName(InfusionTree t);

struct InfusionText {
  const char* name;
  const char* description;
};

// Core ability granted by the first infusion (activated with E).
const AbilityInfo& coreAbility(Genus g);
// Genus effect granted by slotting an infusion of `g` anywhere in `tree`.
// Every further infusion of `g` in the same tree raises its level.
const InfusionText& infusionText(Genus g, InfusionTree tree);

// ---- Infusion items --------------------------------------------------------
// An infusion is an item: a genus (what it does when slotted) and a body
// part (what it gives on its own, wherever it is slotted). "Venoxarid Talon".
enum class InfusionPart : uint8_t { Talon = 0, Hide = 1, Eye = 2, Sinew = 3 };
constexpr int PART_COUNT = 4;

struct InfusionPartInfo {
  const char* name;
  const char* description;  // the item's own effect, per copy slotted
  float damageBonus;        // fraction of all damage dealt
  float maxHealthBonus;     // flat
  float rechargeBonus;      // fraction faster ability recharge
  float speedBonus;         // fraction of move speed
};
const InfusionPartInfo& partInfo(InfusionPart part);

// ---- Suit layout ------------------------------------------------------------
// The suit is the core plus four trees. Each tree is rows of nodes (tiers):
// a node at tier N can be filled once the tree has a node at tier N-1 (tier 1
// needs the core), and only one node per tier per tree can be filled, so each
// tier past the first is a multiple choice. Every node carries a talent, on
// top of the genus effect of the infusion slotted into it.
//
// The whole layout is the SUIT_NODES table in ArenaData.cpp. To grow the
// suit, add rows (depth) or options to a row (width) there and a matching
// Talent below; nothing else assumes the shape.
enum class Talent : uint8_t {
  // Offense
  KillerInstinct,
  OpeningStrike,
  Relentless,
  Affliction,
  Executioner,
  Bloodlust,
  FocusFire,
  // Defense
  Plating,
  SecondWind,
  Regrowth,
  Guardian,
  Thorns,
  LastStand,
  ShareTheLoad,
  // Mobility
  LightFrame,
  QuickRecovery,
  Slipstream,
  HitAndRun,
  Tailwind,
  Blitz,
  EscapeArtist,
  // Genealogy
  Bloodline,
  Purebred,
  HybridVigor,
  Overcharge,
  SharedGift,
  ApexMutation,
  FeedbackLoop,
  Count
};
constexpr int TALENT_COUNT = static_cast<int>(Talent::Count);
// Room for the suit to grow without touching save or network layouts.
constexpr int MAX_SUIT_NODES = 64;
static_assert(TALENT_COUNT <= MAX_SUIT_NODES, "suit outgrew MAX_SUIT_NODES");
// Node index used for the core in slot requests.
constexpr uint8_t CORE_NODE = 255;

struct SuitNode {
  InfusionTree tree;
  uint8_t tier;  // 1-based
  Talent talent;
  const char* name;
  const char* description;
};
int suitNodeCount();
// Nodes are indexed by talent: suitNode(i).talent == Talent(i).
const SuitNode& suitNode(int index);
// Number of tiers in a tree (the deepest node's tier).
int treeTierCount(InfusionTree tree);

// Where a node stands for a suit whose nodes hold `nodeGenus` (indexed like
// suitNode, Genus::None = empty). Shared by the server rules and the HUD.
enum class NodeState : uint8_t {
  Locked,  // the tier above (or the core) is not filled yet
  Open,    // can take an infusion now
  Filled,
  Closed  // another option in this tier was chosen
};
NodeState nodeState(const Genus* nodeGenus, bool coreFilled, int node);

// World item ids used for arena pickups (reuses ItemSpawned/ItemPickedUp).
// An infusion's id carries its genus and part: 1000 + genus * 16 + part.
namespace PickupId {
constexpr uint32_t InfusionBase = 1000;
constexpr uint32_t Scrap = 2000;
constexpr uint32_t Medkit = 2001;
inline uint32_t infusion(Genus genus, InfusionPart part) {
  return InfusionBase + static_cast<uint32_t>(genus) * 16u +
         static_cast<uint32_t>(part);
}
inline bool isInfusion(uint32_t id) {
  if (id < InfusionBase + 16u || id >= InfusionBase + 16u * (GENUS_COUNT + 1)) {
    return false;
  }
  return (id - InfusionBase) % 16u < static_cast<uint32_t>(PART_COUNT);
}
inline Genus infusionGenus(uint32_t id) {
  return static_cast<Genus>((id - InfusionBase) / 16u);
}
inline InfusionPart infusionPart(uint32_t id) {
  return static_cast<InfusionPart>((id - InfusionBase) % 16u);
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
// Isaac's Lifeline: how far a bond reaches (Isaac to an ally, or to the
// nearest face of a rock). The client rims rocks in reach.
constexpr float LIFELINE_RANGE = 520.0f;
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
// The match opens just after the crash: the hull is badly damaged and
// burning, and scrap hauled home is what patches it up.
constexpr float SHIP_START_HEALTH = SHIP_MAX_HEALTH * 0.35f;
// The ship is a crashed RV about 11 m long (some 510 px on screen, 256 to
// either side of its centre): these reach just past its hull.
constexpr float SHIP_RADIUS = 240.0f;       // enemies attack inside this
constexpr float SHIP_SAFE_RADIUS = 420.0f;  // "inside for the night"
// Walk anything you are hauling (scrap, a probe) inside this to deliver it.
constexpr float SHIP_DROP_RADIUS = 300.0f;
constexpr float SHIP_REPAIR_PER_SCRAP = 180.0f;
constexpr int SHIP_TURRET_SCRAP = 3;  // scrap banked before turrets come online

// Roof turrets. Each Derelict Turret objective the crew repairs stands one
// up on the ship's roof. It swivels on its round base to the nearest enemy
// in range and fires hard; a kill goes to whoever last hit the enemy, so it
// backs up the crew's fights instead of stealing them. Mounts are offsets
// from the ship's centre to the turret's foot on the roof as drawn
// (assets/sprites/actors/ShipTurret*.png on Ship.png).
constexpr int ROOF_TURRET_COUNT = 2;
constexpr float ROOF_TURRET_MOUNT_X[ROOF_TURRET_COUNT] = {-140.0f, -10.0f};
constexpr float ROOF_TURRET_MOUNT_Y[ROOF_TURRET_COUNT] = {-168.0f, -214.0f};
constexpr float ROOF_TURRET_RANGE = 560.0f;  // from the mount
constexpr float ROOF_TURRET_DAMAGE = 75.0f;  // a grunt (70) in one shot
constexpr float ROOF_TURRET_INTERVAL_MS = 1100.0f;
constexpr float ROOF_TURRET_TURN_DEG_PER_S = 300.0f;
constexpr float ROOF_TURRET_AIM_TOLERANCE_DEG = 12.0f;  // fires once this close
// The muzzle relative to the foot, as drawn, for a barrel at heading a
// (radians, clockwise from straight down): reach along the barrel on the
// ground (halved in y by the 30 degree camera) and height above the foot.
constexpr float ROOF_TURRET_BARREL_PX = 89.0f;
constexpr float ROOF_TURRET_MUZZLE_LIFT_PX = 46.0f;

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

// Hitstop: when a melee hit lands, the attacker's swing and the target both
// hold for a beat. Heavier hits hold longer; Haste cuts it, so a hasted
// fighter's hits run together (Wade: boom, pause, boom ... bap-bap-bap).
//   ms = min(MAX, BASE + PER_DAMAGE * power) / (1 + HASTE_CUT * haste)
// where power is the hit's damage after crit and Empowered, before armour.
constexpr float HITSTOP_BASE_MS = 60.0f;
constexpr float HITSTOP_MS_PER_DAMAGE = 2.0f;
constexpr float HITSTOP_MAX_MS = 250.0f;
constexpr float HITSTOP_HASTE_CUT = 0.3f;
// The boss barely flinches, so four players cannot pin it in place.
constexpr float HITSTOP_BOSS_TARGET_SCALE = 0.25f;
}  // namespace Tuning

}  // namespace Arena
