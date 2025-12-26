#include "AnimationAssetLoader.h"

#include "Logger.h"
#include "config/AnimationConfig.h"
#include "config/PlayerConfig.h"

// Sprite sheet layout (256x256 PNG, 32x32 frames):
// Row 0: Idle (0, 0)
// Row 1: Walk North (4 frames, y=32)
// Row 2: Walk Northeast (4 frames, y=64)
// Row 3: Walk East (4 frames, y=96)
// Row 4: Walk Southeast (4 frames, y=128)
// Row 5: Walk South (4 frames, y=160)
// Row 6: Walk Southwest (4 frames, y=192)
// Row 7: Walk West (4 frames, y=224)
// Top-right: Walk Northwest (4 frames, y=0, x=128+)

void AnimationAssetLoader::loadPlayerAnimations(
    AnimationController& controller, const std::string& spriteSheetPath) {
  // TODO: In future, load from JSON metadata if available
  // For now, use hardcoded layout

  // Add idle animation
  controller.addAnimation("idle", createIdleAnimation());

  // Add walk animations for all 8 directions
  controller.addAnimation("walk_north",
                          createWalkAnimation(AnimationDirection::NORTH));
  controller.addAnimation("walk_northeast",
                          createWalkAnimation(AnimationDirection::NORTHEAST));
  controller.addAnimation("walk_east",
                          createWalkAnimation(AnimationDirection::EAST));
  controller.addAnimation("walk_southeast",
                          createWalkAnimation(AnimationDirection::SOUTHEAST));
  controller.addAnimation("walk_south",
                          createWalkAnimation(AnimationDirection::SOUTH));
  controller.addAnimation("walk_southwest",
                          createWalkAnimation(AnimationDirection::SOUTHWEST));
  controller.addAnimation("walk_west",
                          createWalkAnimation(AnimationDirection::WEST));
  controller.addAnimation("walk_northwest",
                          createWalkAnimation(AnimationDirection::NORTHWEST));

  Logger::info("Loaded player animations from: " + spriteSheetPath);
}

AnimationClip AnimationAssetLoader::createIdleAnimation() {
  AnimationClip clip("idle", true);

  // Single frame at (0, 0)
  AnimationFrame frame{0,                                        // srcX
                       0,                                        // srcY
                       Config::Player::FRAME_WIDTH,              // srcW
                       Config::Player::FRAME_HEIGHT,             // srcH
                       Config::Animation::IDLE_FRAME_DURATION};  // duration
  clip.frames.push_back(frame);

  return clip;
}

AnimationClip AnimationAssetLoader::createWalkAnimation(
    AnimationDirection dir) {
  AnimationClip clip("walk", true);

  // Determine row based on direction
  int baseY = 0;
  int baseX = 0;

  switch (dir) {
    case AnimationDirection::NORTH:
      baseY = Config::Animation::ROW_NORTH;
      break;
    case AnimationDirection::NORTHEAST:
      baseY = Config::Animation::ROW_NORTHEAST;
      break;
    case AnimationDirection::EAST:
      baseY = Config::Animation::ROW_EAST;
      break;
    case AnimationDirection::SOUTHEAST:
      baseY = Config::Animation::ROW_SOUTHEAST;
      break;
    case AnimationDirection::SOUTH:
      baseY = Config::Animation::ROW_SOUTH;
      break;
    case AnimationDirection::SOUTHWEST:
      baseY = Config::Animation::ROW_SOUTHWEST;
      break;
    case AnimationDirection::WEST:
      baseY = Config::Animation::ROW_WEST;
      break;
    case AnimationDirection::NORTHWEST:
      baseY = Config::Animation::ROW_NORTHWEST;
      baseX = Config::Animation::COL_WALK_START;
      break;
    default:
      baseY = Config::Animation::ROW_SOUTH;  // Fallback to south
  }

  // Create 4-frame walk cycle
  for (int i = 0; i < 4; i++) {
    AnimationFrame frame{baseX + (i * Config::Player::FRAME_WIDTH),  // srcX
                         baseY,                                      // srcY
                         Config::Player::FRAME_WIDTH,                // srcW
                         Config::Player::FRAME_HEIGHT,               // srcH
                         Config::Animation::WALK_FRAME_DURATION};    // duration
    clip.frames.push_back(frame);
  }

  return clip;
}
