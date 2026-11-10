#pragma once

#include <cstdint>

#include "AbilitySystem.h"

// Suit infusions: the core ability (E) and the four trees
// (Offense / Defense / Mobility / Genealogy) for each genus.
// See specs/genera-and-infusion-system.md. These are hooks called from the
// damage pipeline in AbilitySystem; loadout state lives on Player.
namespace Infusions {

// Core ability (slot E).
void castCore(AbilityContext& ctx);
bool canRecastCore(const Player& player);

// Offense + Genealogy: the player damaged an enemy.
void onHitEnemy(AbilitySystem& sys, Player& player, Enemy& enemy, float damage,
                uint32_t flags);
// Defense: returns the damage that should actually be applied.
float onPlayerDamaged(AbilitySystem& sys, Player& player, float damage,
                      Enemy* source, uint32_t flags);
// Genealogy (Unbreakable): the player stunned or snared an enemy.
void onHardCrowdControl(AbilitySystem& sys, Player& player, Enemy& enemy);
// Mobility (Burning Rush).
void onKill(AbilitySystem& sys, Player& player, const Enemy& enemy);
// Mobility (Phase Shift): the player dashed or blinked from a to b.
void onDash(AbilitySystem& sys, Player& player, float x0, float y0, float x1,
            float y1);
void tick(AbilitySystem& sys, Player& player, float dtMs);
float speedMultiplier(const Player& player);
// Defense (Mind Barrier): immune to crowd control while the barrier holds.
bool blocksCrowdControl(const Player& player);

// Slot one infusion from the player's inventory. slot 0 = core,
// 1-4 = tree index + 1. Returns false if the move is not allowed.
bool slotInfusion(Player& player, Arena::Genus genus, int slot);

}  // namespace Infusions
