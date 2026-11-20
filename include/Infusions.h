#pragma once

#include <cstdint>

#include "AbilitySystem.h"

// Suit infusions. See specs/genera-and-infusion-system.md.
//
// An infusion is an item with a genus and a body part. Slotted into the suit
// it gives three things:
//   - its part's own effect (Talon, Hide, Eye, Sinew), wherever it goes;
//   - in the core: the genus core ability (E);
//   - in a tree node: the genus effect for that tree (more infusions of the
//     same genus in the same tree level it up) and the node's talent.
// The suit layout and node rules are data in ArenaData (SUIT_NODES,
// nodeState). Everything here is server-side; loadout state lives on Player.
namespace Infusions {

// ---- loadout ---------------------------------------------------------------
// Put a picked-up item in the pack. False if the pack is full of that item.
bool addToInventory(Player& player, Arena::Genus genus,
                    Arena::InfusionPart part);
// Slot one item from the pack into `node` (Arena::CORE_NODE for the core).
// Returns false, changing nothing, if the move is not allowed.
// maxTier: the deepest tree tier allowed now (Arena::suitTierCap).
bool slotInfusion(Player& player, Arena::Genus genus, Arena::InfusionPart part,
                  int node, int maxTier = 4);
// Empty the suit and the pack (a new match).
void resetLoadout(Player& player);
// Clear per-round talent state (cooldowns, once-per-round uses).
void resetForRound(Player& player);
// Max health the slotted items add (Hide).
float maxHealthBonus(const Player& player);

// ---- core ability -----------------------------------------------------------
void castCore(AbilityContext& ctx);
bool canRecastCore(const Player& player);

// ---- combat hooks -----------------------------------------------------------
// Multiplier on damage the player is about to deal to `enemy`.
float damageMultiplier(AbilitySystem& sys, const Player& player,
                       const Enemy& enemy, uint32_t flags);
// Added to the player's critical strike chance.
float critChanceBonus(const Player& player);
// Multiplier on how fast the ability in `slot` recharges.
float rechargeMultiplier(const Player& player, int slot);
// The player damaged an enemy (not by a proc or damage over time).
void onHitEnemy(AbilitySystem& sys, Player& player, Enemy& enemy, float damage,
                uint32_t flags);
// Returns the damage that should actually be applied to the player.
float onPlayerDamaged(AbilitySystem& sys, Player& player, float damage,
                      Enemy* source, uint32_t flags);
// The player stunned or snared an enemy.
void onHardCrowdControl(AbilitySystem& sys, Player& player, Enemy& enemy);
void onKill(AbilitySystem& sys, Player& player, const Enemy& enemy);
// The player dashed or blinked from a to b.
void onDash(AbilitySystem& sys, Player& player, float x0, float y0, float x1,
            float y1);
// The player used the ability in `slot` (after it fired).
void onAbilityUsed(AbilitySystem& sys, Player& player, int slot);
void tick(AbilitySystem& sys, Player& player, float dtMs);
float speedMultiplier(const Player& player);
// Immune to crowd control while this holds.
bool blocksCrowdControl(const Player& player);

}  // namespace Infusions
