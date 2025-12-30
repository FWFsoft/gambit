#include "AnimationAssetLoader.h"
#include "AnimationSystem.h"
#include "Logger.h"
#include "Player.h"
#include "test_utils.h"

TEST(AnimationSystem_RegisterEntity) {
  resetEventBus();
  AnimationSystem animSystem;

  Player player;
  player.vx = 0.0f;
  player.vy = 0.0f;

  // Register entity - should not crash
  animSystem.registerEntity(&player);
}

TEST(AnimationSystem_MultipleEntities) {
  resetEventBus();
  AnimationSystem animSystem;

  // Create multiple players and load their animations
  Player player1, player2, player3;
  AnimationAssetLoader::loadPlayerAnimations(*player1.getAnimationController(),
                                             "");
  AnimationAssetLoader::loadPlayerAnimations(*player2.getAnimationController(),
                                             "");
  AnimationAssetLoader::loadPlayerAnimations(*player3.getAnimationController(),
                                             "");

  player1.vx = 1.0f;
  player1.vy = 0.0f;
  player2.vx = 0.0f;
  player2.vy = 1.0f;
  player3.vx = -1.0f;
  player3.vy = -1.0f;

  animSystem.registerEntity(&player1);
  animSystem.registerEntity(&player2);
  animSystem.registerEntity(&player3);

  // Update animation states based on velocity (normally done by game logic)
  player1.getAnimationController()->updateAnimationState(player1.vx,
                                                         player1.vy);
  player2.getAnimationController()->updateAnimationState(player2.vx,
                                                         player2.vy);
  player3.getAnimationController()->updateAnimationState(player3.vx,
                                                         player3.vy);

  // Verify directions set correctly based on velocity
  AnimationDirection dir1 =
      player1.getAnimationController()->getCurrentDirection();
  AnimationDirection dir2 =
      player2.getAnimationController()->getCurrentDirection();
  AnimationDirection dir3 =
      player3.getAnimationController()->getCurrentDirection();

  assert(dir1 == AnimationDirection::EAST);
  assert(dir2 == AnimationDirection::SOUTH);
  assert(dir3 == AnimationDirection::NORTHWEST);
}

TEST(AnimationSystem_UnregisterEntity) {
  resetEventBus();
  AnimationSystem animSystem;

  Player player;
  player.vx = 1.0f;
  player.vy = 0.0f;

  animSystem.registerEntity(&player);

  // Unregister - should not crash
  animSystem.unregisterEntity(&player);
}

TEST(AnimationSystem_RegisterSameEntityTwice) {
  resetEventBus();
  AnimationSystem animSystem;

  Player player;

  // Register twice - should handle gracefully
  animSystem.registerEntity(&player);
  animSystem.registerEntity(&player);
}

TEST(AnimationSystem_UnregisterNonExistentEntity) {
  resetEventBus();
  AnimationSystem animSystem;

  Player player;

  // Unregister entity that was never registered - should not crash
  animSystem.unregisterEntity(&player);
}

TEST(AnimationSystem_EventDriven) {
  // Test that AnimationSystem updates entities via UpdateEvent
  resetEventBus();
  AnimationSystem animSystem;

  Player player;
  AnimationAssetLoader::loadPlayerAnimations(*player.getAnimationController(),
                                             "");
  player.vx = 1.0f;
  player.vy = 0.0f;

  animSystem.registerEntity(&player);

  // Update animation state (normally done by game logic)
  player.getAnimationController()->updateAnimationState(player.vx, player.vy);

  // Verify direction is set
  assert(player.getAnimationController()->getCurrentDirection() ==
         AnimationDirection::EAST);

  // Publish an UpdateEvent to trigger animation frame updates
  UpdateEvent updateEvent;
  updateEvent.deltaTime = 16.67f;
  updateEvent.frameNumber = 1;

  EventBus::instance().publish(updateEvent);

  // The system should have advanced the animation frames
  AnimationController* controller = player.getAnimationController();
  assert(controller != nullptr);
  assert(controller->getCurrentDirection() == AnimationDirection::EAST);
}

TEST(AnimationSystem_RegisterUnregisterCycle) {
  resetEventBus();
  AnimationSystem animSystem;

  Player player;

  // Register, unregister, register again - should handle gracefully
  for (int i = 0; i < 10; i++) {
    animSystem.registerEntity(&player);
    animSystem.unregisterEntity(&player);
  }

  // Final register
  animSystem.registerEntity(&player);
}

int main() {
  Logger::init();

  test_AnimationSystem_RegisterEntity();
  test_AnimationSystem_MultipleEntities();
  test_AnimationSystem_UnregisterEntity();
  test_AnimationSystem_RegisterSameEntityTwice();
  test_AnimationSystem_UnregisterNonExistentEntity();
  test_AnimationSystem_EventDriven();
  test_AnimationSystem_RegisterUnregisterCycle();

  return 0;
}
