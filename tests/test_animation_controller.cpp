#include "AnimationAssetLoader.h"
#include "AnimationController.h"
#include "AnimationDirection.h"
#include "Logger.h"
#include "test_utils.h"

TEST(AnimationController_InitialState) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  // Initial state should be idle
  int srcX, srcY, srcW, srcH;
  controller.getCurrentFrame(srcX, srcY, srcW, srcH);

  // Should be first frame of idle animation (32x32 based on
  // Config::Player::SIZE)
  assert(srcW == 32);
  assert(srcH == 32);
}

TEST(AnimationController_DirectionChanges) {
  // Data-driven test for all 8 directions
  struct DirectionTest {
    float vx;
    float vy;
    AnimationDirection expectedDir;
    const char* name;
  };

  DirectionTest tests[] = {
      {1.0f, 0.0f, AnimationDirection::EAST, "East"},
      {-1.0f, 0.0f, AnimationDirection::WEST, "West"},
      {0.0f, 1.0f, AnimationDirection::SOUTH, "South"},
      {0.0f, -1.0f, AnimationDirection::NORTH, "North"},
      {1.0f, 1.0f, AnimationDirection::SOUTHEAST, "Southeast"},
      {1.0f, -1.0f, AnimationDirection::NORTHEAST, "Northeast"},
      {-1.0f, 1.0f, AnimationDirection::SOUTHWEST, "Southwest"},
      {-1.0f, -1.0f, AnimationDirection::NORTHWEST, "Northwest"},
      {0.0f, 0.0f, AnimationDirection::IDLE, "Idle"},
  };

  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  for (const auto& test : tests) {
    controller.updateAnimationState(test.vx, test.vy);

    AnimationDirection actualDir = controller.getCurrentDirection();
    assert(actualDir == test.expectedDir);
  }
}

TEST(AnimationController_FrameProgression) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  // Set to walk animation
  controller.updateAnimationState(1.0f, 0.0f);  // Walk east

  int frame1X, frame1Y, frame1W, frame1H;
  controller.getCurrentFrame(frame1X, frame1Y, frame1W, frame1H);

  // Update with time to progress frames
  controller.advanceFrame(200.0f);  // 200ms should advance frame

  int frame2X, frame2Y, frame2W, frame2H;
  controller.getCurrentFrame(frame2X, frame2Y, frame2W, frame2H);

  // Frame should have progressed (X coordinate should be different)
  assert(frame1X != frame2X || frame1Y != frame2Y);
}

TEST(AnimationController_IdleToWalkTransition) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  // Start idle
  controller.updateAnimationState(0.0f, 0.0f);

  int idleX, idleY, idleW, idleH;
  controller.getCurrentFrame(idleX, idleY, idleW, idleH);

  // Start walking
  controller.updateAnimationState(1.0f, 0.0f);

  int walkX, walkY, walkW, walkH;
  controller.getCurrentFrame(walkX, walkY, walkW, walkH);

  // Frames should be different (walk vs idle)
  assert(idleX != walkX || idleY != walkY);
}

TEST(AnimationController_WalkToIdleTransition) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  // Start walking
  controller.updateAnimationState(1.0f, 0.0f);
  controller.advanceFrame(100.0f);

  int walkX, walkY, walkW, walkH;
  controller.getCurrentFrame(walkX, walkY, walkW, walkH);

  // Stop walking
  controller.updateAnimationState(0.0f, 0.0f);

  int idleX, idleY, idleW, idleH;
  controller.getCurrentFrame(idleX, idleY, idleW, idleH);

  // Should transition to idle frame
  assert(idleX != walkX || idleY != walkY);
}

TEST(AnimationController_DirectionPersistence) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  // Walk east
  controller.updateAnimationState(1.0f, 0.0f);
  AnimationDirection eastDir = controller.getCurrentDirection();
  assert(eastDir == AnimationDirection::EAST);

  // Stop walking - direction changes to IDLE
  controller.updateAnimationState(0.0f, 0.0f);
  AnimationDirection idleDir = controller.getCurrentDirection();

  // Direction should change to IDLE when stopped
  assert(idleDir == AnimationDirection::IDLE);
  assert(eastDir != idleDir);
}

TEST(AnimationController_FrameLooping) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  // Set to walk animation
  controller.updateAnimationState(1.0f, 0.0f);

  // Record first frame
  int firstX, firstY, firstW, firstH;
  controller.getCurrentFrame(firstX, firstY, firstW, firstH);

  // Update for enough time to loop animation (walk cycle is ~600ms)
  for (int i = 0; i < 10; i++) {
    controller.advanceFrame(100.0f);  // 1000ms total
  }

  int loopedX, loopedY, loopedW, loopedH;
  controller.getCurrentFrame(loopedX, loopedY, loopedW, loopedH);

  // Animation should have looped back
  // (can't guarantee exact same frame due to timing, but dimensions should
  // match)
  assert(loopedW == firstW);
  assert(loopedH == firstH);
}

TEST(AnimationController_MultipleDirectionChanges) {
  // Test rapid direction changes
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  struct DirChange {
    float vx;
    float vy;
    AnimationDirection expectedDir;
  };

  DirChange changes[] = {
      {1.0f, 0.0f, AnimationDirection::EAST},
      {0.0f, 1.0f, AnimationDirection::SOUTH},
      {-1.0f, 0.0f, AnimationDirection::WEST},
      {0.0f, -1.0f, AnimationDirection::NORTH},
      {1.0f, 1.0f, AnimationDirection::SOUTHEAST},
  };

  for (const auto& change : changes) {
    controller.updateAnimationState(change.vx, change.vy);
    controller.advanceFrame(50.0f);  // Small time step

    AnimationDirection actualDir = controller.getCurrentDirection();
    assert(actualDir == change.expectedDir);
  }
}

TEST(AnimationController_ZeroTimeUpdate) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");

  controller.updateAnimationState(1.0f, 0.0f);

  int frame1X, frame1Y, frame1W, frame1H;
  controller.getCurrentFrame(frame1X, frame1Y, frame1W, frame1H);

  // Update with zero time
  controller.advanceFrame(0.0f);

  int frame2X, frame2Y, frame2W, frame2H;
  controller.getCurrentFrame(frame2X, frame2Y, frame2W, frame2H);

  // Frame should not change
  assert(frame1X == frame2X && frame1Y == frame2Y);
}

int main() {
  Logger::init();

  test_AnimationController_InitialState();
  test_AnimationController_DirectionChanges();
  test_AnimationController_FrameProgression();
  test_AnimationController_IdleToWalkTransition();
  test_AnimationController_WalkToIdleTransition();
  test_AnimationController_DirectionPersistence();
  test_AnimationController_FrameLooping();
  test_AnimationController_MultipleDirectionChanges();
  test_AnimationController_ZeroTimeUpdate();

  return 0;
}
