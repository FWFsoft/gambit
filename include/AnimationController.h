#pragma once

#include <cassert>
#include <string>
#include <unordered_map>

#include "AnimationClip.h"
#include "AnimationDirection.h"

class AnimationController {
 public:
  AnimationController();

  // Register an animation clip (e.g., "idle", "walk_north", "walk_east")
  void addAnimation(const std::string& name, const AnimationClip& clip);

  // Get animation by name (Tiger Style: asserts if not found)
  const AnimationClip& getAnimation(const std::string& name) const;

  // Update animation state based on velocity
  // Called every frame to determine direction and switch animations
  void updateAnimationState(float vx, float vy);

  // Advance animation timer and frame counter
  // deltaTime in milliseconds (typically 16.67ms at 60 FPS)
  void advanceFrame(float deltaTime);

  // Get current frame's sprite sheet region
  void getCurrentFrame(int& outSrcX, int& outSrcY, int& outSrcW,
                       int& outSrcH) const;

  // Reset animation to first frame (useful for state transitions)
  void reset();

  // Getters
  const std::string& getCurrentAnimationName() const {
    return currentAnimationName;
  }
  int getCurrentFrameIndex() const { return currentFrameIndex; }
  AnimationDirection getCurrentDirection() const { return currentDirection; }

 private:
  std::unordered_map<std::string, AnimationClip> animations;
  std::string currentAnimationName;
  int currentFrameIndex;
  float frameTimer;  // Milliseconds elapsed in current frame
  AnimationDirection currentDirection;

  // Internal helper to switch animation
  void playAnimation(const std::string& name);
};
