# Collision Detection System for Tiled Maps

## Goal
Add collision detection using Tiled map object layers with both visualization and working collision physics. Load a test map with collision boundaries and prevent players from walking through walls.

## Current State
- TiledMap (src/TiledMap.cpp) only loads tile layers, not object layers
- No collision system exists
- Player movement (include/Player.h applyInput()) has no collision checks
- Both client and server use the same movement logic

## Requirements
1. Load collision data from Tiled object layers (rectangles initially)
2. Implement collision detection blocking player movement
3. Debug visualization with F1 toggle
4. Works on both client (prediction) and server (authoritative)
5. Use existing tmxlite example map or similar with collision data

---

## Implementation Plan

### Phase 1: Collision Data Structures
**Goal**: Define collision shape data structures

**New File**: `include/CollisionShape.h`
```cpp
struct AABB {
  float x, y, width, height;
  bool intersects(const AABB& other) const;
  bool contains(float px, float py) const;
};

struct CollisionShape {
  enum class Type { Rectangle, Polygon };
  Type type;
  AABB aabb;
  std::string name;  // From Tiled object
};
```

---

### Phase 2: Load Collision from Tiled Maps
**Goal**: Extract object layers from TMX files

**Modified Files**:
- `include/TiledMap.h`
  - Add: `std::vector<CollisionShape> collisionShapes;`
  - Add: `const std::vector<CollisionShape>& getCollisionShapes() const;`

- `src/TiledMap.cpp`
  - Add method: `void extractCollisionShapes();`
  - Call from `load()` after extracting tile layers
  - Iterate `tmxMap.getLayers()` for `Layer::Type::Object`
  - Extract rectangles from `tmx::ObjectGroup`
  - Convert object positions to collision AABBs
  - Log: "Loaded N collision shapes"

**Implementation**:
```cpp
void TiledMap::extractCollisionShapes() {
  collisionShapes.clear();
  for (const auto& layer : tmxMap.getLayers()) {
    if (layer->getType() == tmx::Layer::Type::Object) {
      const auto& objectLayer = layer->getLayerAs<tmx::ObjectGroup>();
      for (const auto& obj : objectLayer.getObjects()) {
        if (obj.getShape() == tmx::Object::Shape::Rectangle) {
          CollisionShape shape;
          shape.type = CollisionShape::Type::Rectangle;
          shape.name = obj.getName();
          const auto& aabb = obj.getAABB();
          const auto& pos = obj.getPosition();
          shape.aabb = {pos.x, pos.y, aabb.width, aabb.height};
          collisionShapes.push_back(shape);
        }
      }
    }
  }
}
```

---

### Phase 3: Collision Detection System
**Goal**: Implement collision checking logic

**New Files**:
- `include/CollisionSystem.h`
- `src/CollisionSystem.cpp`

**CollisionSystem Class**:
```cpp
class CollisionSystem {
public:
  CollisionSystem(const std::vector<CollisionShape>& shapes);

  // Returns true if movement allowed, false if blocked
  // Modifies newX/newY to slide along walls
  bool checkMovement(float oldX, float oldY, float& newX, float& newY,
                     float playerRadius = 16.0f) const;

  bool isPositionValid(float x, float y, float playerRadius = 16.0f) const;

  const std::vector<CollisionShape>& getShapes() const;

private:
  const std::vector<CollisionShape>& collisionShapes;
  bool intersects(float px, float py, float radius, const CollisionShape& shape) const;
  void resolveCollision(float oldX, float oldY, float& newX, float& newY,
                        float radius, const CollisionShape& shape) const;
};
```

**Key Algorithm** (in checkMovement):
1. Check if new position intersects any collision shape
2. If collision detected, try axis-aligned sliding (X-only, then Y-only)
3. If both blocked, stay at old position

---

### Phase 4: Integrate with Movement System
**Goal**: Add collision checks to player movement

**Modified File**: `include/Player.h`
- Update `applyInput()` signature:
  ```cpp
  inline void applyInput(Player& player, bool moveLeft, bool moveRight,
                         bool moveUp, bool moveDown, float deltaTime,
                         float worldWidth, float worldHeight,
                         const CollisionSystem* collisionSystem = nullptr)
  ```
- Before setting player.x/y, call:
  ```cpp
  if (collisionSystem != nullptr) {
    collisionSystem->checkMovement(oldX, oldY, newX, newY, PLAYER_RADIUS);
  }
  ```
- Add constant: `constexpr float PLAYER_RADIUS = 16.0f;`

---

### Phase 5: Server-Side Integration
**Goal**: Server enforces collision

**Modified Files**:
- `include/ServerGameState.h`
  - Add member: `const CollisionSystem* collisionSystem;`
  - Update constructor to accept CollisionSystem

- `src/ServerGameState.cpp`
  - In `processClientInput()`: Pass collision system to `applyInput()`
  - In `onClientConnected()`: Validate spawn position with `isPositionValid()`

- `src/server_main.cpp`:
  ```cpp
  TiledMap tiledMap;
  tiledMap.load("assets/test_map.tmx");

  CollisionSystem collisionSystem(tiledMap.getCollisionShapes());

  ServerGameState gameState(&server, tiledMap.getWorldWidth(),
                           tiledMap.getWorldHeight(), &collisionSystem);
  ```

---

### Phase 6: Client-Side Integration
**Goal**: Client prediction respects collision

**Modified Files**:
- `include/ClientPrediction.h`
  - Add member: `const CollisionSystem* collisionSystem;`
  - Update constructor

- `src/ClientPrediction.cpp`
  - In `onLocalInput()`: Pass collision system to `applyInput()`
  - In `reconcile()`: Pass collision system when replaying inputs

- `src/client_main.cpp`:
  ```cpp
  TiledMap tiledMap;
  tiledMap.load("assets/test_map.tmx");

  CollisionSystem collisionSystem(tiledMap.getCollisionShapes());

  ClientPrediction clientPrediction(&client, 0,
                                    tiledMap.getWorldWidth(),
                                    tiledMap.getWorldHeight(),
                                    &collisionSystem);
  ```

---

### Phase 7: Debug Visualization
**Goal**: Render collision shapes with F1 toggle

**New Files**:
- `include/CollisionDebugRenderer.h`
- `src/CollisionDebugRenderer.cpp`

**CollisionDebugRenderer Class**:
```cpp
class CollisionDebugRenderer {
public:
  CollisionDebugRenderer(SDL_Renderer* renderer, Camera* camera,
                         const CollisionSystem* collisionSystem);
  void render();
  void toggle();
  bool isEnabled() const;

private:
  SDL_Renderer* renderer;
  Camera* camera;
  const CollisionSystem* collisionSystem;
  bool enabled;

  void renderAABB(const AABB& aabb, uint8_t r, uint8_t g, uint8_t b);
};
```

**Rendering Strategy**:
- Convert AABB corners from world coords to screen coords using camera
- Draw outline with SDL_RenderDrawLine (4 lines per rectangle)
- Red color for collision rectangles

**Modified Files**:
- `include/RenderSystem.h` - Add `CollisionDebugRenderer*` member
- `src/RenderSystem.cpp` - Call `collisionDebugRenderer->render()` after tiles/players
- `include/InputSystem.h` - Add `CollisionDebugRenderer*` member
- `src/InputSystem.cpp` - Add F1 key handler to toggle debug renderer
- `src/client_main.cpp` - Instantiate and wire up debug renderer

---

### Phase 8: Test Map Setup
**Goal**: Create or find map with collision data

**Option A** (Recommended): Modify existing test_map.tmx
1. Open `assets/test_map.tmx` in Tiled editor
2. Layer → Add Object Layer → Name: "Collision"
3. Draw rectangles around maze walls (tile=1 areas)
4. Name objects: "wall_1", "wall_2", etc.
5. Save and test

**Option B**: Find tmxlite example map
1. Check tmxlite repository examples for isometric map with objects
2. Copy to assets/ folder
3. Verify it loads correctly

**Test Map Requirements**:
- Isometric orientation
- At least one object layer with rectangles
- Sized appropriately for 976x976 world

---

### Phase 9: CMakeLists.txt Updates
**Goal**: Add new source files to build

**Modified File**: `CMakeLists.txt`

Add to Server:
```cmake
add_executable(Server
    ...
    src/CollisionSystem.cpp
)
```

Add to Client:
```cmake
add_executable(Client
    ...
    src/CollisionSystem.cpp
    src/CollisionDebugRenderer.cpp
)
```

---

## File Changes Summary

### New Files (4)
```
include/CollisionShape.h             (collision data structures)
include/CollisionSystem.h            (collision detection logic)
src/CollisionSystem.cpp              (collision implementation)
include/CollisionDebugRenderer.h     (debug visualization)
src/CollisionDebugRenderer.cpp       (debug rendering)
```

### Modified Files (11)
```
include/TiledMap.h                   (add collision shapes)
src/TiledMap.cpp                     (extract object layers)
include/Player.h                     (add collision parameter)
include/ServerGameState.h            (add collision system)
src/ServerGameState.cpp              (use collision in movement)
src/server_main.cpp                  (instantiate collision system)
include/ClientPrediction.h           (add collision system)
src/ClientPrediction.cpp             (use collision in prediction)
src/client_main.cpp                  (instantiate collision + debug)
include/RenderSystem.h               (add debug renderer)
src/RenderSystem.cpp                 (render debug overlay)
include/InputSystem.h                (add debug toggle)
src/InputSystem.cpp                  (F1 key handler)
CMakeLists.txt                       (add new sources)
```

### Modified/Created Assets (1)
```
assets/test_map.tmx                  (add Collision object layer)
```

---

## Testing Strategy

### Phase-by-Phase Testing

**After Phase 2**:
- Build succeeds
- Log shows "Loaded N collision shapes" (N > 0)
- No crashes

**After Phase 3**:
- Unit test: AABB.intersects() works correctly
- Unit test: CollisionSystem.checkMovement() blocks movement
- Collision system instantiates without errors

**After Phase 6**:
- Run `/dev` (1 server + 4 clients)
- Players cannot walk through walls
- Movement feels responsive (no stuttering)
- Client prediction and server stay in sync

**After Phase 7**:
- Press F1 to toggle collision overlay
- Red rectangles appear around walls
- Overlay moves correctly with camera
- Overlay shows in isometric perspective

### Integration Testing
1. Build: `make build`
2. Run: `make dev`
3. Test collision: Try walking through walls (should be blocked)
4. Test sliding: Walk diagonally into wall (should slide along)
5. Test debug: Press F1 (should show red collision boxes)
6. Test multiplayer: All 4 clients see same collision behavior
7. Verify logs: Check for collision shape count on startup

---

## Critical Design Decisions

1. **Collision on both sides**: Client prediction and server authority both use identical collision code (shared `applyInput()` function) to prevent prediction errors

2. **Simple AABB for MVP**: Start with axis-aligned bounding boxes (rectangles). Polygon support can be added later without changing API

3. **Sliding collision**: When blocked, try moving on single axis (X or Y only) to allow sliding along walls for better game feel

4. **Debug visualization**: F1 toggle makes collision debugging fast without recompilation (Tiger Style)

5. **Player as circle**: Use PLAYER_RADIUS = 16.0f to approximate 32x32 player as circle for simpler collision math

6. **No spatial partitioning yet**: O(N) collision check is fine for MVP. Add quadtree/grid if profiling shows bottleneck

---

## Future Extensions

- **Polygon collision**: Support arbitrary shapes from Tiled
- **Trigger zones**: Object type="trigger" for events (spawn points, level transitions)
- **One-way platforms**: Special collision logic
- **Moving platforms**: Dynamic collision shapes
- **Pathfinding**: Use collision map for A* navigation
