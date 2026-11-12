#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "Animatable.h"
#include "ArenaData.h"
#include "AnimationController.h"
#include "CollisionSystem.h"
#include "Item.h"
#include "MovementInput.h"
#include "config/PlayerConfig.h"

constexpr int INVENTORY_SIZE = 20;
constexpr int EQUIPMENT_WEAPON_SLOT = 0;
constexpr int EQUIPMENT_ARMOR_SLOT = 1;
constexpr int EQUIPMENT_SLOTS = 2;

struct Player : public Animatable {
  uint32_t id;
  uint32_t characterId;  // Character from selection screen (1-19)
  float x, y;
  float vx, vy;
  float health;
  uint8_t r, g, b;

  // Animation (shared_ptr allows Player to be copyable for remote
  // interpolation)
  std::shared_ptr<AnimationController> animationController;

  // Inventory
  ItemStack inventory[INVENTORY_SIZE];
  ItemStack equipment[EQUIPMENT_SLOTS];  // [0] = weapon, [1] = armor

  // Server-only fields
  uint32_t lastInputSequence;
  float deathTime;  // Server tick when player died (0 = alive)

  // Client-only fields
  uint32_t lastServerTick;

  // --- Arena combat state -------------------------------------------------
  // Server authoritative; the subset in PlayerState is mirrored to clients.
  float maxHealth = Config::Player::MAX_HEALTH;
  float moveSpeed = Config::Player::SPEED;
  float shield = 0.0f;
  float shieldMs = 0.0f;  // remaining shield duration
  float aimX = 0.0f, aimY = 0.0f;
  uint8_t buttons = 0, prevButtons = 0;
  // Presses seen in any input packet since the last ability tick. Several
  // packets can land in one server tick; a quick tap must not be lost.
  uint8_t pressedLatch = 0, lastPacketButtons = 0;
  float cooldownMs[Arena::SLOT_COUNT] = {0, 0, 0, 0, 0};
  int chargeSlot = -1;        // slot being charged / channelled
  float chargeMs = 0.0f;      // time held so far
  float channelTimerMs = 0.0f;
  float actionLockMs = 0.0f;  // brief lock after a cast
  // Forced motion (dash / roll / pull). Input is ignored while it runs.
  float forcedVx = 0.0f, forcedVy = 0.0f, forcedMs = 0.0f;
  bool forcedGhost = false;    // passes through enemies while dashing
  float intangibleMs = 0.0f;   // cannot be hit
  float hiddenMs = 0.0f;       // enemies cannot target
  float speedMult = 1.0f;      // mirrored so prediction matches the server
  uint8_t stateFlags = 0;      // PlayerFlags, mirrored
  uint8_t resource = 0;        // kit meter (0-255)
  // Scratch state for the active kit (meaning is per character).
  float kitTimer[4] = {0, 0, 0, 0};
  int kitCounter[4] = {0, 0, 0, 0};
  uint32_t kitTarget = 0;
  bool godMode = false;
  // Effect-lab training ally: takes effects but never takes damage.
  bool trainingDummy = false;

  // Downed / revive
  float reviveProgress = 0.0f;

  // Infusions (suit loadout)
  Arena::Genus coreGenus = Arena::Genus::None;
  Arena::Genus treeGenus[Arena::TREE_COUNT] = {
      Arena::Genus::None, Arena::Genus::None, Arena::Genus::None,
      Arena::Genus::None};
  uint8_t treeLevel[Arena::TREE_COUNT] = {0, 0, 0, 0};
  uint8_t infusionInventory[Arena::GENUS_COUNT] = {0, 0, 0, 0};
  float infusionTimer[4] = {0, 0, 0, 0};  // per-tree cooldown / accumulators
  int infusionCounter[4] = {0, 0, 0, 0};
  float coreTimerMs = 0.0f;  // core ability active window

  // Match stats
  float scoreAccum = 0.0f;  // fractional points
  int32_t score = 0;
  uint16_t kills = 0;
  uint8_t deaths = 0;
  bool ready = false;
  bool carryingScrap = false;

  // Level of `genus` infusion slotted in `tree` (0 = none).
  int infusionLevel(Arena::Genus genus, Arena::InfusionTree tree) const {
    int t = static_cast<int>(tree);
    return treeGenus[t] == genus ? treeLevel[t] : 0;
  }

  Player()
      : id(0),
        characterId(0),
        x(0),
        y(0),
        vx(0),
        vy(0),
        health(Config::Player::MAX_HEALTH),
        r(255),
        g(255),
        b(255),
        animationController(std::make_shared<AnimationController>()),
        lastInputSequence(0),
        deathTime(0.0f),
        lastServerTick(0) {}

  // Helper methods
  bool isDead() const { return health <= 0.0f; }
  bool isAlive() const { return health > 0.0f; }

  // Inventory helper methods
  int findEmptySlot() const {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
      if (inventory[i].isEmpty()) {
        return i;
      }
    }
    return -1;
  }

  int findItemStack(uint32_t itemId) const {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
      if (inventory[i].itemId == itemId && !inventory[i].isEmpty()) {
        return i;
      }
    }
    return -1;
  }

  bool hasEquippedWeapon() const {
    return !equipment[EQUIPMENT_WEAPON_SLOT].isEmpty();
  }
  bool hasEquippedArmor() const {
    return !equipment[EQUIPMENT_ARMOR_SLOT].isEmpty();
  }

  // Animatable interface implementation
  AnimationController* getAnimationController() override {
    return animationController.get();
  }

  const AnimationController* getAnimationController() const override {
    return animationController.get();
  }

  float getVelocityX() const override { return vx; }

  float getVelocityY() const override { return vy; }
};

struct MovementModifiers {
  float speedMultiplier;  // From effects (Slow/Haste)
  bool canMove;           // From effects (Stunned/Snared)

  MovementModifiers() : speedMultiplier(1.0f), canMove(true) {}
  MovementModifiers(float speed, bool move)
      : speedMultiplier(speed), canMove(move) {}
};

inline void applyInput(
    Player& player, const MovementInput& input,
    const MovementModifiers& modifiers = MovementModifiers()) {
  // Check if player can move (stunned, snared, grappled)
  if (!modifiers.canMove) {
    player.vx = 0.0f;
    player.vy = 0.0f;
    return;
  }

  float dx = 0, dy = 0;

  // Simple orthogonal movement in isometric world space
  // worldX/worldY are already screen-space isometric coordinates from
  // gridToWorld
  if (input.moveUp) {
    dy -= 1;  // Up on screen = decrease worldY
  }
  if (input.moveRight) {
    dx += 1;  // Right on screen = increase worldX
  }
  if (input.moveDown) {
    dy += 1;  // Down on screen = increase worldY
  }
  if (input.moveLeft) {
    dx -= 1;  // Left on screen = decrease worldX
  }

  float len = std::sqrt(dx * dx + dy * dy);
  if (len > 0) {
    dx /= len;
    dy /= len;
  }

  float effectiveSpeed = player.moveSpeed * modifiers.speedMultiplier;
  player.vx = dx * effectiveSpeed;
  player.vy = dy * effectiveSpeed;

  // Update animation state based on new velocity
  if (player.animationController) {
    player.animationController->updateAnimationState(player.vx, player.vy);
  }

  // Calculate new position
  float oldX = player.x;
  float oldY = player.y;
  float newX = player.x + player.vx * input.deltaTime / 1000.0f;
  float newY = player.y + player.vy * input.deltaTime / 1000.0f;

  // Check collision if system is provided
  if (input.collisionSystem != nullptr) {
    input.collisionSystem->checkMovement(oldX, oldY, newX, newY,
                                         Config::Player::RADIUS);
  }

  player.x = newX;
  player.y = newY;

  // Clamp to diamond-shaped isometric map bounds
  // For isometric diamond: |x/maxX| + |y/maxY| <= 1
  float halfWidth = input.worldWidth / 2.0f;
  float halfHeight = input.worldHeight / 2.0f;

  // Check if player is outside the diamond
  if (halfWidth > 0 && halfHeight > 0) {
    float normX = std::abs(player.x) / halfWidth;
    float normY = std::abs(player.y) / halfHeight;

    if (normX + normY > 1.0f) {
      // Player is outside diamond, clamp to diamond edge
      // Project back onto the diamond boundary
      float scale = 1.0f / (normX + normY);
      player.x *= scale;
      player.y *= scale;
    }
  }
}
