#pragma once

#include "EventBus.h"

// Headless UI system for testing
// Subscribes to RenderEvent and game events but performs no actual UI rendering
class HeadlessUISystem {
 public:
  HeadlessUISystem();
  ~HeadlessUISystem() = default;

  // No-op render method for compatibility
  void render();

 private:
  void onRender(const RenderEvent& e);
  void onItemPickedUp(const ItemPickedUpEvent& e);
};
