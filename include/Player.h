#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "Animatable.h"
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

  float effectiveSpeed = Config::Player::SPEED * modifiers.speedMultiplier;
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
