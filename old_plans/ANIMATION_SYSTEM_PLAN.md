# Animation System Implementation Plan

## Goal
Add 8-directional character animations (idle + walking) to Gambit with a component-based architecture that can be reused for NPCs and enemies. Generate test sprite sheets with PIL while designing for future PixelLab.ai integration.

## User Requirements
- **Animation States**: Idle + 8-directional walking
- **Architecture**: Component-based - entities implement `Animatable` interface, `AnimationController` manages state
- **Sprite Sheets**: Generate test sprite with PIL, design for PixelLab.ai metadata integration later
- **Reusability**: Must work for players, NPCs, and enemies

## Current System (From Exploration)

**Sprite Rendering** ✅ (Already Implemented):
- `SpriteRenderer::drawRegion(texture, x, y, w, h, srcX, srcY, srcW, srcH)` - perfect for sprite sheets
- `TextureManager` - caches textures
- Player renders with static 32x32 sprite from `assets/player.png`

**Player Movement** ✅ (Already Implemented):
- `applyInput()` in `include/Player.h` updates position based on velocity
- GameLoop runs at 60 FPS with deltaTime
- Client prediction + server reconciliation working

**Missing for Animations**:
- ❌ No animation state in Player struct (frame index, timer, direction)
- ❌ No frame sequencing logic
- ❌ No animation state machine

---

## Architecture Design

### Component Hierarchy

```
AnimationSystem (Global)
    ├─> Subscribes to UpdateEvent (60 FPS)
    └─> Updates all AnimationController instances

AnimationController (Component)
    ├─> Owns: currentAnimation, currentFrame, frameTimer
    ├─> State machine: Idle ↔ Walk (8 directions)
    └─> Calculates sprite sheet region for current frame

Animatable (Interface)
    ├─> getAnimationController() → AnimationController*
    ├─> getVelocityX() → float
    └─> getVelocityY() → float

Player (Struct)
    ├─> Implements Animatable
    └─> Owns unique_ptr<AnimationController>
```

### Data Flow

```
GameLoop (60 FPS)
    │
    ├─> UpdateEvent
    │   ├─> AnimationSystem::onUpdate(deltaTime)
    │   │   └─> controller->advanceFrame(deltaTime)
    │   │
    │   └─> ClientPrediction::onLocalInput()
    │       └─> applyInput()
    │           ├─> Updates player.vx, player.vy
    │           └─> controller->updateAnimationState(vx, vy)
    │
    └─> RenderEvent
        └─> RenderSystem::drawPlayer()
            ├─> controller->getCurrentFrame(&srcX, &srcY, &srcW, &srcH)
            └─> spriteRenderer->drawRegion(...)
```

---

## Implementation Plan

### Phase 1: Core Animation Classes

**New Files to Create**:

1. **`include/AnimationDirection.h`**
   - Enum: `IDLE, NORTH, NORTHEAST, EAST, SOUTHEAST, SOUTH, SOUTHWEST, WEST, NORTHWEST`
   - Helper: `velocityToDirection(vx, vy)` converts velocity vector to direction
   - Uses atan2 to map velocity angle to 8 directions

2. **`include/AnimationClip.h`**
   ```cpp
   struct AnimationFrame {
     int srcX, srcY, srcW, srcH;  // Sprite sheet coordinates
     float duration;               // Frame duration in ms
   };

   struct AnimationClip {
     string name;
     vector<AnimationFrame> frames;
     bool loop;
     // Methods: getFrame(index), getFrameCount(), getTotalDuration()
   };
   ```

3. **`include/AnimationController.h` + `src/AnimationController.cpp`**
   ```cpp
   class AnimationController {
   public:
     void addAnimation(name, clip);
     void updateAnimationState(vx, vy);  // Determine direction from velocity
     void advanceFrame(deltaTime);        // Advance frame timer
     void getCurrentFrame(&srcX, &srcY, &srcW, &srcH);
   private:
     unordered_map<string, AnimationClip> animations;
     string currentAnimationName;
     int currentFrameIndex;
     float frameTimer;
     AnimationDirection currentDirection;
   };
   ```

4. **`include/Animatable.h`**
   ```cpp
   class Animatable {
   public:
     virtual AnimationController* getAnimationController() = 0;
     virtual float getVelocityX() const = 0;
     virtual float getVelocityY() const = 0;
   };
   ```

5. **`include/AnimationSystem.h` + `src/AnimationSystem.cpp`**
   ```cpp
   class AnimationSystem {
   public:
     AnimationSystem();  // Subscribe to UpdateEvent
     void registerEntity(Animatable* entity);
     void unregisterEntity(Animatable* entity);
   private:
     vector<Animatable*> entities;
     void onUpdate(const UpdateEvent& e);
   };
   ```

6. **`include/AnimationAssetLoader.h` + `src/AnimationAssetLoader.cpp`**
   ```cpp
   class AnimationAssetLoader {
   public:
     static void loadPlayerAnimations(AnimationController& controller,
                                      const string& spriteSheetPath);
   private:
     static AnimationClip createIdleAnimation();
     static AnimationClip createWalkAnimation(AnimationDirection dir);
   };
   ```

**Implementation Details**:
- `AnimationController::advanceFrame()`: Increment frameTimer by deltaTime, advance frame when timer exceeds frame.duration
- `AnimationController::updateAnimationState()`: Call `velocityToDirection()`, switch animation if direction changed
- `AnimationAssetLoader`: Currently creates clips programmatically; future version loads from JSON metadata

### Phase 2: Player Integration

**Modified Files**:

1. **`include/Player.h`**
   - Add: `struct Player : public Animatable`
   - Add member: `unique_ptr<AnimationController> animationController;`
   - Implement Animatable interface:
     ```cpp
     AnimationController* getAnimationController() override {
       return animationController.get();
     }
     float getVelocityX() const override { return vx; }
     float getVelocityY() const override { return vy; }
     ```
   - In `applyInput()` after updating position:
     ```cpp
     player.animationController->updateAnimationState(player.vx, player.vy);
     ```

2. **`src/RenderSystem.cpp`** (lines 99-116)
   - Modify `drawPlayer()` to use animation frames:
     ```cpp
     void RenderSystem::drawPlayer(const Player& player) {
       int screenX, screenY;
       camera->worldToScreen(player.x, player.y, screenX, screenY);

       const AnimationController* controller = player.getAnimationController();
       if (controller) {
         int srcX, srcY, srcW, srcH;
         controller->getCurrentFrame(srcX, srcY, srcW, srcH);

         spriteRenderer->drawRegion(*playerTexture,
                                   screenX - PLAYER_SIZE/2, screenY - PLAYER_SIZE/2,
                                   PLAYER_SIZE, PLAYER_SIZE,
                                   srcX, srcY, srcW, srcH,
                                   1.0f, 1.0f, 1.0f, 1.0f);
       } else {
         // Fallback: render static sprite
         spriteRenderer->draw(*playerTexture, ...);
       }
     }
     ```

3. **`src/client_main.cpp`**
   - After creating players, initialize animations:
     ```cpp
     AnimationSystem animationSystem;

     // Load animations into local player
     AnimationAssetLoader::loadPlayerAnimations(
         *localPlayer.getAnimationController(),
         "assets/player_animated.png");

     animationSystem.registerEntity(&localPlayer);

     // Update RenderSystem to use animated sprite sheet
     playerTexture = TextureManager::instance().get("assets/player_animated.png");
     ```

4. **`src/RemotePlayerInterpolation.cpp`**
   - When remote players join, initialize their animations:
     ```cpp
     AnimationAssetLoader::loadPlayerAnimations(
         *remotePlayer.getAnimationController(),
         "assets/player_animated.png");
     animationSystem.registerEntity(&remotePlayer);
     ```

### Phase 3: Sprite Sheet Asset

**Create Test Sprite Sheet**:

1. **`scripts/generate_test_sprite.py`** (new file)
   ```python
   #!/usr/bin/env python3
   from PIL import Image, ImageDraw

   # Create 256x256 sprite sheet (8x8 grid of 32x32 sprites)
   sprite_sheet = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
   draw = ImageDraw.Draw(sprite_sheet)

   # Row 0: Idle (1 frame) - Blue
   draw_frame(0, 0, (100, 100, 255, 255), 0)

   # Row 1: Walk North (4 frames) - Red
   for i in range(4):
       draw_frame(1, i, (255, 100, 100, 255), i)

   # Row 2-8: Walk NE, E, SE, S, SW, W, NW (different colors)
   # ... similar for other directions

   sprite_sheet.save('assets/player_animated.png')
   ```

**Sprite Sheet Layout**:
```
256x256 PNG, 8x8 grid of 32x32 frames

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

**Frame Coordinates**:
- Idle: (0, 0)
- Walk North Frame 0: (0, 32), Frame 1: (32, 32), etc.
- Walk Northeast Frame 0: (0, 64), Frame 1: (32, 64), etc.

### Phase 4: Testing

**Unit Tests** (`tests/test_animation.cpp`):

```cpp
TEST(AnimationController, InitializesWithIdle) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");
  EXPECT_EQ(controller.getCurrentAnimationName(), "idle");
}

TEST(AnimationController, TransitionsToWalkOnMovement) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");
  controller.updateAnimationState(-200.0f, -200.0f);  // North
  EXPECT_EQ(controller.getCurrentAnimationName(), "walk_north");
}

TEST(AnimationController, AdvancesFrames) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");
  controller.updateAnimationState(200.0f, 0.0f);  // East

  controller.advanceFrame(100.0f);  // 100ms
  EXPECT_EQ(controller.getCurrentFrameIndex(), 1);
}

TEST(AnimationController, LoopsWalkCycle) {
  AnimationController controller;
  AnimationAssetLoader::loadPlayerAnimations(controller, "");
  controller.updateAnimationState(200.0f, 0.0f);

  // Advance through 4 frames
  for (int i = 0; i < 4; i++) {
    controller.advanceFrame(100.0f);
  }

  EXPECT_EQ(controller.getCurrentFrameIndex(), 0);  // Looped
}

TEST(VelocityToDirection, CorrectlyMapsDirections) {
  EXPECT_EQ(velocityToDirection(200, 0), AnimationDirection::EAST);
  EXPECT_EQ(velocityToDirection(0, 200), AnimationDirection::SOUTH);
  EXPECT_EQ(velocityToDirection(0, 0), AnimationDirection::IDLE);
}
```

**Integration Testing**:
1. Run `/dev` - start 2 clients
2. Move players with WASD
3. Verify:
   - Idle animation when stopped
   - Walk animation when moving
   - Correct direction matches input
   - Smooth 4-frame cycle @ 100ms per frame (10 FPS walk)

---

## PixelLab.ai Integration (Future)

### Expected Output Format

PixelLab.ai likely outputs:
- **PNG sprite sheet**
- **JSON metadata** describing frame layout

Example JSON:
```json
{
  "spriteSheet": "character_001.png",
  "frameWidth": 32,
  "frameHeight": 32,
  "animations": {
    "idle": {
      "frames": [{"x": 0, "y": 0}],
      "loop": true,
      "frameDuration": 1000
    },
    "walk_north": {
      "frames": [
        {"x": 0, "y": 32},
        {"x": 32, "y": 32},
        {"x": 64, "y": 32},
        {"x": 96, "y": 32}
      ],
      "loop": true,
      "frameDuration": 100
    }
  }
}
```

### Future AnimationAssetLoader Enhancement

```cpp
void AnimationAssetLoader::loadPlayerAnimations(
    AnimationController& controller, const string& spriteSheetPath) {

  // Derive metadata path: "sprite.png" → "sprite.json"
  string metadataPath = spriteSheetPath;
  metadataPath = metadataPath.substr(0, metadataPath.rfind('.')) + ".json";

  ifstream file(metadataPath);
  if (!file.is_open()) {
    // Fallback to hardcoded animations
    loadHardcodedAnimations(controller);
    return;
  }

  // Parse JSON metadata (using nlohmann/json or similar)
  json metadata = json::parse(file);

  for (auto& [animName, animData] : metadata["animations"].items()) {
    AnimationClip clip(animName, animData["loop"]);

    for (auto& frameData : animData["frames"]) {
      AnimationFrame frame{
        frameData["x"], frameData["y"],
        metadata["frameWidth"], metadata["frameHeight"],
        animData["frameDuration"]
      };
      clip.frames.push_back(frame);
    }

    controller.addAnimation(animName, clip);
  }
}
```

**Migration Path**:
1. Generate sprite with PixelLab.ai
2. Export as PNG + JSON
3. Place in `assets/` directory
4. AnimationAssetLoader automatically loads from JSON
5. If JSON missing, falls back to hardcoded test animations

---

## Performance Analysis

### Memory Footprint
- AnimationController: ~200 bytes (maps, vectors, state)
- AnimationClip storage: ~1KB per entity (9 animations)
- **Total per player**: ~1.2KB (negligible)

For 4 players: **~5KB total** (minimal overhead)

### CPU Performance (Per Frame @ 60 FPS)
- `AnimationSystem::onUpdate()`: O(N) where N = entity count
  - `updateAnimationState()`: ~10 float ops
  - `advanceFrame()`: ~5 float ops
- `RenderSystem::drawPlayer()`:
  - `getCurrentFrame()`: O(1) map lookup + vector access

**Total**: ~15 float ops per entity = **60 ops for 4 players** (trivial)

### GPU Performance
No change - still 1 `drawRegion()` call per player per frame. Sprite sheet (256×256) fits easily in texture cache.

---

## Network Synchronization

### Why Animation State Doesn't Need Syncing

Animation is **client-side only** and **deterministic**:

1. **Input-Driven**: Animation state derived from velocity (vx, vy)
2. **Velocity is Synced**: Server broadcasts authoritative velocity in `StateUpdatePacket`
3. **Deterministic Calculation**: All clients compute same direction from same velocity
4. **Frame Timing is Local**: Frame advancement is client-side for visual smoothness

This means:
- ✅ Server doesn't track animation state
- ✅ No bandwidth overhead
- ✅ Network lag doesn't cause animation stutter
- ✅ Each client independently renders correct animation

---

## File Changes Summary

### New Files (12)
```
include/AnimationDirection.h
include/AnimationClip.h
include/AnimationController.h
src/AnimationController.cpp
include/Animatable.h
include/AnimationSystem.h
src/AnimationSystem.cpp
include/AnimationAssetLoader.h
src/AnimationAssetLoader.cpp
scripts/generate_test_sprite.py
tests/test_animation.cpp
assets/player_animated.png (generated)
```

### Modified Files (4)
```
include/Player.h               - Add AnimationController, implement Animatable
src/RenderSystem.cpp           - Use getCurrentFrame() for sprite regions
src/client_main.cpp            - Initialize AnimationSystem, load animations
src/RemotePlayerInterpolation.cpp - Initialize remote player animations
```

### Dependencies
None! All code uses existing libraries (STL, existing EventBus).

---

## Future Enhancements

### 1. Attack Animations
```cpp
controller.addAnimation("attack_north", createAttackAnimation(NORTH));
// Trigger via PlayerAttackEvent
```

### 2. Animation Events
Trigger gameplay at specific frames:
```cpp
struct AnimationFrame {
  vector<string> events;  // e.g., "activate_hitbox", "play_sound"
};
```

### 3. NPC/Enemy Animation
Same system works for all entities:
```cpp
struct Enemy : public Animatable {
  unique_ptr<AnimationController> animationController;
  // ... implement Animatable interface
};
```

### 4. Sprite Flipping
For 4-directional sprites (flip for other 4):
```cpp
struct AnimationFrame {
  bool flipHorizontal;
  bool flipVertical;
};
```

---

## Implementation Order

1. ✅ **Phase 1**: Core animation classes (2-3 hours)
   - AnimationDirection, AnimationClip, AnimationController
   - Animatable interface, AnimationSystem
   - AnimationAssetLoader

2. ✅ **Phase 2**: Player integration (1 hour)
   - Modify Player struct
   - Update RenderSystem::drawPlayer()
   - Initialize in client_main.cpp

3. ✅ **Phase 3**: Sprite sheet asset (30 mins)
   - Create generate_test_sprite.py
   - Generate player_animated.png

4. ✅ **Phase 4**: Testing (1 hour)
   - Write unit tests
   - Integration testing with /dev
   - Visual verification

**Total Estimate**: ~5 hours

---

## Success Criteria

- ✅ Players show idle animation when stopped
- ✅ Players show walk animation when moving
- ✅ Walk direction matches input (8 directions)
- ✅ Smooth frame transitions (4 frames @ 100ms = 400ms cycle)
- ✅ All unit tests passing
- ✅ No performance regression (~60 FPS maintained)
- ✅ Works in multiplayer (4 clients)
- ✅ Architecture reusable for NPCs/enemies

---

Ready to implement!
