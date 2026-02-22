#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Objective.h"

// Forward declarations
struct EnemyDeathEvent;

// Callback type for when objective state changes
using ObjectiveStateCallback =
    std::function<void(uint32_t objectiveId, ObjectiveState newState)>;

// Callback type for objective progress updates
using ObjectiveProgressCallback =
    std::function<void(uint32_t objectiveId, float progress)>;

// Callback type for objective completion (includes player who completed it)
using ObjectiveCompletionCallback =
    std::function<void(uint32_t objectiveId, uint32_t playerId)>;

// Callback type for when a player stops interacting
using ObjectiveStopCallback =
    std::function<void(uint32_t objectiveId, uint32_t playerId)>;

/**
 * ObjectiveSystem - Server-side objective tracking and management
 *
 * Responsibilities:
 * - Track all objectives loaded from the map
 * - Handle player interactions with objectives
 * - Track progress (timers for scrapyard, kills for outpost)
 * - Broadcast state changes via callbacks
 */
class ObjectiveSystem {
 public:
  ObjectiveSystem();
  ~ObjectiveSystem();

  // Initialize with objectives from the map
  void initialize(const std::vector<Objective>& mapObjectives);

  // Update objectives each frame (handles interaction timers)
  void update(float deltaTime);

  // Player attempts to interact with nearby objectives
  // Returns true if an interaction was started
  bool tryInteract(uint32_t playerId, float playerX, float playerY);

  // Player stops interacting (moved away or cancelled)
  void stopInteraction(uint32_t playerId);

  // Called when an enemy dies - checks for CaptureOutpost progress
  void onEnemyDeath(float enemyX, float enemyY);

  // Get all objectives (for network sync)
  const std::vector<Objective>& getObjectives() const { return objectives; }

  // Get a specific objective by ID
  Objective* getObjective(uint32_t objectiveId);
  const Objective* getObjective(uint32_t objectiveId) const;

  // Find objective near a position (within radius)
  Objective* findObjectiveNear(float x, float y);

  // Get player interactions (player ID -> objective ID)
  const std::unordered_map<uint32_t, uint32_t>& getPlayerInteractions() const {
    return playerInteractions;
  }

  // Set callbacks for state changes
  void setStateCallback(ObjectiveStateCallback callback) {
    stateCallback = std::move(callback);
  }

  void setProgressCallback(ObjectiveProgressCallback callback) {
    progressCallback = std::move(callback);
  }

  void setCompletionCallback(ObjectiveCompletionCallback callback) {
    completionCallback = std::move(callback);
  }

  void setStopCallback(ObjectiveStopCallback callback) {
    stopCallback = std::move(callback);
  }

 private:
  std::vector<Objective> objectives;

  // Map from player ID to objective ID they're interacting with
  std::unordered_map<uint32_t, uint32_t> playerInteractions;

  // Callbacks
  ObjectiveStateCallback stateCallback;
  ObjectiveProgressCallback progressCallback;
  ObjectiveCompletionCallback completionCallback;
  ObjectiveStopCallback stopCallback;

  // Helper to complete an objective
  void completeObjective(Objective& objective);

  // Helper to start an objective
  void startObjective(Objective& objective, uint32_t playerId);
};
