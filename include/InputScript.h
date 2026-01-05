#pragma once

#include <SDL2/SDL.h>

#include <map>
#include <string>
#include <vector>

// Input action to be executed at a specific frame
struct InputAction {
  uint64_t frame;     // Frame number when this action should execute
  int key;            // SDL key code
  bool isKeyDown;     // true for key down, false for key up
  uint64_t duration;  // How many frames to hold (0 = instant press/release)
};

// Scripted input system for headless testing
// Allows programmatic or file-based input injection
class InputScript {
 public:
  InputScript();
  ~InputScript();

  // Add a key press action (press and release)
  void addKeyPress(uint64_t frame, int key, uint64_t duration = 1);

  // Add a key down action
  void addKeyDown(uint64_t frame, int key);

  // Add a key up action
  void addKeyUp(uint64_t frame, int key);

  // Add a movement action (uses WASD keys)
  void addMove(uint64_t startFrame, uint64_t duration, bool left, bool right,
               bool up, bool down);

  // Process inputs for the current frame (publishes events)
  void processFrame(uint64_t currentFrame);

  // Load script from JSON file (optional, for future)
  // bool loadFromFile(const std::string& filename);

  // Clear all actions
  void clear();

 private:
  std::vector<InputAction> actions;
  size_t nextActionIndex;  // Index of next action to process
};
