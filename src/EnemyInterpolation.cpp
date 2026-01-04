#include "EnemyInterpolation.h"

#include <SDL2/SDL.h>

#include "AnimationAssetLoader.h"
#include "AnimationSystem.h"
#include "DamageNumberSystem.h"
#include "Logger.h"

EnemyInterpolation::EnemyInterpolation(AnimationSystem* animSystem)
    : animationSystem(animSystem) {
  // Subscribe to network packet events
  EventBus::instance().subscribe<NetworkPacketReceivedEvent>(
      [this](const NetworkPacketReceivedEvent& e) {
        onNetworkPacketReceived(e);
      });

  Logger::info("EnemyInterpolation initialized");
}

void EnemyInterpolation::onNetworkPacketReceived(
    const NetworkPacketReceivedEvent& e) {
  if (e.size == 0) return;

  PacketType type = static_cast<PacketType>(e.data[0]);

  if (type == PacketType::EnemyStateUpdate) {
    EnemyStateUpdatePacket packet = deserializeEnemyStateUpdate(e.data, e.size);

    for (const auto& enemyState : packet.enemies) {
      updateEnemyState(enemyState);
    }
  } else if (type == PacketType::EnemyDied) {
    EnemyDiedPacket packet = deserializeEnemyDied(e.data, e.size);
    removeEnemy(packet.enemyId);

    Logger::info("Enemy " + std::to_string(packet.enemyId) +
                 " was killed by player " + std::to_string(packet.killerId));
  }
}

void EnemyInterpolation::updateEnemyState(const NetworkEnemyState& state) {
  uint32_t enemyId = state.id;

  // Create enemy if first time seeing it
  if (enemies.find(enemyId) == enemies.end()) {
    Enemy enemy;
    enemy.id = state.id;
    enemy.type = static_cast<EnemyType>(state.type);
    enemy.state = static_cast<::EnemyState>(state.state);
    enemy.x = state.x;
    enemy.y = state.y;
    enemy.vx = state.vx;
    enemy.vy = state.vy;
    enemy.health = state.health;
    enemy.maxHealth = state.maxHealth;

    // Load animations (POC: same as player for now)
    AnimationAssetLoader::loadPlayerAnimations(enemy.animController,
                                               "assets/player_animated.png");

    enemies[enemyId] = enemy;

    // Register with animation system
    animationSystem->registerEntity(&enemies[enemyId]);

    Logger::info("Added new enemy ID=" + std::to_string(enemyId) +
                 " type=" + std::to_string(static_cast<int>(enemy.type)));
  }

  // Update enemy state
  Enemy& enemy = enemies[enemyId];
  enemy.state = static_cast<::EnemyState>(state.state);

  // Check for health changes (damage or healing)
  float oldHealth = enemy.health;
  enemy.health = state.health;
  enemy.maxHealth = state.maxHealth;

  if (enemy.health < oldHealth && enemy.state != ::EnemyState::Dead) {
    // Enemy took damage - publish event for damage numbers
    float damageTaken = oldHealth - enemy.health;
    DamageDealtEvent damageEvent;
    damageEvent.x = state.x;  // Use server's authoritative position
    damageEvent.y = state.y;
    damageEvent.damageAmount = damageTaken;
    damageEvent.isCritical = false;  // TODO: Add crit system later
    EventBus::instance().publish(damageEvent);
  } else if (enemy.health > oldHealth && enemy.state != ::EnemyState::Dead) {
    // Enemy was healed - publish event for healing numbers
    float healAmount = enemy.health - oldHealth;
    HealingEvent healEvent;
    healEvent.x = state.x;
    healEvent.y = state.y;
    healEvent.healAmount = healAmount;
    EventBus::instance().publish(healEvent);
  }

  // Add snapshot for interpolation
  EnemySnapshot snapshot;
  snapshot.x = state.x;
  snapshot.y = state.y;
  snapshot.vx = state.vx;
  snapshot.vy = state.vy;
  snapshot.health = state.health;
  snapshot.state = state.state;
  snapshot.timestamp = SDL_GetTicks64();

  auto& queue = snapshots[enemyId];
  queue.push_back(snapshot);

  if (queue.size() > MAX_SNAPSHOTS) {
    queue.pop_front();
  }
}

void EnemyInterpolation::removeEnemy(uint32_t enemyId) {
  auto it = enemies.find(enemyId);
  if (it != enemies.end()) {
    animationSystem->unregisterEntity(&it->second);
    enemies.erase(it);
  }
  snapshots.erase(enemyId);

  Logger::info("Removed enemy ID=" + std::to_string(enemyId));
}

bool EnemyInterpolation::getInterpolatedState(uint32_t enemyId,
                                              float interpolation,
                                              Enemy& outEnemy) const {
  auto it = enemies.find(enemyId);
  if (it == enemies.end()) {
    return false;
  }

  const Enemy& enemy = it->second;

  auto snapIt = snapshots.find(enemyId);
  if (snapIt == snapshots.end() || snapIt->second.size() < 2) {
    // Not enough snapshots, return current state
    outEnemy = enemy;
    return true;
  }

  // Interpolate between last two snapshots
  const auto& queue = snapIt->second;
  const EnemySnapshot& prev = queue[queue.size() - 2];
  const EnemySnapshot& curr = queue[queue.size() - 1];

  outEnemy = enemy;
  outEnemy.x = prev.x + (curr.x - prev.x) * interpolation;
  outEnemy.y = prev.y + (curr.y - prev.y) * interpolation;
  outEnemy.vx = curr.vx;
  outEnemy.vy = curr.vy;

  return true;
}

std::vector<uint32_t> EnemyInterpolation::getEnemyIds() const {
  std::vector<uint32_t> ids;
  ids.reserve(enemies.size());
  for (const auto& [id, enemy] : enemies) {
    ids.push_back(id);
  }
  return ids;
}
