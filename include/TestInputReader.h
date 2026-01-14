#pragma once

#include <SDL2/SDL.h>

#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

// Simple test input reader for visual testing
// Reads commands from a text file and simulates keyboard/mouse input
class TestInputReader {
 public:
  enum class CommandType {
    WAIT,         // Wait N frames
    KEY_DOWN,     // Press key down
    KEY_UP,       // Release key
    SCREENSHOT,   // Capture screenshot
    MOUSE_MOVE,   // Move mouse to (x, y)
    MOUSE_CLICK,  // Click mouse button at current position
  };

  struct Command {
    CommandType type;
    std::string stringArg;  // For KEY_DOWN/KEY_UP: key name, for SCREENSHOT:
                            // filename, for MOUSE_CLICK: button name
    int intArg;             // For WAIT: frame count, MOUSE_MOVE: x coordinate
    int intArg2;            // For MOUSE_MOVE: y coordinate
  };

  explicit TestInputReader(const std::string& commandFilePath);

  // Read next command from file (returns empty if none available)
  std::optional<Command> readNextCommand();

  // Check if we're currently waiting
  bool isWaiting() const { return waitFramesRemaining > 0; }

  // Decrement wait counter (call once per frame)
  void tick();

  // Convert key name string to SDL scancode
  static SDL_Scancode stringToScancode(const std::string& keyName);

 private:
  std::string filePath;
  std::ifstream file;
  int waitFramesRemaining = 0;
  int lineNumber = 0;

  // Reopen file to check for new commands (allows live editing)
  void reopenFile();

  // Parse a command line
  std::optional<Command> parseLine(const std::string& line);

  // Key name to SDL scancode mapping
  static const std::unordered_map<std::string, SDL_Scancode> keyMap;
};
