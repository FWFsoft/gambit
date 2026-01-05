#pragma once

#include <string>
#include <vector>

#include "EventBus.h"

// Forward declaration
class InputScript;

// Mock Window class for headless client testing
// Provides the same interface as Window but without SDL/OpenGL dependencies
class MockWindow {
 public:
  MockWindow(const std::string& title, int width, int height);
  ~MockWindow();

  void pollEvents();
  bool isOpen() const;
  void close();

  // Mock SDL window accessor (returns nullptr)
  void* getWindow() const;

  // No-op ImGui initialization for headless mode
  void initImGui();
  void shutdownImGui();

  // Headless-specific: Set input script for automated input injection
  void setInputScript(InputScript* script);

  // Headless-specific: Get current frame number for input scripting
  void setFrameNumber(uint64_t frame);

 private:
  std::string title;
  int width;
  int height;
  bool openFlag;
  InputScript* inputScript;
  uint64_t currentFrame;
};
