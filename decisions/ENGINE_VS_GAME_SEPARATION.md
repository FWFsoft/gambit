# Engine vs Game Separation Plan

## Overview

This document outlines the strategy for separating the Gambit game engine from the demo game currently hosted in this repository. The goal is to create a clear distinction between reusable engine components and game-specific logic, enabling the engine to be used for multiple games while maintaining the current demo as a reference implementation.

## Current State

Currently, the codebase intermingles engine and game logic:

- **Engine-like components**: Window management, rendering, networking, ECS, collision, animation
- **Game-specific components**: Player mechanics, spawn logic, multiplayer game state, music system, specific map files
- **Shared/ambiguous**: Configuration values, asset management, input handling

## Goals

1. **Engine reusability**: Extract core engine functionality that can be used across multiple games
2. **Clear boundaries**: Establish well-defined interfaces between engine and game code
3. **Maintain demo**: Keep the current 4-player co-op demo as a reference implementation
4. **Minimal disruption**: Design the separation to minimize breaking changes

## Proposed Architecture

### Engine Components (Core - Reusable)

These components should live in the engine layer and provide generic functionality:

#### **Core Systems**
- `Window` - SDL window management
- `EventBus` - Event-driven architecture
- `GameLoop` - Fixed 60 FPS update loop
- `Logger` - Logging infrastructure
- `FileSystem` - File utilities

#### **Rendering**
- `SpriteRenderer` - Sprite rendering with shaders
- `TileRenderer` - Isometric tile rendering
- `Texture` / `TextureManager` - Texture loading and management
- `Camera` - Camera transformation (world ↔ screen)
- `OpenGLUtils` - OpenGL helper functions

#### **Networking**
- `NetworkClient` / `NetworkServer` - ENet wrappers
- `NetworkProtocol` - Packet serialization/deserialization
- Generic snapshot/reconciliation framework

#### **Animation**
- `AnimationController` - Animation state machine
- `AnimationClip` / `AnimationFrame` - Animation data structures
- `AnimationSystem` - Animation update system
- `Animatable` - Interface for animated entities

#### **Collision**
- `CollisionSystem` - Generic AABB and polygon collision
- `CollisionShape` / `AABB` - Collision data structures

#### **Map/Tilemap**
- `TiledMap` - Tiled TMX loader with generic metadata extraction
- Generic tilemap rendering

#### **Audio**
- `MusicSystem` - Zone-based and event-based music (generalized)
- Sound effect system (future)

#### **Config System**
- `config/` namespace - Centralized configuration
- Config loader (future: JSON/TOML-based)

---

### Game Components (Demo - Game-Specific)

These components implement game-specific logic and should live in a separate "game" layer:

#### **Game Entities**
- `Player` - Player entity with health, movement, color
- `applyInput()` - Player movement logic with world bounds
- Player spawn position finding algorithm

#### **Game State**
- `ServerGameState` - 4-player co-op server logic
- `ClientPrediction` - Client-side prediction for players
- `RemotePlayerInterpolation` - Remote player smoothing
- Player color assignment (4-player colors)

#### **Game Config**
- `PlayerConfig` - Player-specific constants (speed, health, colors, size)
- `GameplayConfig` - Spawn search, prediction thresholds
- Game-specific screen size (800x600)

#### **Game Assets**
- `assets/test_map.tmx` - Specific map file
- `assets/player_animated.png` - Player sprite sheet
- `assets/music/` - Music files
- `AnimationAssetLoader::loadPlayerAnimations()` - Game-specific sprite layout

#### **Game UI/Debug**
- `InputSystem` - F1 debug toggles (game-specific controls)
- `CollisionDebugRenderer` - Debug visualization
- `MusicZoneDebugRenderer` - Music zone visualization

#### **Game Entry Points**
- `client_main.cpp` - Client bootstrap logic
- `server_main.cpp` - Server bootstrap logic

---

## Separation Strategy

### Phase 1: Establish Boundaries (No Code Movement)

**Goal**: Identify and document what belongs where without moving files.

1. **Create `COMPONENTS.md`** documenting:
   - Engine components (what they are, public API)
   - Game components (what they depend on)
   - Dependency graph (game → engine only)

2. **Enforce layering rules**:
   - Engine code CANNOT depend on game code
   - Game code CAN depend on engine code
   - Create `include/engine/` and `include/game/` directories (future)

3. **Identify coupling points**:
   - Where does engine code assume game-specific behavior?
   - Where is game logic embedded in engine systems?

**Action items**:
- Audit `Player.h` - Does it belong in engine or game?
- Audit `MovementInput` - Engine interface or game-specific?
- Audit `WorldConfig` - Engine config or game-specific?
- Review config headers - Which are engine vs game?

---

### Phase 2: Extract Game-Specific Logic

**Goal**: Move game-specific logic out of engine components.

1. **Generalize engine interfaces**:
   - `MusicSystem` → Accept generic "trigger zones" instead of `MusicZone`
   - `CollisionSystem` → Already generic ✅
   - `AnimationAssetLoader` → Make sprite layout data-driven (JSON metadata)

2. **Create game layer**:
   - New directory: `src/game/` for game-specific implementations
   - Move `ServerGameState`, `ClientPrediction`, `RemotePlayerInterpolation` to `src/game/`
   - Move `Player` and `applyInput()` to `src/game/`

3. **Introduce game interface**:
   - Create `include/engine/Game.h` with pure virtual interface:
     ```cpp
     class Game {
       virtual void onClientConnected(ClientId id) = 0;
       virtual void onClientInput(ClientId id, const InputPacket& input) = 0;
       virtual void onUpdate(float deltaTime) = 0;
       virtual void onRender(RenderContext& ctx) = 0;
     };
     ```
   - Demo game implements `DemoGame : public Game`

**Action items**:
- Extract player logic to `src/game/PlayerEntity.cpp`
- Create `src/game/DemoGame.cpp` implementing engine's `Game` interface
- Refactor `client_main.cpp` / `server_main.cpp` to instantiate `DemoGame`

---

### Phase 3: Configuration Separation

**Goal**: Separate engine config from game config.

1. **Engine config** (`include/engine/config/`):
   - `EngineConfig.h` - Rendering, networking, logging
   - `RenderConfig.h` - Screen dimensions, ortho projection
   - `NetworkConfig.h` - Max clients, channels, timeout
   - `AnimationConfig.h` - Generic animation timings

2. **Game config** (`include/game/config/`):
   - `DemoGameConfig.h` - Game-specific values
   - `PlayerConfig.h` - Player speed, health, colors
   - `GameplayConfig.h` - Spawn search, prediction

3. **Config loader**:
   - Engine reads from `engine_config.json`
   - Game reads from `game_config.json`
   - Hot-reload support (future)

**Action items**:
- Split existing `include/config/` into `engine/config/` and `game/config/`
- Move player/gameplay configs to game layer
- Create JSON config files (optional for now)

---

### Phase 4: Asset Management Separation

**Goal**: Game-specific assets are loaded by game layer, engine provides generic loaders.

1. **Engine asset loaders**:
   - `TextureLoader` - Generic PNG/image loading
   - `TiledMapLoader` - Generic TMX loading
   - `MusicLoader` - Generic OGG/WAV/MP3 loading
   - `AnimationLoader` - Load from JSON metadata

2. **Game asset definitions**:
   - `assets/game/` directory for demo game assets
   - `assets/game/player.json` - Player sprite sheet metadata
   - `assets/game/animations.json` - Animation definitions
   - `assets/game/maps/` - Game-specific maps

3. **Asset path configuration**:
   - Engine uses virtual paths like `"textures/player"`
   - Game maps virtual paths to real paths via `AssetManifest`

**Action items**:
- Create `assets/game/` directory
- Move `test_map.tmx`, `player_animated.png`, `music/` to `assets/game/`
- Create JSON metadata for animations
- Implement `AnimationLoader::fromJSON()`

---

### Phase 5: Directory Restructure

**Goal**: Physically separate engine from game code in the file system.

**Proposed structure**:
```
gambit/
├── engine/
│   ├── include/
│   │   ├── Window.h
│   │   ├── EventBus.h
│   │   ├── SpriteRenderer.h
│   │   └── config/
│   │       ├── RenderConfig.h
│   │       └── NetworkConfig.h
│   └── src/
│       ├── Window.cpp
│       ├── EventBus.cpp
│       └── SpriteRenderer.cpp
├── game/
│   ├── include/
│   │   ├── DemoGame.h
│   │   ├── PlayerEntity.h
│   │   └── config/
│   │       ├── PlayerConfig.h
│   │       └── GameplayConfig.h
│   └── src/
│       ├── DemoGame.cpp
│       ├── PlayerEntity.cpp
│       ├── client_main.cpp
│       └── server_main.cpp
├── assets/
│   ├── engine/       # Engine placeholder assets
│   └── game/         # Demo game assets
└── CMakeLists.txt
```

**Alternative (less disruptive)**:
```
gambit/
├── include/
│   ├── engine/       # Engine headers
│   └── game/         # Game headers
├── src/
│   ├── engine/       # Engine implementation
│   └── game/         # Game implementation
├── assets/
│   └── game/         # All game assets
└── CMakeLists.txt
```

**Action items**:
- Choose directory structure (vote: less disruptive alternative)
- Create `include/engine/` and `include/game/` directories
- Move headers to appropriate directories
- Update `#include` paths across codebase
- Update CMakeLists.txt with new paths

---

### Phase 6: Build System Separation

**Goal**: Build engine as a library, game as an executable linking to engine.

**CMakeLists.txt structure**:
```cmake
# Engine library
add_library(GambitEngine STATIC
  src/engine/Window.cpp
  src/engine/EventBus.cpp
  src/engine/SpriteRenderer.cpp
  # ... all engine sources
)

target_include_directories(GambitEngine PUBLIC include/engine)
target_link_libraries(GambitEngine PUBLIC SDL2 ENet OpenGL ...)

# Demo game executable
add_executable(DemoGameClient
  src/game/client_main.cpp
  src/game/DemoGame.cpp
  src/game/PlayerEntity.cpp
  # ... all game sources
)

target_include_directories(DemoGameClient PRIVATE include/game)
target_link_libraries(DemoGameClient PRIVATE GambitEngine)

add_executable(DemoGameServer
  src/game/server_main.cpp
  src/game/DemoGame.cpp
  src/game/PlayerEntity.cpp
)

target_link_libraries(DemoGameServer PRIVATE GambitEngine)
```

**Benefits**:
- Engine can be built as a static library
- Multiple games can link to the same engine
- Enforces dependency direction (game → engine)
- Enables separate testing of engine components

**Action items**:
- Refactor CMakeLists.txt to create `GambitEngine` library
- Link client/server executables to `GambitEngine`
- Ensure no circular dependencies

---

### Phase 7: API Stabilization

**Goal**: Define stable public API for engine, hide implementation details.

1. **Public engine headers**:
   - Create `include/engine/Gambit.h` - Single include for all engine APIs
   - Mark internal headers as `detail/` or `impl/`

2. **Namespace separation**:
   - Engine code in `namespace gambit::engine`
   - Game code in `namespace demogame`

3. **ABI stability**:
   - Use PIMPL pattern for engine classes (future)
   - Avoid exposing STL types in public API (future)

4. **Documentation**:
   - Doxygen comments for public engine API
   - Examples for each engine system
   - Migration guide for existing games

**Action items**:
- Add namespaces to engine code
- Create `Gambit.h` master header
- Write API documentation (markdown or Doxygen)

---

## Migration Challenges

### Challenge 1: Player Entity Ambiguity

**Problem**: `Player` struct is used by both engine (networking, animation) and game (health, colors).

**Solution**:
- Engine defines `Entity` interface with ID, position, velocity
- Engine networking operates on `Entity*` or `EntitySnapshot`
- Game defines `PlayerEntity : public Entity` with game-specific fields
- Serialization uses `EntitySnapshot` struct (POD) for network packets

**Code**:
```cpp
// Engine - include/engine/Entity.h
class Entity {
  virtual uint32_t getId() const = 0;
  virtual glm::vec2 getPosition() const = 0;
  virtual glm::vec2 getVelocity() const = 0;
  virtual AnimationController* getAnimationController() = 0;
};

// Game - include/game/PlayerEntity.h
class PlayerEntity : public Entity {
  uint32_t id;
  float x, y;
  float vx, vy;
  float health;  // Game-specific
  uint8_t r, g, b;  // Game-specific
  // ...
};
```

---

### Challenge 2: Config Value Ownership

**Problem**: Some config values (like screen size) are used by both engine and game.

**Solution**:
- Engine defines `RenderConfig` with screen size, ortho projection
- Game can override values via `Game::getConfig()`
- Engine reads defaults, game overrides specific values

**Code**:
```cpp
// Engine
struct RenderConfig {
  int screenWidth = 800;
  int screenHeight = 600;
  // ... engine rendering settings
};

// Game can override
RenderConfig DemoGame::getRenderConfig() {
  RenderConfig config;
  config.screenWidth = 800;  // Demo game wants 800x600
  config.screenHeight = 600;
  return config;
}
```

---

### Challenge 3: Asset Path Resolution

**Problem**: Engine needs to load assets, but game controls where assets are stored.

**Solution**:
- Engine uses virtual asset paths: `"textures/player"`
- Game provides `AssetManifest` mapping virtual → real paths
- Engine's `AssetLoader` resolves paths via manifest

**Code**:
```cpp
// Game provides manifest
AssetManifest DemoGame::getAssetManifest() {
  AssetManifest manifest;
  manifest.addTexture("player", "assets/game/player_animated.png");
  manifest.addMap("test_map", "assets/game/maps/test_map.tmx");
  manifest.addMusic("forest", "assets/game/music/forest.ogg");
  return manifest;
}

// Engine uses virtual paths
Texture* tex = TextureManager::instance().get("player");  // Resolved via manifest
```

---

## Timeline and Phases Summary

| Phase | Effort | Risk | Dependencies |
|-------|--------|------|--------------|
| **Phase 1**: Establish Boundaries | Low | Low | None |
| **Phase 2**: Extract Game Logic | Medium | Medium | Phase 1 |
| **Phase 3**: Config Separation | Low | Low | Phase 1 |
| **Phase 4**: Asset Management | Medium | Medium | Phase 2 |
| **Phase 5**: Directory Restructure | Medium | High | Phases 2, 3 |
| **Phase 6**: Build System Separation | Low | Medium | Phase 5 |
| **Phase 7**: API Stabilization | Medium | Low | Phase 6 |

**Recommended order**: 1 → 2 → 3 → 4 → 5 → 6 → 7

**When to start**: After demo game is feature-complete (music, combat, etc.)

---

## Success Criteria

The separation is successful when:

1. ✅ **Engine is reusable**: A new game can be created by implementing `Game` interface
2. ✅ **No circular dependencies**: Game depends on engine, engine does NOT depend on game
3. ✅ **Build separation**: Engine builds as a library, games link to it
4. ✅ **Asset isolation**: Game assets are in `assets/game/`, engine has no hardcoded paths
5. ✅ **Config separation**: Engine config vs game config are clearly separated
6. ✅ **Demo still works**: Existing 4-player co-op demo works identically after separation
7. ✅ **Tests pass**: All unit tests and integration tests still pass

---

## Future Considerations

### Multi-Game Support

Once separation is complete, creating a new game should be as simple as:

```cpp
// new_game/src/NewGame.cpp
class NewGame : public gambit::engine::Game {
  void onClientConnected(ClientId id) override {
    // Custom logic
  }
  // ... implement other methods
};

// new_game/src/main.cpp
int main() {
  gambit::engine::Engine engine;
  NewGame game;
  engine.run(&game);
}
```

### Plugin System

Future: Support game logic as plugins/DLLs that engine loads at runtime.

### Editor Integration

Future: Level editor, asset pipeline, visual scripting - all operate on engine APIs.

---

## Conclusion

This separation plan provides a roadmap for extracting the Gambit engine from the demo game. The phased approach minimizes disruption while establishing clear boundaries between reusable engine code and game-specific logic.

**Recommendation**: Start with Phase 1 (documentation) immediately, defer implementation phases until the demo game is feature-complete.
