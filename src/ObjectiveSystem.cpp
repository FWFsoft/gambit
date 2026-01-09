#include "ObjectiveSystem.h"

#include "Logger.h"

ObjectiveSystem::ObjectiveSystem() = default;
ObjectiveSystem::~ObjectiveSystem() = default;

void ObjectiveSystem::initialize(const std::vector<Objective>& mapObjectives) {
  objectives = mapObjectives;
  playerInteractions.clear();

  Logger::info("ObjectiveSystem initialized with " +
               std::to_string(objectives.size()) + " objectives");
}

void ObjectiveSystem::update(float deltaTime) {
  // Update interaction progress for AlienScrapyard objectives
  for (auto& [playerId, objectiveId] : playerInteractions) {
    Objective* obj = getObjective(objectiveId);
    if (!obj || obj->state != ObjectiveState::InProgress) {
      continue;
    }

    if (obj->type == ObjectiveType::AlienScrapyard) {
      // Progress the interaction timer
      float progressIncrement = deltaTime / obj->interactionTime;
      obj->interactionProgress += progressIncrement;

      // Notify progress update
      if (progressCallback) {
        progressCallback(obj->id, obj->getProgress());
      }

      // Check for completion
      if (obj->interactionProgress >= 1.0f) {
        obj->interactionProgress = 1.0f;
        completeObjective(*obj);
      }
    }
  }
}

bool ObjectiveSystem::tryInteract(uint32_t playerId, float playerX,
                                  float playerY) {
  // Check if player is already interacting
  if (playerInteractions.count(playerId) > 0) {
    return false;  // Already interacting
  }

  // Find nearest objective in range
  Objective* nearestObj = nullptr;
  float nearestDistSq = std::numeric_limits<float>::max();

  for (auto& obj : objectives) {
    if (obj.state != ObjectiveState::Inactive) {
      continue;  // Skip non-inactive objectives
    }

    float dx = playerX - obj.x;
    float dy = playerY - obj.y;
    float distSq = dx * dx + dy * dy;
    float radiusSq = obj.radius * obj.radius;

    if (distSq <= radiusSq && distSq < nearestDistSq) {
      nearestObj = &obj;
      nearestDistSq = distSq;
    }
  }

  if (nearestObj) {
    startObjective(*nearestObj, playerId);
    return true;
  }

  return false;
}

void ObjectiveSystem::stopInteraction(uint32_t playerId) {
  auto it = playerInteractions.find(playerId);
  if (it == playerInteractions.end()) {
    return;  // Not interacting
  }

  uint32_t objectiveId = it->second;
  playerInteractions.erase(it);

  Objective* obj = getObjective(objectiveId);
  if (obj && obj->state == ObjectiveState::InProgress) {
    // For AlienScrapyard, reset progress if player stops interacting
    if (obj->type == ObjectiveType::AlienScrapyard) {
      obj->state = ObjectiveState::Inactive;
      obj->interactionProgress = 0.0f;
      obj->interactingPlayerId = 0;

      if (stateCallback) {
        stateCallback(obj->id, obj->state);
      }

      Logger::info("Objective '" + obj->name +
                   "' interaction cancelled by player " +
                   std::to_string(playerId));
    }
    // For CaptureOutpost, don't reset - it continues until enemies are dead
  }
}

void ObjectiveSystem::onEnemyDeath(float enemyX, float enemyY) {
  // Check all CaptureOutpost objectives
  for (auto& obj : objectives) {
    if (obj.type != ObjectiveType::CaptureOutpost) {
      continue;
    }

    if (obj.state != ObjectiveState::InProgress) {
      continue;
    }

    // Check if enemy died within objective radius
    if (obj.isInRange(enemyX, enemyY)) {
      obj.enemiesKilled++;

      Logger::info("Enemy killed in outpost '" + obj.name +
                   "': " + std::to_string(obj.enemiesKilled) + "/" +
                   std::to_string(obj.enemiesRequired));

      // Notify progress update
      if (progressCallback) {
        progressCallback(obj.id, obj.getProgress());
      }

      // Check for completion
      if (obj.enemiesKilled >= obj.enemiesRequired) {
        completeObjective(obj);
      }
    }
  }
}

Objective* ObjectiveSystem::getObjective(uint32_t objectiveId) {
  for (auto& obj : objectives) {
    if (obj.id == objectiveId) {
      return &obj;
    }
  }
  return nullptr;
}

const Objective* ObjectiveSystem::getObjective(uint32_t objectiveId) const {
  for (const auto& obj : objectives) {
    if (obj.id == objectiveId) {
      return &obj;
    }
  }
  return nullptr;
}

Objective* ObjectiveSystem::findObjectiveNear(float x, float y) {
  for (auto& obj : objectives) {
    if (obj.isInRange(x, y)) {
      return &obj;
    }
  }
  return nullptr;
}

void ObjectiveSystem::startObjective(Objective& objective, uint32_t playerId) {
  objective.state = ObjectiveState::InProgress;
  objective.interactingPlayerId = playerId;
  playerInteractions[playerId] = objective.id;

  Logger::info("Objective '" + objective.name + "' started by player " +
               std::to_string(playerId));

  if (stateCallback) {
    stateCallback(objective.id, objective.state);
  }
}

void ObjectiveSystem::completeObjective(Objective& objective) {
  objective.state = ObjectiveState::Completed;

  // Remove from player interactions
  if (objective.interactingPlayerId != 0) {
    playerInteractions.erase(objective.interactingPlayerId);
    objective.interactingPlayerId = 0;
  }

  Logger::info("Objective '" + objective.name + "' COMPLETED!");

  if (stateCallback) {
    stateCallback(objective.id, objective.state);
  }
}
