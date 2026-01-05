#include "HeadlessRenderSystem.h"

#include "Logger.h"

HeadlessRenderSystem::HeadlessRenderSystem() {
  EventBus::instance().subscribe<RenderEvent>(
      [this](const RenderEvent& e) { onRender(e); });

  Logger::info("HeadlessRenderSystem initialized");
}

void HeadlessRenderSystem::onRender(const RenderEvent& e) {
  // No-op: No actual rendering in headless mode
  // This just maintains event compatibility
}
