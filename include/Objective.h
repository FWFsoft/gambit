#pragma once

#include <cstdint>
#include <string>

// Types of objectives available in the game
enum class ObjectiveType : uint8_t {
  AlienScrapyard,  // Interact to collect scrap, timer-based completion
  CaptureOutpost,  // Kill all enemies in zone to complete
  SalvageMedpacks,  // Kill enemies, then interact with pod for healing
  RecoverProbe,    // Dig out probe (interact timer), carry to ship deposit
  SaveTheFrogs,    // Collect 3 frogs (repeating interact+deposit cycle)
  NoxiousGas,      // Kick spores in a damaging gas cloud (interact timer)
  LittleJohn       // Gate guarded by passive enemy that activates on approach
};

// Current state of an objective
enum class ObjectiveState : uint8_t {
  Inactive,        // Not yet started, available to interact
  InProgress,      // Player has initiated, working toward completion
  ReadyToDeposit,  // AlienScrapyard: scrap collected, carry to deposit point
  Completed        // Finished successfully
};

// Objective data structure loaded from TMX and tracked by server
struct Objective {
  uint32_t id = 0;  // Unique identifier
  ObjectiveType type = ObjectiveType::AlienScrapyard;
  ObjectiveState state = ObjectiveState::Inactive;
  std::string name;          // Display name
  std::string description;   // UI description
  float x = 0.0f, y = 0.0f;  // World position
  float radius = 50.0f;      // Interaction/zone radius

  // For CaptureOutpost: track enemies in zone
  int enemiesRequired = 0;  // Total enemies to kill
  int enemiesKilled = 0;    // Current kill count

  // For AlienScrapyard: interaction timer
  float interactionTime = 3.0f;      // Seconds to complete
  float interactionProgress = 0.0f;  // Current progress (0.0 - 1.0)

  // For AlienScrapyard/RecoverProbe: deposit point (carry item here to complete)
  float depositX = 0.0f;
  float depositY = 0.0f;
  bool hasDepositPoint = false;  // True if deposit coords are set

  // For SaveTheFrogs: multi-deposit tracking (reuses enemiesRequired/Killed in
  // packet)
  int frogsRequired = 3;   // Total frogs to deposit (default 3)
  int frogsDeposited = 0;  // How many frogs delivered so far

  // For NoxiousGas: throttle damage ticks
  float gasDamageTimer = 0.0f;  // Accumulates ms; damage fires every 2000ms

  // For LittleJohn: guardian enemy tracking
  uint32_t guardianEnemyId = 0;  // ID of the passive guardian (0 = none/dead)

  // Player currently interacting (0 = none)
  uint32_t interactingPlayerId = 0;

  // Helper to check if a point is within the objective's radius
  bool isInRange(float px, float py) const {
    float dx = px - x;
    float dy = py - y;
    return (dx * dx + dy * dy) <= (radius * radius);
  }

  // Helper to check if a point is within deposit range
  bool isInDepositRange(float px, float py) const {
    if (!hasDepositPoint) return false;
    float dx = px - depositX;
    float dy = py - depositY;
    return (dx * dx + dy * dy) <= (radius * radius);
  }

  // Get progress as a percentage (0.0 - 1.0)
  float getProgress() const {
    switch (type) {
      case ObjectiveType::AlienScrapyard:
      case ObjectiveType::RecoverProbe:
        return interactionProgress;
      case ObjectiveType::CaptureOutpost:
      case ObjectiveType::SalvageMedpacks:
        if (enemiesRequired == 0) return 1.0f;
        return static_cast<float>(enemiesKilled) /
               static_cast<float>(enemiesRequired);
      case ObjectiveType::SaveTheFrogs:
        if (frogsRequired == 0) return 1.0f;
        return static_cast<float>(frogsDeposited) /
               static_cast<float>(frogsRequired);
      case ObjectiveType::NoxiousGas:
        return interactionProgress;
      case ObjectiveType::LittleJohn:
        return (state == ObjectiveState::Completed) ? 1.0f : 0.0f;
      default:
        return 0.0f;
    }
  }
};

// Convert ObjectiveType to string for logging/display
inline std::string objectiveTypeToString(ObjectiveType type) {
  switch (type) {
    case ObjectiveType::AlienScrapyard:
      return "AlienScrapyard";
    case ObjectiveType::CaptureOutpost:
      return "CaptureOutpost";
    case ObjectiveType::SalvageMedpacks:
      return "SalvageMedpacks";
    case ObjectiveType::RecoverProbe:
      return "RecoverProbe";
    case ObjectiveType::SaveTheFrogs:
      return "SaveTheFrogs";
    case ObjectiveType::NoxiousGas:
      return "NoxiousGas";
    case ObjectiveType::LittleJohn:
      return "LittleJohn";
    default:
      return "Unknown";
  }
}

// Convert ObjectiveState to string for logging/display
inline std::string objectiveStateToString(ObjectiveState state) {
  switch (state) {
    case ObjectiveState::Inactive:
      return "Inactive";
    case ObjectiveState::InProgress:
      return "InProgress";
    case ObjectiveState::ReadyToDeposit:
      return "ReadyToDeposit";
    case ObjectiveState::Completed:
      return "Completed";
    default:
      return "Unknown";
  }
}

// Parse ObjectiveType from string (for TMX loading)
inline ObjectiveType parseObjectiveType(const std::string& str) {
  if (str == "alien_scrapyard" || str == "AlienScrapyard") {
    return ObjectiveType::AlienScrapyard;
  }
  if (str == "capture_outpost" || str == "CaptureOutpost") {
    return ObjectiveType::CaptureOutpost;
  }
  if (str == "salvage_medpacks" || str == "SalvageMedpacks") {
    return ObjectiveType::SalvageMedpacks;
  }
  if (str == "recover_probe" || str == "RecoverProbe") {
    return ObjectiveType::RecoverProbe;
  }
  if (str == "save_the_frogs" || str == "SaveTheFrogs") {
    return ObjectiveType::SaveTheFrogs;
  }
  if (str == "noxious_gas" || str == "NoxiousGas") {
    return ObjectiveType::NoxiousGas;
  }
  if (str == "little_john" || str == "LittleJohn") {
    return ObjectiveType::LittleJohn;
  }
  // Default to AlienScrapyard if unknown
  return ObjectiveType::AlienScrapyard;
}
