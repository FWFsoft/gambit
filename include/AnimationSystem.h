#pragma once

#include <algorithm>
#include <vector>

#include "Animatable.h"
#include "EventBus.h"

// System that updates all animations in the game
// Subscribes to UpdateEvent and advances all animation timers
class AnimationSystem {
 public:
  AnimationSystem();

  // Register an entity with the animation system
  void registerEntity(Animatable* entity);

  // Unregister an entity (e.g., when player disconnects)
  void unregisterEntity(Animatable* entity);

 private:
  std::vector<Animatable*> entities;

  void onUpdate(const UpdateEvent& e);
};
