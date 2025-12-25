# Code Quality Improvement: Tests and Refactoring

## Goal
1. Add comprehensive unit tests for collision detection system
2. Refactor multi-parameter functions to use parameter objects (structs)
3. Improve code organization and reduce function/file sizes

## Current State Analysis

### Test Coverage Gaps
- **ZERO collision tests** - CollisionSystem, AABB completely untested
- test_player.cpp tests movement but not collision
- 6 existing test suites, but none cover new collision functionality

### Code Smell: Too Many Parameters
- `applyInput()` - 8 parameters (20+ call sites)
- `RenderSystem()` - 7 parameters
- `ClientPrediction()` - 5 parameters
- `ServerGameState()` - 4 parameters

### Large Functions Needing Extraction
- `client_main.cpp::main()` - 79 lines
- `ServerGameState::onClientConnected()` - 65 lines (spawn logic extractable)
- `ClientPrediction::reconcile()` - 60 lines

---

## Implementation Plan

### Phase 1: Add Collision Tests
**Goal**: Achieve comprehensive test coverage for collision system

**New File**: `tests/test_collision.cpp`

**Test Structure**:
```cpp
// AABB tests (8 tests)
TEST(AABB_Intersects_NoOverlap)
TEST(AABB_Intersects_PartialOverlap)
TEST(AABB_Intersects_CompleteContainment)
TEST(AABB_Intersects_EdgeTouching)
TEST(AABB_Contains_Inside)
TEST(AABB_Contains_Outside)
TEST(AABB_Contains_OnEdge)
TEST(AABB_Contains_AtCorner)

// CollisionSystem tests (12 tests)
TEST(CollisionSystem_NoCollision)
TEST(CollisionSystem_DirectBlock)
TEST(CollisionSystem_SlideAlongX)
TEST(CollisionSystem_SlideAlongY)
TEST(CollisionSystem_BlockBothAxes)
TEST(CollisionSystem_CornerSliding)
TEST(CollisionSystem_MultipleShapes)
TEST(CollisionSystem_IsPositionValid_Valid)
TEST(CollisionSystem_IsPositionValid_Invalid)
TEST(CollisionSystem_IsPositionValid_OnEdge)
TEST(CollisionSystem_PlayerRadiusAccounting)
TEST(CollisionSystem_ZeroMovement)
```

**Modified Files**:
- `CMakeLists.txt` - Add test_collision executable

**Testing Strategy**:
- Test AABB intersection logic thoroughly (all 8 directions + containment)
- Test sliding collision resolution (X-only, Y-only, both blocked)
- Test position validation for spawn points
- Test edge cases (zero movement, exact boundaries, player radius)

---

### Phase 2: Refactor applyInput() to Use Parameter Object
**Goal**: Reduce 8 parameters to 2 (Player& and MovementInput&)

**New File**: `include/MovementInput.h`
```cpp
#pragma once

#include "CollisionSystem.h"

struct MovementInput {
  // Input state
  bool moveLeft;
  bool moveRight;
  bool moveUp;
  bool moveDown;

  // Simulation context
  float deltaTime;
  float worldWidth;
  float worldHeight;
  const CollisionSystem* collisionSystem;

  // Constructors for convenience
  MovementInput()
    : moveLeft(false), moveRight(false), moveUp(false), moveDown(false),
      deltaTime(16.67f), worldWidth(800.0f), worldHeight(600.0f),
      collisionSystem(nullptr) {}

  MovementInput(bool left, bool right, bool up, bool down, float dt,
                float ww, float wh, const CollisionSystem* cs = nullptr)
    : moveLeft(left), moveRight(right), moveUp(up), moveDown(down),
      deltaTime(dt), worldWidth(ww), worldHeight(wh), collisionSystem(cs) {}
};
```

**Modified Files**:

1. **include/Player.h** - Update signature:
```cpp
// Old (8 parameters):
inline void applyInput(Player& player, bool moveLeft, bool moveRight,
                       bool moveUp, bool moveDown, float deltaTime,
                       float worldWidth, float worldHeight,
                       const CollisionSystem* collisionSystem = nullptr)

// New (2 parameters):
inline void applyInput(Player& player, const MovementInput& input)
```

2. **src/ClientPrediction.cpp** - Update calls:
```cpp
// Line 42 (onLocalInput):
// Old:
applyInput(localPlayer, e.moveLeft, e.moveRight, e.moveUp, e.moveDown, 16.67f,
           worldWidth, worldHeight, collisionSystem);
// New:
MovementInput input(e.moveLeft, e.moveRight, e.moveUp, e.moveDown,
                   16.67f, worldWidth, worldHeight, collisionSystem);
applyInput(localPlayer, input);

// Line 145 (reconcile):
// Similar update for replay loop
```

3. **src/ServerGameState.cpp** - Update call (line 150)

4. **All test files** - Update 15+ test calls:
   - tests/test_player.cpp
   - tests/test_inputsystem.cpp
   - tests/test_eventbus.cpp

**Benefits**:
- Easier to add new movement parameters (gravity, acceleration, etc.)
- Clear grouping of related parameters
- Easier to pass movement context around
- Self-documenting code

---

### Phase 3: Create WorldConfig Struct
**Goal**: Consolidate world dimensions + collision system

**New File**: `include/WorldConfig.h`
```cpp
#pragma once

#include "CollisionSystem.h"

struct WorldConfig {
  float width;
  float height;
  const CollisionSystem* collisionSystem;

  WorldConfig(float w, float h, const CollisionSystem* cs = nullptr)
    : width(w), height(h), collisionSystem(cs) {}
};
```

**Modified Files**:

1. **include/ClientPrediction.h**:
```cpp
// Old:
ClientPrediction(NetworkClient* client, uint32_t localPlayerId,
                 float worldWidth, float worldHeight,
                 const CollisionSystem* collisionSystem);

// New:
ClientPrediction(NetworkClient* client, uint32_t localPlayerId,
                 const WorldConfig& world);
```

2. **include/ServerGameState.h**:
```cpp
// Old:
ServerGameState(NetworkServer* server, float worldWidth, float worldHeight,
                const CollisionSystem* collisionSystem);

// New:
ServerGameState(NetworkServer* server, const WorldConfig& world);
```

3. **src/client_main.cpp** - Create WorldConfig instance:
```cpp
WorldConfig world(map.getWorldWidth(), map.getWorldHeight(), &collisionSystem);
ClientPrediction clientPrediction(&client, localPlayerId, world);
```

4. **src/server_main.cpp** - Similar update

**Benefits**:
- World dimensions and collision always travel together
- Easier to add world-level properties (gravity, bounds, etc.)
- Consistent API across client and server

---

### Phase 4: Extract ServerGameState Spawn Logic
**Goal**: Reduce onClientConnected() from 65 to ~30 lines

**New Methods in ServerGameState.h**:
```cpp
private:
  Player createPlayer(uint32_t playerId);
  bool findValidSpawnPosition(float& x, float& y);
  void assignPlayerColor(Player& player, size_t playerCount);
```

**Implementation in ServerGameState.cpp**:
```cpp
Player ServerGameState::createPlayer(uint32_t playerId) {
  Player player;
  player.id = playerId;
  player.vx = 0;
  player.vy = 0;
  player.health = 100.0f;
  player.lastInputSequence = 0;

  // Find spawn position
  player.x = worldWidth / 2.0f;
  player.y = worldHeight / 2.0f;
  if (!findValidSpawnPosition(player.x, player.y)) {
    Logger::error("Failed to find valid spawn position");
  }

  return player;
}

bool ServerGameState::findValidSpawnPosition(float& x, float& y) {
  if (collisionSystem &&
      !collisionSystem->isPositionValid(x, y, PLAYER_RADIUS)) {
    Logger::info("Default spawn invalid, searching...");

    for (float radius = 50.0f; radius < 500.0f; radius += 50.0f) {
      for (float angle = 0; angle < 360.0f; angle += 45.0f) {
        float testX = worldWidth / 2.0f +
                      radius * std::cos(angle * 3.14159f / 180.0f);
        float testY = worldHeight / 2.0f +
                      radius * std::sin(angle * 3.14159f / 180.0f);
        if (collisionSystem->isPositionValid(testX, testY, PLAYER_RADIUS)) {
          x = testX;
          y = testY;
          return true;
        }
      }
    }
    return false;
  }
  return true;
}

void ServerGameState::assignPlayerColor(Player& player, size_t playerCount) {
  const uint8_t colors[4][3] = {
      {255, 0, 0},   // Red
      {0, 255, 0},   // Green
      {0, 0, 255},   // Blue
      {255, 255, 0}  // Yellow
  };
  int colorIndex = playerCount % 4;
  player.r = colors[colorIndex][0];
  player.g = colors[colorIndex][1];
  player.b = colors[colorIndex][2];
}
```

**Modified**: `ServerGameState::onClientConnected()` becomes:
```cpp
void ServerGameState::onClientConnected(const ClientConnectedEvent& e) {
  uint32_t playerId = e.clientId;

  Player player = createPlayer(playerId);
  assignPlayerColor(player, players.size());

  players[playerId] = player;
  peerToPlayerId[e.peer] = playerId;

  Logger::info("Player " + std::to_string(playerId) + " joined");

  // Broadcast join
  PlayerJoinedPacket packet;
  packet.playerId = playerId;
  packet.r = player.r;
  packet.g = player.g;
  packet.b = player.b;
  server->broadcastPacket(serialize(packet));
}
```

**Benefits**:
- Function reduced from 65 to ~20 lines
- Each extracted method has single responsibility
- Spawn logic reusable if needed elsewhere
- Easier to test spawn algorithm independently

---

### Phase 5: Optional - RenderSystem Config Struct
**Goal**: Reduce constructor from 7 to 3 parameters

**New File**: `include/RenderConfig.h`
```cpp
#pragma once

struct RenderConfig {
  Window* window;
  Camera* camera;
  TiledMap* map;
  TileRenderer* tileRenderer;
  CollisionDebugRenderer* debugRenderer;
};

struct RenderState {
  ClientPrediction* prediction;
  RemotePlayerInterpolation* interpolation;
};
```

**Modified**:
```cpp
// Old:
RenderSystem(Window*, ClientPrediction*, RemotePlayerInterpolation*,
             Camera*, TiledMap*, TileRenderer*, CollisionDebugRenderer*);

// New:
RenderSystem(const RenderConfig& config, const RenderState& state);
```

**Note**: Lower priority - only if time permits

---

## File Changes Summary

### New Files (3)
```
include/MovementInput.h        (movement parameter object)
include/WorldConfig.h          (world configuration)
tests/test_collision.cpp       (collision system tests - 20 tests)
```

### Modified Files (15)
```
include/Player.h               (applyInput signature)
include/ClientPrediction.h     (constructor signature)
include/ServerGameState.h      (constructor + new private methods)
src/ClientPrediction.cpp       (update applyInput calls)
src/ServerGameState.cpp        (extract spawn logic, update calls)
src/client_main.cpp            (use WorldConfig)
src/server_main.cpp            (use WorldConfig)
tests/test_player.cpp          (update applyInput calls - 13 tests)
tests/test_inputsystem.cpp     (update applyInput calls - 3 tests)
tests/test_eventbus.cpp        (update applyInput calls - 5 tests)
CMakeLists.txt                 (add test_collision)
```

### Optional (Phase 5)
```
include/RenderConfig.h         (render system config)
include/RenderSystem.h         (updated constructor)
src/RenderSystem.cpp           (updated constructor)
src/client_main.cpp            (use RenderConfig)
```

---

## Testing Strategy

### Phase 1 Testing (Collision Tests)
After adding test_collision.cpp:
```bash
make build
cd build && ctest --output-on-failure
```
Expected: 7 test suites, 55 total tests (35 existing + 20 new), 100% pass rate

### Phase 2-4 Testing (Refactoring)
After each refactoring phase:
```bash
make build
cd build && ctest --output-on-failure
```
Expected: All existing tests still pass (regression testing)
Expected: No behavioral changes, only API improvements

### Integration Testing
```bash
make dev  # Run 1 server + 4 clients
```
- Test collision still works
- Test F1 debug toggle still works
- Test multiplayer synchronization
- Test spawn position validation

---

## Implementation Order

**Recommended sequence**:
1. Phase 1: Add collision tests (validation before refactoring)
2. Phase 2: MovementInput refactor (highest impact)
3. Phase 3: WorldConfig refactor (complements Phase 2)
4. Phase 4: Extract spawn logic (code quality win)
5. Phase 5: RenderConfig (optional, if time permits)

**Rationale**:
- Tests first ensure refactoring doesn't break functionality
- MovementInput has widest impact (20+ call sites)
- WorldConfig is natural follow-up (uses CollisionSystem)
- Spawn extraction is independent, can be done anytime
- RenderConfig is nice-to-have, lower priority

---

## Benefits Summary

**Code Quality**:
- Reduce average function parameters from 6+ to 2-3
- Extract long functions (65+ lines) to focused helpers (<30 lines)
- Single Responsibility Principle applied consistently

**Testability**:
- 100% collision code coverage (currently 0%)
- Easier to test with parameter objects
- Spawn logic testable in isolation

**Maintainability**:
- Self-documenting parameter objects
- Easier to extend (add new parameters to struct, not function signature)
- Reduced coupling between components

**Type Safety**:
- Compile-time grouping of related parameters
- Harder to mix up parameter order
- Clear semantic meaning (MovementInput vs 8 bools/floats)

---

## Critical Files

**Most Important**:
1. `/Users/mkornfield/dev/gambit/include/Player.h:39-88` - applyInput refactor
2. `/Users/mkornfield/dev/gambit/tests/test_collision.cpp` - NEW collision tests
3. `/Users/mkornfield/dev/gambit/src/ServerGameState.cpp:37-102` - spawn extraction

**High Impact**:
4. `/Users/mkornfield/dev/gambit/include/MovementInput.h` - NEW parameter object
5. `/Users/mkornfield/dev/gambit/include/WorldConfig.h` - NEW config object
6. `/Users/mkornfield/dev/gambit/src/ClientPrediction.cpp` - update calls

**Supporting**:
7-15. All test files and main.cpp files for updated calls
