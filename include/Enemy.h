#pragma once

#include <cstdint>

#include "Animatable.h"
#include "AnimationController.h"
#include "ArenaData.h"

// Attack = winding up a telegraphed attack.
enum class EnemyState : uint8_t { Idle = 0, Chase = 1, Attack = 2, Dead = 3 };

// The twelve alien kinds (4 genera x 3 tiers) plus the boss.
using EnemyType = Arena::EnemyKind;

struct Enemy : public Animatable {
  uint32_t id;       // Unique enemy ID
  EnemyType type;    // Enemy type (determines sprite, stats, etc.)
  EnemyState state;  // Current AI state

  // Transform
  float x, y;    // World position
  float vx, vy;  // Velocity (for animation direction)

  // Combat stats
  float health;          // Current health
  float maxHealth;       // Maximum health
  float damage;          // Damage per attack
  float attackRange;     // Melee attack range
  float detectionRange;  // How far enemy can detect players
  float speed;           // Movement speed (pixels/second)
  float lastAttackTime;  // Timestamp of last attack (milliseconds)

  // Target tracking
  uint32_t targetPlayerId;  // Player being chased/attacked (0 = no target)

  // LittleJohn: guardian starts passive (won't chase) until player enters zone
  bool passive = false;

  // --- Arena combat state -------------------------------------------------
  float radius = 12.0f;
  float windupMs = 0.0f;       // remaining telegraph time (state == Attack)
  float windupTotalMs = 0.0f;
  float cooldownMs = 0.0f;     // until the next attack may start
  float aimX = 0.0f, aimY = 0.0f;  // where the attack was aimed
  float knockVx = 0.0f, knockVy = 0.0f, knockMs = 0.0f;
  float deadMs = 0.0f;         // time since death (removed after a delay)
  bool targetShip = false;     // marches on the ship instead of players
  bool elite = false;
  bool campGuard = false;      // stays near its camp until provoked
  float homeX = 0.0f, homeY = 0.0f;
  uint32_t lastHitBy = 0;      // player who last damaged it
  int bossMove = 0;            // boss attack rotation
  float bossTimerMs = 0.0f;
  uint8_t netFlags = 0;        // EnemyFlags, mirrored to clients

  // Respawn tracking
  uint32_t spawnIndex;  // Which spawn point this enemy came from
  float deathTime;      // Timestamp when enemy died (milliseconds)
  float respawnDelay;   // Random respawn delay in milliseconds (1-5 seconds)

  // Animation
  AnimationController animController;

  // Animatable interface (for AnimationSystem)
  AnimationController* getAnimationController() override {
    return &animController;
  }
  const AnimationController* getAnimationController() const override {
    return &animController;
  }
  float getVelocityX() const override { return vx; }
  float getVelocityY() const override { return vy; }

  // Constructor with defaults for Slime enemy type
  Enemy()
      : id(0),
        type(EnemyType::Toxlith),
        state(EnemyState::Idle),
        x(0.0f),
        y(0.0f),
        vx(0.0f),
        vy(0.0f),
        health(50.0f),
        maxHealth(50.0f),
        damage(10.0f),
        attackRange(40.0f),
        detectionRange(200.0f),
        speed(100.0f),
        lastAttackTime(0.0f),
        targetPlayerId(0),
        spawnIndex(0),
        deathTime(0.0f),
        respawnDelay(0.0f) {}
};
