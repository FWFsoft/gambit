#include "CombatSystem.h"

#include <cmath>

#include "Logger.h"
#include "NetworkProtocol.h"
#include "Player.h"

CombatSystem::CombatSystem(NetworkClient* netClient,
                           ClientPrediction* prediction,
                           EnemyInterpolation* enemyInterp)
    : networkClient(netClient),
      clientPrediction(prediction),
      enemyInterpolation(enemyInterp) {
  // Subscribe to attack input
  EventBus::instance().subscribe<AttackInputEvent>(
      [this](const AttackInputEvent& e) { onAttackInput(e); });

  Logger::info("CombatSystem initialized");
}

void CombatSystem::onAttackInput(const AttackInputEvent& e) {
  const Player& player = clientPrediction->getLocalPlayer();

  // Dead players cannot attack
  if (player.health <= 0) {
    return;
  }

  // Find nearest enemy within attack range
  uint32_t enemyId = findNearestEnemy(player.x, player.y, ATTACK_RANGE);

  if (enemyId == 0) {
    // No enemies in range - silent fail
    return;
  }

  // Send attack packet to server
  AttackEnemyPacket packet;
  packet.enemyId = enemyId;
  packet.damage = ATTACK_DAMAGE;

  networkClient->send(serialize(packet));

  Logger::info("Attacked enemy ID=" + std::to_string(enemyId));
}

uint32_t CombatSystem::findNearestEnemy(float playerX, float playerY,
                                        float maxRange) const {
  auto enemyIds = enemyInterpolation->getEnemyIds();

  uint32_t nearestId = 0;
  float nearestDist = maxRange;

  for (uint32_t id : enemyIds) {
    Enemy enemy;
    if (enemyInterpolation->getInterpolatedState(id, 0.0f, enemy)) {
      // Skip dead enemies
      if (enemy.state == ::EnemyState::Dead) {
        continue;
      }

      float dist = distance(playerX, playerY, enemy.x, enemy.y);

      if (dist < nearestDist) {
        nearestId = id;
        nearestDist = dist;
      }
    }
  }

  return nearestId;
}

float CombatSystem::distance(float x1, float y1, float x2, float y2) const {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}
