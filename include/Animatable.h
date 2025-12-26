#pragma once

class AnimationController;

// Interface for entities that can be animated
// This allows AnimationSystem to operate on any entity type
class Animatable {
 public:
  virtual ~Animatable() = default;

  // Get the animation controller for this entity
  virtual AnimationController* getAnimationController() = 0;
  virtual const AnimationController* getAnimationController() const = 0;

  // Get current velocity (used to determine animation direction)
  virtual float getVelocityX() const = 0;
  virtual float getVelocityY() const = 0;
};
