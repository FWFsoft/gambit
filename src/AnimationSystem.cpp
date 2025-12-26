#include "AnimationSystem.h"

#include <algorithm>

#include "AnimationController.h"
#include "Logger.h"

AnimationSystem::AnimationSystem() {
  // Subscribe to UpdateEvent to advance animations every frame
  EventBus::instance().subscribe<UpdateEvent>(
      [this](const UpdateEvent& e) { onUpdate(e); });

  Logger::info("AnimationSystem initialized");
}

void AnimationSystem::registerEntity(Animatable* entity) {
  assert(entity != nullptr && "Cannot register null entity");
  assert(entity->getAnimationController() != nullptr &&
         "Entity must have AnimationController");

  entities.push_back(entity);
  Logger::debug("Registered entity with AnimationSystem (total: " +
                std::to_string(entities.size()) + ")");
}

void AnimationSystem::unregisterEntity(Animatable* entity) {
  auto it = std::find(entities.begin(), entities.end(), entity);
  if (it != entities.end()) {
    entities.erase(it);
    Logger::debug("Unregistered entity from AnimationSystem (total: " +
                  std::to_string(entities.size()) + ")");
  }
}

void AnimationSystem::onUpdate(const UpdateEvent& e) {
  // Advance all animations by deltaTime
  for (Animatable* entity : entities) {
    AnimationController* controller = entity->getAnimationController();
    if (controller) {
      controller->advanceFrame(e.deltaTime);
    }
  }
}
