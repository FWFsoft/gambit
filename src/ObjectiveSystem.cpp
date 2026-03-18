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
  // Collect deferred actions to avoid modifying playerInteractions during
  // iteration (erasing during range-for is undefined behavior)
  std::vector<uint32_t> playersToRemove;
  std::vector<Objective*> objectivesToComplete;

  // Update interaction progress for AlienScrapyard objectives
  for (auto& [playerId, objectiveId] : playerInteractions) {
    Objective* obj = getObjective(objectiveId);
    if (!obj || obj->state != ObjectiveState::InProgress) {
      continue;
    }

    if (obj->type == ObjectiveType::AlienScrapyard ||
        obj->type == ObjectiveType::RecoverProbe ||
        obj->type == ObjectiveType::SaveTheFrogs ||
        obj->type == ObjectiveType::NoxiousGas) {
      // Progress the interaction timer (deltaTime is ms, interactionTime is
      // seconds)
      float progressIncrement =
          deltaTime / (obj->interactionTime * 1000.0f);
      obj->interactionProgress += progressIncrement;

      // Notify progress update
      if (progressCallback) {
        progressCallback(obj->id, obj->getProgress());
      }

      // Check for completion of current pickup
      if (obj->interactionProgress >= 1.0f) {
        obj->interactionProgress = 1.0f;

        if (obj->hasDepositPoint) {
          // Two-phase: transition to ReadyToDeposit instead of completing
          obj->state = ObjectiveState::ReadyToDeposit;
          playersToRemove.push_back(playerId);

          Logger::info("Objective '" + obj->name +
                       "' item collected - carry to deposit point");

          if (stateCallback) {
            stateCallback(obj->id, obj->state);
          }
        } else {
          objectivesToComplete.push_back(obj);
        }
      }
    }
  }

  // Apply deferred mutations after iteration
  for (uint32_t pid : playersToRemove) {
    playerInteractions.erase(pid);
  }
  for (Objective* obj : objectivesToComplete) {
    completeObjective(*obj);
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

    if ((obj.type == ObjectiveType::AlienScrapyard ||
         obj.type == ObjectiveType::RecoverProbe) &&
        obj.state == ObjectiveState::ReadyToDeposit &&
        obj.isInDepositRange(playerX, playerY)) {
      canInteract = true;  // Ready to deposit item at deposit point
    }

    // NoxiousGas: allow interact when Inactive or InProgress (re-start timer)
    if (obj.type == ObjectiveType::NoxiousGas &&
        (obj.state == ObjectiveState::Inactive ||
         obj.state == ObjectiveState::InProgress) &&
        obj.interactingPlayerId == 0 && obj.isInRange(playerX, playerY)) {
      canInteract = true;
    }

    // LittleJohn: allow gate interact when InProgress (guardian activated)
    if (obj.type == ObjectiveType::LittleJohn &&
        obj.state == ObjectiveState::InProgress &&
        obj.isInRange(playerX, playerY)) {
      canInteract = true;
    }

    // SaveTheFrogs: allow re-pickup when InProgress (more frogs needed) and no
    // one is currently picking up a frog, OR deposit when ReadyToDeposit
    if (obj.type == ObjectiveType::SaveTheFrogs) {
      if (obj.state == ObjectiveState::InProgress &&
          obj.frogsDeposited < obj.frogsRequired &&
          obj.interactingPlayerId == 0 &&
          obj.isInRange(playerX, playerY)) {
        canInteract = true;  // Pick up next frog
      }
      if (obj.state == ObjectiveState::ReadyToDeposit &&
          obj.isInDepositRange(playerX, playerY)) {
        canInteract = true;  // Deposit frog at pond
      }
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
    // SalvageMedpacks: complete when enemies cleared and player interacts
    if (nearestObj->type == ObjectiveType::SalvageMedpacks &&
        nearestObj->state == ObjectiveState::InProgress &&
        nearestObj->enemiesKilled >= nearestObj->enemiesRequired) {
      nearestObj->interactingPlayerId = playerId;
      completeObjective(*nearestObj);
    } else if ((nearestObj->type == ObjectiveType::AlienScrapyard ||
                nearestObj->type == ObjectiveType::RecoverProbe) &&
               nearestObj->state == ObjectiveState::ReadyToDeposit) {
      // Depositing item at deposit point — complete the objective
      nearestObj->interactingPlayerId = playerId;
      completeObjective(*nearestObj);
    } else if (nearestObj->type == ObjectiveType::SaveTheFrogs &&
               nearestObj->state == ObjectiveState::ReadyToDeposit) {
      // Depositing a frog at the pond
      nearestObj->interactingPlayerId = playerId;
      nearestObj->frogsDeposited++;
      nearestObj->interactionProgress = 0.0f;

      Logger::info("Frog deposited for '" + nearestObj->name + "': " +
                   std::to_string(nearestObj->frogsDeposited) + "/" +
                   std::to_string(nearestObj->frogsRequired));

      if (nearestObj->frogsDeposited >= nearestObj->frogsRequired) {
        completeObjective(*nearestObj);
      } else {
        // More frogs needed — go back to InProgress for next pickup
        nearestObj->state = ObjectiveState::InProgress;
        nearestObj->interactingPlayerId = 0;

        if (progressCallback) {
          progressCallback(nearestObj->id, nearestObj->getProgress());
        }
        if (stateCallback) {
          stateCallback(nearestObj->id, nearestObj->state);
        }
      }
    } else if (nearestObj->type == ObjectiveType::LittleJohn &&
               nearestObj->state == ObjectiveState::InProgress) {
      // Interact with gate — complete immediately (no timer)
      nearestObj->interactingPlayerId = playerId;
      completeObjective(*nearestObj);
    } else if (nearestObj->type == ObjectiveType::NoxiousGas &&
               nearestObj->state == ObjectiveState::InProgress) {
      // Resume interaction (player re-entered zone)
      nearestObj->interactionProgress = 0.0f;
      startObjective(*nearestObj, playerId);
    } else {
      // Start the objective (or re-start pick-up phase for SaveTheFrogs)
      if (nearestObj->type == ObjectiveType::SaveTheFrogs) {
        nearestObj->interactionProgress = 0.0f;
      }
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
    if (obj->type == ObjectiveType::AlienScrapyard ||
        obj->type == ObjectiveType::RecoverProbe ||
        obj->type == ObjectiveType::NoxiousGas) {
      // Reset entirely - player must start over
      obj->state = ObjectiveState::Inactive;
      obj->interactionProgress = 0.0f;
      obj->interactingPlayerId = 0;

      if (stateCallback) {
        stateCallback(obj->id, obj->state);
      }

      Logger::info("Objective '" + obj->name +
                   "' interaction cancelled by player " +
                   std::to_string(playerId));
    } else if (obj->type == ObjectiveType::SaveTheFrogs) {
      // Cancel current frog pickup but keep deposited frogs
      obj->interactionProgress = 0.0f;
      obj->interactingPlayerId = 0;
      // Stay InProgress so player can pick up next frog

      Logger::info("Objective '" + obj->name +
                   "' frog pickup cancelled by player " +
                   std::to_string(playerId) + " (" +
                   std::to_string(obj->frogsDeposited) + "/" +
                   std::to_string(obj->frogsRequired) + " deposited)");
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
