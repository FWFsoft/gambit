# Tiled Map Integration with Isometric Camera System

## Goal
Integrate Tiled map editor with isometric rendering (like Hades), implement camera system that follows local player with map bounds clamping, and support maps much larger than the 800x600 window.

## Requirements
- **Map Format**: TMX (XML) using tmxlite library
- **Orientation**: Isometric (2.5D diamond grid)
- **Map Size**: Much larger than screen (e.g., 30x30 tiles @ 64x32 = ~1920x960 world)
- **Camera**: Basic following (centers on local player) + map bounds clamping (no black void)

## Current State
- World bounds: 800x600 (hardcoded in `Player.h` lines 35-36)
- Rendering: Direct screen coordinates, no camera transform (`RenderSystem.cpp`)
- Players: 32x32 colored rectangles at positions (x, y)

---

## Implementation Plan

### ✅ Phase 1: Integrate tmxlite Library (COMPLETED)
**Goal**: Add tmxlite as dependency.

**Steps**:
1. ✅ Created `vcpkg.json` manifest with tmxlite dependency
2. ✅ Updated `CMakeLists.txt` to use pkg-config for tmxlite
3. ✅ Updated `GNUmakefile` to use vcpkg toolchain
4. ✅ Updated GitHub Actions to use `lukka/run-vcpkg@v11`

**Testing**: ✅ `/build` succeeds, no errors

---

### ✅ Phase 2: Camera System Foundation (COMPLETED)
**Goal**: Create camera that follows player and transforms world→screen coordinates.

**New Files**:
- `include/Camera.h` - Camera class with isometric support

**Camera Class**:
```cpp
struct Camera {
  float x, y;              // Camera world position (center)
  float worldWidth, worldHeight;
  int screenWidth, screenHeight;
  bool isometric;

  Camera(int screenW, int screenH);

  void follow(float targetX, float targetY);
  void setWorldBounds(float w, float h);
  void setIsometric(bool iso);

  // Transform world coords to screen coords
  void worldToScreen(float worldX, float worldY, int& screenX, int& screenY) const;

private:
  void clampToWorldBounds();  // Prevents black void at edges
};
```

**Isometric Transform** (in `worldToScreen`):
```cpp
if (isometric) {
  float isoX = worldX - worldY;
  float isoY = (worldX + worldY) * 0.5f;
  screenX = static_cast<int>(isoX - x + screenWidth / 2);
  screenY = static_cast<int>(isoY - y + screenHeight / 2);
}
```

**Modified Files**:
- `include/RenderSystem.h` - Add `Camera* camera` member, update constructor
- `src/RenderSystem.cpp`:
  - Update constructor to accept Camera
  - In `onRender()`: call `camera->follow(localPlayer.x, localPlayer.y)`
  - In `drawPlayer()`: use `camera->worldToScreen()` instead of direct coords
- `src/client_main.cpp` - Instantiate Camera, pass to RenderSystem

**Testing**: Camera follows local player, works with 800x600 world (no visual change yet)

---

### ✅ Phase 3: Tiled Map Loading (COMPLETED)
**Goal**: Load TMX files and extract map metadata.

**New Files**:
- `include/TiledMap.h` - Map loading wrapper
- `src/TiledMap.cpp` - Implementation
- `assets/test_map.tmx` - Test isometric map (create in Tiled)

**TiledMap Class**:
```cpp
class TiledMap {
public:
  bool load(const std::string& filepath);

  int getWidth() const;    // In tiles
  int getHeight() const;   // In tiles
  int getTileWidth() const;  // In pixels
  int getTileHeight() const; // In pixels
  int getWorldWidth() const;   // In pixels (for isometric: calculated)
  int getWorldHeight() const;  // In pixels
  bool isIsometric() const;
  const std::vector<const tmx::TileLayer*>& getTileLayers() const;

private:
  tmx::Map tmxMap;
  bool isometric;
  int mapWidth, mapHeight, tileWidth, tileHeight;
  std::vector<const tmx::TileLayer*> tileLayers;
};
```

**Test Map Specs** (create in Tiled):
- Orientation: Isometric
- Map size: 30x30 tiles
- Tile size: 64x32 pixels
- Add 1-2 tile layers with simple colored tiles
- Save as `assets/test_map.tmx`

**Modified Files**:
- `src/client_main.cpp`:
  - Load map: `TiledMap map; map.load("assets/test_map.tmx");`
  - Configure camera: `camera.setIsometric(true); camera.setWorldBounds(map.getWorldWidth(), map.getWorldHeight());`

**Testing**: Map loads, dimensions logged, assertions fire if file missing

---

### ✅ Phase 4: Isometric Tile Rendering (COMPLETED)
**Goal**: Render isometric tiles from map.

**New Files**:
- `include/TileRenderer.h` - Tile rendering logic
- `src/TileRenderer.cpp` - Implementation

**TileRenderer Class**:
```cpp
class TileRenderer {
public:
  TileRenderer(SDL_Renderer* renderer, Camera* camera);
  void render(const TiledMap& map);

private:
  void renderTile(const TiledMap& map, int tileX, int tileY, uint32_t gid);
  void gridToWorld(const TiledMap& map, int tileX, int tileY,
                   float& worldX, float& worldY);
};
```

**Grid to World Transform** (in `gridToWorld`):
```cpp
if (map.isIsometric()) {
  worldX = (tileX - tileY) * (tileWidth / 2.0f);
  worldY = (tileX + tileY) * (tileHeight / 2.0f);
}
```

**Rendering Strategy** (for now):
- Render colored rectangles as placeholder tiles
- Back-to-front row order (top-left to bottom-right)
- Use `camera->worldToScreen()` for each tile

**Modified Files**:
- `src/RenderSystem.cpp`:
  - Add `TileRenderer* tileRenderer` and `TiledMap* tiledMap` members
  - In `onRender()`, before drawing players: `tileRenderer->render(*tiledMap);`
- `src/client_main.cpp` - Instantiate TileRenderer
- `CMakeLists.txt` - Add `src/TileRenderer.cpp` to Client sources

**Testing**: Tiles render in diamond pattern, camera scrolls map

---

### ✅ Phase 5: Depth Sorting (Players with Tiles) (COMPLETED)
**Goal**: Interleave players with tiles based on Y depth.

**Algorithm**:
```cpp
// In TileRenderer::render()
for (int tileY = 0; tileY < mapHeight; ++tileY) {
  // Render all tiles in this row
  for (int tileX = 0; tileX < mapWidth; ++tileX) {
    renderTile(tileX, tileY);
  }

  // Render players whose Y position falls in this row's depth range
  float rowMinDepth = tileY * (tileHeight / 2.0f);
  float rowMaxDepth = (tileY + 1) * (tileHeight / 2.0f);
  for (const Player& player : playersInRange(rowMinDepth, rowMaxDepth)) {
    renderPlayer(player);
  }
}
```

**Modified Files**:
- `include/TileRenderer.h` - Add `PlayerRenderCallback` typedef
- `src/TileRenderer.cpp` - Update `render()` to accept callback, invoke for each row
- `src/RenderSystem.cpp` - Collect all players (local + remote), pass callback to TileRenderer

**Testing**: Players appear behind/in-front of tiles correctly based on Y position

---

### ✅ Phase 6: Dynamic World Bounds (COMPLETED)
**Goal**: Replace hardcoded 800x600 with map-based bounds.

**Modified Files**:
- `include/Player.h`:
  - Remove: `constexpr float WORLD_WIDTH = 800.0f; constexpr float WORLD_HEIGHT = 600.0f;`
  - Update `applyInput()` signature to accept bounds: `inline void applyInput(Player& player, bool moveLeft, bool moveRight, bool moveUp, bool moveDown, float deltaTime, float worldWidth, float worldHeight)`
  - Update clamping: `player.x = std::clamp(player.x, 0.0f, worldWidth);`

- `include/ClientPrediction.h` - Add `worldWidth`, `worldHeight` members, update constructor
- `src/ClientPrediction.cpp` - Pass bounds to `applyInput()`

- `include/ServerGameState.h` - Add `worldWidth`, `worldHeight` members, update constructor
- `src/ServerGameState.cpp`:
  - Pass bounds to `applyInput()`
  - Clamp spawned player positions to map bounds

- `src/server_main.cpp`:
  - Load map: `TiledMap serverMap; serverMap.load("assets/test_map.tmx");`
  - Pass bounds to ServerGameState

- `src/client_main.cpp` - Pass map bounds to ClientPrediction constructor

**Testing**: Players clamp at map edges, camera stops at boundaries

---

### Phase 7: Polish & Optimization
**Goal**: Add assertions, frustum culling, debug visualization.

**Enhancements**:
1. **Frustum Culling** - Only render visible tiles (in `TileRenderer::render()`)
2. **Tiger Style Assertions**:
   - `assert(map.load(...) && "Failed to load required map");`
   - `assert(player.x >= 0 && player.x <= worldWidth);`
3. **Performance Logging** - Warn if tile rendering > 5ms
4. **Debug Overlay** (F1 toggle) - Camera bounds, grid lines

**Testing**: 60 FPS maintained, no crashes for 5+ minutes

---

## File Structure

### New Files (7 + assets)
```
external/tmxlite/               (git submodule)
include/Camera.h                (camera system)
include/TiledMap.h              (map loading)
include/TileRenderer.h          (tile rendering)
src/TiledMap.cpp
src/TileRenderer.cpp
assets/test_map.tmx             (isometric test map - create in Tiled)
assets/tileset.png              (placeholder tileset image)
```

### Modified Files (9)
```
CMakeLists.txt                  (add tmxlite, TileRenderer.cpp)
include/Player.h                (dynamic world bounds)
include/RenderSystem.h          (add Camera, TileRenderer)
src/RenderSystem.cpp            (camera transform, tile rendering, depth sort)
include/ClientPrediction.h      (world bounds)
src/ClientPrediction.cpp        (pass bounds to applyInput)
include/ServerGameState.h       (world bounds)
src/ServerGameState.cpp         (pass bounds, clamp spawns)
src/client_main.cpp             (load map, create Camera/TileRenderer)
src/server_main.cpp             (load map for bounds)
```

---

## Critical Code Snippets

### Isometric Projection
```cpp
// World (cartesian) → Screen (isometric diamond)
void Camera::worldToScreen(float worldX, float worldY, int& screenX, int& screenY) {
  if (isometric) {
    float isoX = worldX - worldY;
    float isoY = (worldX + worldY) * 0.5f;
    screenX = static_cast<int>(isoX - x + screenWidth / 2);
    screenY = static_cast<int>(isoY - y + screenHeight / 2);
  } else {
    screenX = static_cast<int>(worldX - x + screenWidth / 2);
    screenY = static_cast<int>(worldY - y + screenHeight / 2);
  }
}
```

### Grid to World
```cpp
// Isometric grid coords → Cartesian world coords
void gridToWorld(int tileX, int tileY, float& worldX, float& worldY) {
  worldX = (tileX - tileY) * (tileWidth / 2.0f);
  worldY = (tileX + tileY) * (tileHeight / 2.0f);
}
```

### Camera Bounds Clamping
```cpp
void Camera::clampToWorldBounds() {
  float halfW = screenWidth / 2.0f;
  float halfH = screenHeight / 2.0f;

  if (worldWidth <= screenWidth) {
    x = worldWidth / 2.0f;  // Center if world smaller than screen
  } else {
    x = std::clamp(x, halfW, worldWidth - halfW);
  }

  if (worldHeight <= screenHeight) {
    y = worldHeight / 2.0f;
  } else {
    y = std::clamp(y, halfH, worldHeight - halfH);
  }
}
```

---

## Testing Strategy

**Per-Phase Testing**:
- Phase 1: ✅ `/build` succeeds
- Phase 2: ✅ Camera follows player (800x600 world still)
- Phase 3: Map loads, dimensions logged
- Phase 4: Isometric tiles render in diamond pattern
- Phase 5: Players behind/in-front of tiles correctly
- Phase 6: Movement clamps at map edges
- Phase 7: 60 FPS maintained

**Integration Testing** (`/dev`):
1. Build project
2. Server loads map for bounds
3. 4 clients load map and render tiles
4. Each player can explore full map
5. Camera follows local player smoothly
6. No black void at map edges
7. Depth sorting works correctly
8. Runs for 5+ minutes without crashes

---

## Key Design Decisions

1. **Players stay in cartesian coords** - Only rendering transforms to isometric. Movement stays simple (WASD = cardinal directions).

2. **Server loads map too** - Both client and server load same TMX file for world bounds agreement. No network transfer needed.

3. **Back-to-front row rendering** - Simple depth sort for isometric. More sophisticated algorithms possible later.

4. **No texture rendering in MVP** - Use colored rectangles for tiles initially. Add texture support in follow-up phase.

5. **No hot reload** - Fast build/boot cycle makes this unnecessary (Tiger Style).

---

## Future Enhancements (Post-MVP)

- **Phase 8**: Texture rendering (replace colored tiles with actual tileset images)
- **Phase 9**: Object layers (spawn points, collision areas, triggers)
- **Phase 10**: Parallax layers (background/foreground depth)
- **Phase 11**: Animated tiles (water, torches)
- **Phase 12**: Map streaming (load/unload chunks for huge maps)
