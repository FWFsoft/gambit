#pragma once

#include "ClientPrediction.h"
#include "EnemyInterpolation.h"
#include "EventBus.h"
#include "NetworkClient.h"

// Client-side combat system
// Handles attack input and sends attack packets to server
class CombatSystem {
 public:
  CombatSystem(NetworkClient* netClient, ClientPrediction* prediction,
               EnemyInterpolation* enemyInterp);

 private:
  NetworkClient* networkClient;
  ClientPrediction* clientPrediction;
  EnemyInterpolation* enemyInterpolation;

  static constexpr float ATTACK_RANGE = 150.0f;  // Increased from 50px to 150px
  static constexpr float ATTACK_DAMAGE = 25.0f;

  void onAttackInput(const AttackInputEvent& e);

  // Find nearest enemy within attack range
  uint32_t findNearestEnemy(float playerX, float playerY, float maxRange) const;

  float distance(float x1, float y1, float x2, float y2) const;
};
