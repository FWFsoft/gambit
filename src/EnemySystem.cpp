#include "EnemySystem.h"

#include <cassert>
#include <cmath>
#include <random>

#include "Logger.h"
#include "config/GameplayConfig.h"

EnemySystem::EnemySystem(const std::vector<EnemySpawn>& spawns)
    : spawns(spawns), nextEnemyId(1), accumulatedTime(0.0f) {
  Logger::info("EnemySystem initialized with " + std::to_string(spawns.size()) +
               " spawn points");
}

void EnemySystem::spawnAllEnemies() {
  for (size_t i = 0; i < spawns.size(); ++i) {
    const auto& spawn = spawns[i];
    Enemy enemy;
    enemy.id = nextEnemyId++;
    enemy.type = spawn.type;
    enemy.state = EnemyState::Idle;
    enemy.x = spawn.x;
    enemy.y = spawn.y;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    enemy.spawnIndex = static_cast<uint32_t>(i);

    // Set stats based on type (POC: only Slime)
    if (spawn.type == EnemyType::Slime) {
      enemy.maxHealth = 50.0f;
      enemy.health = 50.0f;
      enemy.damage = 1.0f;
      enemy.attackRange = 40.0f;
      enemy.detectionRange = 200.0f;
      enemy.speed = 100.0f;
    }

    enemies[enemy.id] = enemy;

    Logger::info("Spawned enemy ID=" + std::to_string(enemy.id) +
                 " type=" + std::to_string(static_cast<int>(enemy.type)) +
                 " at (" + std::to_string(enemy.x) + ", " +
                 std::to_string(enemy.y) + ")");
  }
}

void EnemySystem::update(float deltaTime,
                         std::unordered_map<uint32_t, Player>& players) {
  // Clear death events from last frame
  diedThisFrame.clear();

  // Update accumulated time (convert seconds to milliseconds)
  accumulatedTime += deltaTime * 1000.0f;

  for (auto& [id, enemy] : enemies) {
    // Check if dead enemy should respawn (random delay)
    if (enemy.state == EnemyState::Dead) {
      if (accumulatedTime - enemy.deathTime >= enemy.respawnDelay) {
        Logger::info("Enemy time" + std::to_string(enemy.deathTime) +
                     " respawn delay " + std::to_string(enemy.respawnDelay));
        // Respawn enemy at original spawn point
        assert(enemy.spawnIndex < spawns.size() && "Invalid spawn index");
        const EnemySpawn& spawn = spawns[enemy.spawnIndex];

        enemy.x = spawn.x;
        enemy.y = spawn.y;
        enemy.vx = 0.0f;
        enemy.vy = 0.0f;
        enemy.health = enemy.maxHealth;
        enemy.state = EnemyState::Idle;
        enemy.targetPlayerId = 0;
        enemy.deathTime = 0.0f;
        enemy.respawnDelay = 0.0f;

        Logger::info("Enemy " + std::to_string(id) + " respawned at spawn " +
                     std::to_string(enemy.spawnIndex));
      }

      continue;  // Don't update AI for dead enemies
    }

    updateEnemyAI(enemy, players, deltaTime);
  }
}

void EnemySystem::updateEnemyAI(Enemy& enemy,
                                std::unordered_map<uint32_t, Player>& players,
                                float deltaTime) {
  switch (enemy.state) {
    case EnemyState::Idle:
      updateIdleState(enemy, players);
      break;

    case EnemyState::Chase:
      updateChaseState(enemy, players, deltaTime);
      break;

    case EnemyState::Attack:
      updateAttackState(enemy, players, deltaTime);
      break;

    case EnemyState::Dead:
      // Do nothing (already dead)
      break;
  }
}

void EnemySystem::updateIdleState(
    Enemy& enemy, const std::unordered_map<uint32_t, Player>& players) {
  // Look for nearest player within detection range
  const Player* nearestPlayer =
      findNearestPlayer(enemy, players, enemy.detectionRange);

  if (nearestPlayer != nullptr) {
    // Found a target - transition to Chase
    enemy.targetPlayerId = nearestPlayer->id;
    enemy.state = EnemyState::Chase;

    Logger::debug("Enemy " + std::to_string(enemy.id) + " detected player " +
                  std::to_string(nearestPlayer->id) + ", entering Chase state");
  } else {
    // No targets nearby, stay idle
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
  }
}

void EnemySystem::updateChaseState(
    Enemy& enemy, const std::unordered_map<uint32_t, Player>& players,
    float deltaTime) {
  // Find target player
  auto it = players.find(enemy.targetPlayerId);
  if (it == players.end()) {
    // Target player disconnected or died - return to Idle
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  const Player& target = it->second;

  // Check if target died
  if (target.isDead()) {
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  // Calculate distance to target
  float dist = distance(enemy.x, enemy.y, target.x, target.y);

  // Check if in attack range
  if (dist <= enemy.attackRange) {
    // Transition to Attack state
    enemy.state = EnemyState::Attack;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  // Check if target escaped detection range
  if (dist >
      enemy.detectionRange * 1.2f) {  // Hysteresis: 120% of detection range
    // Lost target - return to Idle
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    return;
  }

  // Move toward target
  float dx = target.x - enemy.x;
  float dy = target.y - enemy.y;
  float dirLength = std::sqrt(dx * dx + dy * dy);

  if (dirLength > 0.001f) {
    // Normalize direction and apply speed
    float normX = dx / dirLength;
    float normY = dy / dirLength;

    enemy.vx = normX * enemy.speed;
    enemy.vy = normY * enemy.speed;

    // Update position (deltaTime is in milliseconds, convert to seconds)
    float deltaSeconds = deltaTime / 1000.0f;
    enemy.x += enemy.vx * deltaSeconds;
    enemy.y += enemy.vy * deltaSeconds;
  }
}

void EnemySystem::updateAttackState(
    Enemy& enemy, std::unordered_map<uint32_t, Player>& players,
    float deltaTime) {
  // Find target player
  auto it = players.find(enemy.targetPlayerId);
  if (it == players.end()) {
    // Target player disconnected or died - return to Idle
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    return;
  }

  Player& target = it->second;

  // Skip dead players
  if (target.health <= 0.0f) {
    enemy.targetPlayerId = 0;
    enemy.state = EnemyState::Idle;
    return;
  }

  // Calculate distance to target
  float dist = distance(enemy.x, enemy.y, target.x, target.y);

  // Check if target moved out of attack range
  if (dist > enemy.attackRange * 1.2f) {  // Hysteresis
    // Return to Chase state
    enemy.state = EnemyState::Chase;
    return;
  }

  // Check attack cooldown
  if (accumulatedTime - enemy.lastAttackTime >=
      Config::Gameplay::ENEMY_ATTACK_COOLDOWN) {
    // Apply damage
    target.health -= enemy.damage;
    if (target.health < 0.0f) {
      target.health = 0.0f;
    }

    // Update attack timestamp
    enemy.lastAttackTime = accumulatedTime;

    Logger::debug("Enemy " + std::to_string(enemy.id) + " attacked player " +
                  std::to_string(target.id) + " for " +
                  std::to_string(enemy.damage) +
                  " damage, health: " + std::to_string(target.health));
  }

  // Stand still while in attack state
  enemy.vx = 0.0f;
  enemy.vy = 0.0f;
}

void EnemySystem::damageEnemy(uint32_t enemyId, float damage,
                              uint32_t attackerId) {
  auto it = enemies.find(enemyId);
  if (it == enemies.end()) {
    Logger::info("Attempted to damage non-existent enemy ID=" +
                 std::to_string(enemyId));
    return;
  }

  Enemy& enemy = it->second;

  // Skip if already dead
  if (enemy.state == EnemyState::Dead) {
    return;
  }

  // Apply damage
  enemy.health -= damage;

  Logger::debug("Enemy " + std::to_string(enemyId) + " took " +
                std::to_string(damage) + " damage, " +
                "health: " + std::to_string(enemy.health) + "/" +
                std::to_string(enemy.maxHealth));

  // Check if killed
  if (enemy.health <= 0.0f) {
    enemy.health = 0.0f;
    enemy.state = EnemyState::Dead;
    enemy.vx = 0.0f;
    enemy.vy = 0.0f;
    enemy.deathTime = accumulatedTime;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(5000.0f, 10000.0f);
    enemy.respawnDelay = dist(gen);

    Logger::info("Enemy " + std::to_string(enemyId) + " killed by player " +
                 std::to_string(attackerId) + " (respawn in " +
                 std::to_string(enemy.respawnDelay / 1000.0f) + "s)");

    // Track death for broadcasting
    diedThisFrame.push_back({enemyId, attackerId});
  }
}

const Player* EnemySystem::findNearestPlayer(
    const Enemy& enemy, const std::unordered_map<uint32_t, Player>& players,
    float maxRange) const {
  const Player* nearest = nullptr;
  float nearestDist = maxRange;

  for (const auto& [id, player] : players) {
    // Skip dead players
    if (player.isDead()) {
      continue;
    }

    float dist = distance(enemy.x, enemy.y, player.x, player.y);
    if (dist < nearestDist) {
      nearest = &player;
      nearestDist = dist;
    }
  }

  return nearest;
}

float EnemySystem::distance(float x1, float y1, float x2, float y2) const {
  float dx = x2 - x1;
  float dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}
