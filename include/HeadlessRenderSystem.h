#pragma once

#include "EventBus.h"

// Headless rendering system for testing
// Subscribes to RenderEvent but performs no actual rendering
class HeadlessRenderSystem {
 public:
  HeadlessRenderSystem();
  ~HeadlessRenderSystem() = default;

  // No-op sprite renderer access (returns nullptr)
  void* getSpriteRenderer() { return nullptr; }

 private:
  void onRender(const RenderEvent& e);
};
