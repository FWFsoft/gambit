#pragma once

#include <cstdint>

// Client-side state for character selection
// Stored separately from Player to avoid polluting shared player data
class CharacterSelectionState {
 public:
  static CharacterSelectionState& instance();

  uint32_t getSelectedCharacterId() const { return selectedCharacterId; }
  void setSelectedCharacter(uint32_t id) { selectedCharacterId = id; }
  bool hasSelection() const { return selectedCharacterId != 0; }
  void reset() { selectedCharacterId = 0; }

 private:
  CharacterSelectionState() : selectedCharacterId(0) {}
  uint32_t selectedCharacterId;  // 0 = no selection
};
