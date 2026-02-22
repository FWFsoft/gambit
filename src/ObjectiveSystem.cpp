#include "ObjectiveSystem.h"

#include <limits>

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

        if (obj->hasDepositPoint) {
          // Two-phase: transition to ReadyToDeposit instead of completing
          obj->state = ObjectiveState::ReadyToDeposit;
          playerInteractions.erase(playerId);

          Logger::info("Objective '" + obj->name +
                       "' scrap collected - carry to deposit point");

          if (stateCallback) {
            stateCallback(obj->id, obj->state);
          }
        } else {
          completeObjective(*obj);
        }
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
    // Can interact with Inactive objectives (to start them)
    // OR SalvageMedpacks that are InProgress with all enemies cleared
    // OR AlienScrapyard that is ReadyToDeposit at the deposit point
    bool canInteract = (obj.state == ObjectiveState::Inactive);

    if (obj.type == ObjectiveType::SalvageMedpacks &&
        obj.state == ObjectiveState::InProgress &&
        obj.enemiesKilled >= obj.enemiesRequired) {
      canInteract = true;  // Pod ready to collect
    }

    if (obj.type == ObjectiveType::AlienScrapyard &&
        obj.state == ObjectiveState::ReadyToDeposit &&
        obj.isInDepositRange(playerX, playerY)) {
      canInteract = true;  // Ready to deposit scrap at deposit point
    }

    if (!canInteract) {
      continue;
    }

    // For ReadyToDeposit, check deposit range instead of objective range
    float dx, dy;
    if (obj.state == ObjectiveState::ReadyToDeposit && obj.hasDepositPoint) {
      dx = playerX - obj.depositX;
      dy = playerY - obj.depositY;
    } else {
      dx = playerX - obj.x;
      dy = playerY - obj.y;
    }
    float distSq = dx * dx + dy * dy;
    float radiusSq = obj.radius * obj.radius;

    if (distSq <= radiusSq && distSq < nearestDistSq) {
      nearestObj = &obj;
      nearestDistSq = distSq;
    }
  }

  if (nearestObj) {
    // If SalvageMedpacks with enemies cleared, complete it
    if (nearestObj->type == ObjectiveType::SalvageMedpacks &&
        nearestObj->state == ObjectiveState::InProgress &&
        nearestObj->enemiesKilled >= nearestObj->enemiesRequired) {
      nearestObj->interactingPlayerId = playerId;
      completeObjective(*nearestObj);
    } else if (nearestObj->type == ObjectiveType::AlienScrapyard &&
               nearestObj->state == ObjectiveState::ReadyToDeposit) {
      // Depositing scrap at deposit point — complete the objective
      nearestObj->interactingPlayerId = playerId;
      completeObjective(*nearestObj);
    } else {
      // Otherwise start the objective
      startObjective(*nearestObj, playerId);
    }
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

  // Notify stop callback (for removing Slow debuff, etc.)
  if (stopCallback) {
    stopCallback(objectiveId, playerId);
  }
}

void ObjectiveSystem::onEnemyDeath(float enemyX, float enemyY) {
  // Check CaptureOutpost and SalvageMedpacks objectives
  for (auto& obj : objectives) {
    if (obj.type != ObjectiveType::CaptureOutpost &&
        obj.type != ObjectiveType::SalvageMedpacks) {
      continue;
    }

    if (obj.state != ObjectiveState::InProgress) {
      continue;
    }

    // Check if enemy died within objective radius
    if (obj.isInRange(enemyX, enemyY)) {
      obj.enemiesKilled++;

      Logger::info("Enemy killed in " + objectiveTypeToString(obj.type) + " '" +
                   obj.name + "': " + std::to_string(obj.enemiesKilled) + "/" +
                   std::to_string(obj.enemiesRequired));

      // Notify progress update
      if (progressCallback) {
        progressCallback(obj.id, obj.getProgress());
      }

      // Check for completion
      if (obj.enemiesKilled >= obj.enemiesRequired) {
        // CaptureOutpost auto-completes when all enemies dead
        if (obj.type == ObjectiveType::CaptureOutpost) {
          completeObjective(obj);
        }
        // SalvageMedpacks requires pod interaction after enemies are dead
        else if (obj.type == ObjectiveType::SalvageMedpacks) {
          Logger::info("All enemies cleared for '" + obj.name +
                       "' - pod ready for interaction");
        }
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
  uint32_t completingPlayerId = objective.interactingPlayerId;

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

  // Notify completion callback (for rewards, debuff removal, etc.)
  if (completionCallback && completingPlayerId != 0) {
    completionCallback(objective.id, completingPlayerId);
  }
}
