#include "AnimationController.h"

#include "Logger.h"

AnimationController::AnimationController()
    : currentAnimationName("idle"),
      currentFrameIndex(0),
      frameTimer(0.0f),
      currentDirection(AnimationDirection::IDLE) {}

void AnimationController::addAnimation(const std::string& name,
                                       const AnimationClip& clip) {
  animations[name] = clip;
}

const AnimationClip& AnimationController::getAnimation(
    const std::string& name) const {
  auto it = animations.find(name);
  assert(it != animations.end() && "Animation not found");
  return it->second;
}

void AnimationController::updateAnimationState(float vx, float vy) {
  // If no animations loaded (e.g., on server), skip animation updates
  if (animations.empty()) {
    return;
  }

  AnimationDirection newDirection = velocityToDirection(vx, vy);

  // If direction changed, switch animation
  if (newDirection != currentDirection) {
    currentDirection = newDirection;

    std::string newAnimationName;
    if (newDirection == AnimationDirection::IDLE) {
      newAnimationName = "idle";
    } else {
      // Map direction to animation name
      switch (newDirection) {
        case AnimationDirection::NORTH:
          newAnimationName = "walk_north";
          break;
        case AnimationDirection::NORTHEAST:
          newAnimationName = "walk_northeast";
          break;
        case AnimationDirection::EAST:
          newAnimationName = "walk_east";
          break;
        case AnimationDirection::SOUTHEAST:
          newAnimationName = "walk_southeast";
          break;
        case AnimationDirection::SOUTH:
          newAnimationName = "walk_south";
          break;
        case AnimationDirection::SOUTHWEST:
          newAnimationName = "walk_southwest";
          break;
        case AnimationDirection::WEST:
          newAnimationName = "walk_west";
          break;
        case AnimationDirection::NORTHWEST:
          newAnimationName = "walk_northwest";
          break;
        default:
          newAnimationName = "idle";
      }
    }

    if (newAnimationName != currentAnimationName) {
      playAnimation(newAnimationName);
    }
  }
}

void AnimationController::advanceFrame(float deltaTime) {
  if (animations.empty()) return;

  const AnimationClip& currentAnim = getAnimation(currentAnimationName);
  if (currentAnim.getFrameCount() == 0) return;

  frameTimer += deltaTime;

  const AnimationFrame& currentFrame = currentAnim.getFrame(currentFrameIndex);

  // Advance to next frame if timer exceeded
  while (frameTimer >= currentFrame.duration) {
    frameTimer -= currentFrame.duration;
    currentFrameIndex++;

    // Loop or clamp
    if (currentFrameIndex >= currentAnim.getFrameCount()) {
      if (currentAnim.loop) {
        currentFrameIndex = 0;
      } else {
        currentFrameIndex = currentAnim.getFrameCount() - 1;
        frameTimer = 0.0f;
        break;
      }
    }
  }
}

void AnimationController::getCurrentFrame(int& outSrcX, int& outSrcY,
                                          int& outSrcW, int& outSrcH) const {
  if (animations.empty()) {
    // Fallback to full sprite
    outSrcX = 0;
    outSrcY = 0;
    outSrcW = 32;
    outSrcH = 32;
    return;
  }

  const AnimationClip& currentAnim = getAnimation(currentAnimationName);
  if (currentAnim.getFrameCount() == 0) {
    outSrcX = 0;
    outSrcY = 0;
    outSrcW = 32;
    outSrcH = 32;
    return;
  }

  const AnimationFrame& frame = currentAnim.getFrame(currentFrameIndex);
  outSrcX = frame.srcX;
  outSrcY = frame.srcY;
  outSrcW = frame.srcW;
  outSrcH = frame.srcH;
}

void AnimationController::reset() {
  currentFrameIndex = 0;
  frameTimer = 0.0f;
}

void AnimationController::playAnimation(const std::string& name) {
  // Tiger Style: Validate animation exists
  assert(animations.find(name) != animations.end() &&
         "Attempted to play non-existent animation");

  currentAnimationName = name;
  currentFrameIndex = 0;
  frameTimer = 0.0f;
}
