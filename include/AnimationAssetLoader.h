#pragma once

#include <string>

#include "AnimationController.h"

// Loads animation data into AnimationController
// Currently creates animations programmatically based on hardcoded sprite sheet
// layout Future version will load from JSON metadata (PixelLab.ai integration)
class AnimationAssetLoader {
 public:
  // Load all player animations (idle + 8-directional walk)
  // spriteSheetPath: Path to the sprite sheet texture (for future metadata
  // loading)
  static void loadPlayerAnimations(AnimationController& controller,
                                   const std::string& spriteSheetPath);

 private:
  // Create idle animation (1 frame)
  static AnimationClip createIdleAnimation();

  // Create walk animation for a specific direction (4 frames, looping)
  static AnimationClip createWalkAnimation(AnimationDirection dir);
};
