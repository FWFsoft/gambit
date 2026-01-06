#pragma once

#include <string>

// Client-side state for map selection
// Stores the selected map path for use when starting the game
class MapSelectionState {
 public:
  static MapSelectionState& instance();

  std::string getSelectedMapPath() const { return selectedMapPath; }
  void setSelectedMap(const std::string& path) { selectedMapPath = path; }
  bool hasSelection() const { return !selectedMapPath.empty(); }
  void reset() { selectedMapPath = ""; }

 private:
  MapSelectionState() : selectedMapPath("assets/maps/test_map.tmx") {}
  std::string selectedMapPath;  // Default to test_map.tmx
};
