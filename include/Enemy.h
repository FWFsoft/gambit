#pragma once

#include <cstdint>

#include "Animatable.h"
#include "AnimationController.h"

enum class EnemyState : uint8_t { Idle = 0, Chase = 1, Attack = 2, Dead = 3 };

enum class EnemyType : uint8_t {
  Slime = 0,    // POC: Only implement this one
  Goblin = 1,   // Future
  Skeleton = 2  // Future
};

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
        type(EnemyType::Slime),
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
        targetPlayerId(0) {}
};
