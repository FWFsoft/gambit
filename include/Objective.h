#pragma once

#include <cstdint>
#include <string>

// Types of objectives available in the game
enum class ObjectiveType : uint8_t {
  AlienScrapyard,  // Interact to collect scrap, timer-based completion
  CaptureOutpost   // Kill all enemies in zone to complete
};

// Current state of an objective
enum class ObjectiveState : uint8_t {
  Inactive,    // Not yet started, available to interact
  InProgress,  // Player has initiated, working toward completion
  Completed    // Finished successfully
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

  // Player currently interacting (0 = none)
  uint32_t interactingPlayerId = 0;

  // Helper to check if a point is within the objective's radius
  bool isInRange(float px, float py) const {
    float dx = px - x;
    float dy = py - y;
    return (dx * dx + dy * dy) <= (radius * radius);
  }

  // Get progress as a percentage (0.0 - 1.0)
  float getProgress() const {
    switch (type) {
      case ObjectiveType::AlienScrapyard:
        return interactionProgress;
      case ObjectiveType::CaptureOutpost:
        if (enemiesRequired == 0) return 1.0f;
        return static_cast<float>(enemiesKilled) /
               static_cast<float>(enemiesRequired);
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
  } else if (str == "capture_outpost" || str == "CaptureOutpost") {
    return ObjectiveType::CaptureOutpost;
  }
  // Default to AlienScrapyard if unknown
  return ObjectiveType::AlienScrapyard;
}
