#include "HeadlessUISystem.h"

#include "Logger.h"

HeadlessUISystem::HeadlessUISystem() {
  EventBus::instance().subscribe<RenderEvent>(
      [this](const RenderEvent& e) { onRender(e); });

  EventBus::instance().subscribe<ItemPickedUpEvent>(
      [this](const ItemPickedUpEvent& e) { onItemPickedUp(e); });

  Logger::info("HeadlessUISystem initialized");
}

void HeadlessUISystem::render() {
  // No-op: No actual UI rendering in headless mode
}

void HeadlessUISystem::onRender(const RenderEvent& e) {
  // No-op: No actual UI rendering in headless mode
  // This just maintains event compatibility
}

void HeadlessUISystem::onItemPickedUp(const ItemPickedUpEvent& e) {
  // No-op: Track pickups but don't show UI notifications
  Logger::debug("Headless: Item picked up - ID: " + std::to_string(e.itemId) +
                ", Quantity: " + std::to_string(e.quantity));
}
