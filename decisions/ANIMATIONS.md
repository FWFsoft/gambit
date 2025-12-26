# Animation System Architecture

## Overview

The animation system uses a component-based architecture where entities implement the `Animatable` interface and own an `AnimationController` component. A global `AnimationSystem` updates all registered entities at 60 FPS.

## Core Components

### Animatable Interface (`include/Animatable.h`)
```cpp
class Animatable {
  virtual AnimationController* getAnimationController() = 0;
  virtual float getVelocityX() const = 0;
  virtual float getVelocityY() const = 0;
};
```

### AnimationController (`include/AnimationController.h`)
Manages animation state for a single entity:
- Stores animation clips (idle, walk_north, walk_east, etc.)
- Tracks current animation, frame index, and frame timer
- Provides `updateAnimationState(vx, vy)` - determines direction from velocity
- Provides `advanceFrame(deltaTime)` - progresses through frames
- Provides `getCurrentFrame(&srcX, &srcY, &srcW, &srcH)` - returns sprite sheet region

### AnimationSystem (`include/AnimationSystem.h`)
Global system that:
- Subscribes to `UpdateEvent` (60 FPS)
- Maintains list of registered `Animatable` entities
- Calls `advanceFrame()` on all controllers each frame

### AnimationAssetLoader (`include/AnimationAssetLoader.h`)
Utility for loading animations from sprite sheets:
```cpp
AnimationAssetLoader::loadPlayerAnimations(
    *player.getAnimationController(),
    "assets/player_animated.png"
);
```

## Adding New Animations to Players

### 1. Update Sprite Sheet

Create a sprite sheet with your new animation frames. Current layout (256x256 PNG, 32x32 frames):
```
Row 0: Idle (1 frame)
Row 1: Walk North (4 frames)
Row 2: Walk Northeast (4 frames)
Row 3: Walk East (4 frames)
Row 4: Walk Southeast (4 frames)
Row 5: Walk South (4 frames)
Row 6: Walk Southwest (4 frames)
Row 7: Walk West (4 frames)
Row 8: Walk Northwest (4 frames)
```

To add attack animations, extend the sprite sheet:
```
Row 9: Attack North (4 frames)
Row 10: Attack Northeast (4 frames)
... etc
```

### 2. Update AnimationAssetLoader

Add new animation clips in `src/AnimationAssetLoader.cpp`:

```cpp
void AnimationAssetLoader::loadPlayerAnimations(
    AnimationController& controller, const string& spriteSheetPath) {

  // Existing idle + walk animations...

  // Add attack animations
  controller.addAnimation("attack_north", createAttackAnimation(AnimationDirection::NORTH));
  controller.addAnimation("attack_east", createAttackAnimation(AnimationDirection::EAST));
  // ... other directions
}

AnimationClip AnimationAssetLoader::createAttackAnimation(AnimationDirection dir) {
  AnimationClip clip("attack", false);  // false = don't loop

  int row = 9 + static_cast<int>(dir);  // Rows 9-16 for attacks
  for (int i = 0; i < 4; i++) {
    AnimationFrame frame{
      i * FRAME_SIZE,      // srcX
      row * FRAME_SIZE,    // srcY
      FRAME_SIZE,          // srcW
      FRAME_SIZE,          // srcH
      80.0f                // 80ms per frame = faster attack
    };
    clip.frames.push_back(frame);
  }

  return clip;
}
```

### 3. Trigger New Animations

In your game logic where you handle attacks:

```cpp
// In ClientInput processing
if (attackButtonPressed) {
  // Determine attack direction from current facing
  player.getAnimationController()->playAnimation("attack_north");

  // When attack completes, it will return to idle (non-looping)
}
```

For auto-transitioning based on state:

```cpp
void AnimationController::updateAnimationState(float vx, float vy) {
  if (animations.empty()) return;

  // Check entity state first
  if (entity->isAttacking()) {
    playAnimation("attack_" + directionToString(currentDirection));
    return;
  }

  // Then check movement
  if (std::abs(vx) < 0.1f && std::abs(vy) < 0.1f) {
    playAnimation("idle");
  } else {
    AnimationDirection dir = velocityToDirection(vx, vy);
    playAnimation("walk_" + directionToString(dir));
  }
}
```

## Network Synchronization

Animation state is **client-side only** and deterministic:
- ✅ No network bandwidth used for animations
- ✅ Each client independently computes animation from synced velocity
- ✅ Server never tracks animation state

The server only broadcasts:
- Position (x, y)
- Velocity (vx, vy)
- Health

Clients derive animations from velocity using `velocityToDirection()`.

## Reusability for NPCs/Enemies

Any entity can use animations by:
1. Implementing `Animatable` interface
2. Owning an `AnimationController`
3. Registering with `AnimationSystem`

```cpp
struct Enemy : public Animatable {
  shared_ptr<AnimationController> animationController;
  float vx, vy;

  AnimationController* getAnimationController() override {
    return animationController.get();
  }

  float getVelocityX() const override { return vx; }
  float getVelocityY() const override { return vy; }
};

// In initialization:
Enemy enemy;
AnimationAssetLoader::loadEnemyAnimations(
    *enemy.getAnimationController(),
    "assets/enemy_animated.png"
);
animationSystem.registerEntity(&enemy);
```

## File Locations

- **Core Classes**: `include/Animation*.h` + `src/Animation*.cpp`
- **Sprite Sheets**: `assets/*.png`
- **Asset Loading**: `src/AnimationAssetLoader.cpp`
- **Player Integration**: `include/Player.h` (implements `Animatable`)
- **Rendering**: `src/RenderSystem.cpp:100` (`drawPlayer()` uses `getCurrentFrame()`)

## Future: JSON Metadata Support

When using PixelLab.ai or similar tools, animations can be loaded from JSON:

```json
{
  "spriteSheet": "character.png",
  "frameWidth": 32,
  "frameHeight": 32,
  "animations": {
    "attack_north": {
      "frames": [{"x": 0, "y": 288}, {"x": 32, "y": 288}, ...],
      "loop": false,
      "frameDuration": 80
    }
  }
}
```

Update `AnimationAssetLoader::loadPlayerAnimations()` to parse JSON and auto-load animations.
