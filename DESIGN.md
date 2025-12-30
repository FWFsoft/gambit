# Requirements

## Core Vision

* **A game engine for building 2D isometric ARPG games**
  * Potential for RTS game support (similar perspective and map design)
  * Click-to-move system with pathfinding (useful for both player movement and enemy AI)

## Performance & Quality

* **Focus on debuggability, stability, and performance**
  * Builds, boots, and loads very fast
  * [Tiger Style development](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md)
    * Assert on negative space, crashing the application early
    * Extensive fuzz testing
  * Full debugger support

## Developer Experience

* **"High-Code" philosophy**
  * Avoid abstraction layers that hide fundamental programming concepts
  * APIs over UIs
  * Direct control over performance-critical code

* **No Hot Reload**
  * Fast build/boot/load cycle eliminates need for hot reload
  * Consider save-states for rapid iteration

## Asset Pipeline

* **Map Editor**: Tiled integration
  * Import maps with custom metadata
  * Support for invisible game objects (interactables, spawners, etc.)
  * Fast iteration: edit in Tiled, save, rebuild, test

* **Sprite Assets**: Aseprite integration
  * Seamless sprite sheet importing
  * Define sprites, hitboxes, and hurtboxes in Aseprite

* **UI Builder**: [Rive](https://rive.app/) integration

## Technical Requirements

* **Physics Engine**: Optional (use library if needed)

* **Networking**
  * 4-player co-op PvE
  * Matchmaking support
  * Client-server architecture with prediction and interpolation

* **Platform Support**
  * Primary: PC (Windows/Linux/macOS)
  * Other platforms: nice to have

* **Scene System**
  * Scene Graph for environment changes and map transitions
  * Event-driven scene traversal

## Nice-to-Have Features

* Foreground/Background elements with parallax scrolling
* Advanced particle effects
* Dynamic lighting

# Design Decisions

## Technology Stack

* **Programming Language**: C++17
* **Build System**: CMake + vcpkg for dependency management
* **Graphics**: SDL2 + OpenGL
* **Networking**: ENet (UDP-based, reliable)
  * Abstract network layer for future changes
  * Client-server architecture (started with P2P capability)
  * Steamworks API for lobbying and matchmaking
* **Logging**: spdlog (fast, structured logging)

## Asset Pipeline

* **Level Editor**: Tiled
  * Import maps with custom metadata
  * Invisible game objects defined via Tiled metadata
  * Behavior properties (visibility, interactivity, etc.)

* **Sprite Pipeline**: Aseprite
  * Seamless import of sprite sheets
  * Designers define sprites, hitboxes, and hurtboxes in Aseprite
  * Alignment techniques:
    1. Identical canvas sizes with center-point alignment
    2. Anchor points via hex color codes

* **UI Builder**: [Rive](https://rive.app/)

## Architecture Principles

### Event-Based Architecture

Games built on the engine operate over an **asynchronous event-based architecture** rather than traditional update/render loops.

**Why Event-Driven?**
* Avoids coupling to engine update/render loops
* No "MonoBehavior" pattern - developers aren't forced into engine lifecycle management
* Subscribe only to events you need, when you need them
* Better separation of concerns
* Easier testing and debugging

### Fixed 60 FPS Game Loop

* **Update loop**: ALWAYS fires at 60 FPS (16.67ms intervals)
  * Deterministic gameplay
  * Consistent physics simulation
  * Predictable network behavior

* **Render loop**: Variable rate, drops frames if needed
  * If frame isn't ready in 16ms, skip rendering
  * Gameplay never slows down
  * Maintains smooth experience


# Implementation Status

## Completed Systems

* ✅ **Networking**: ENet-based client-server with binary protocol
* ✅ **Game Loop**: Fixed 60 FPS update loop with event system
* ✅ **Event Bus**: Type-safe publish-subscribe architecture
* ✅ **Input System**: WASD keyboard input with state tracking
* ✅ **Rendering**: OpenGL-based isometric tile rendering
* ✅ **Combat**: Player health, damage, and respawn system
* ✅ **Enemies**: Enemy spawning and basic AI
* ✅ **UI**: ImGui integration for settings and HUD
* ✅ **Music**: Background music with combat layer support
* ✅ **Items**: Item registry with CSV loading
* ✅ **Client Prediction**: Instant local response with server reconciliation
* ✅ **Remote Interpolation**: Smooth remote player movement

## In Development

* 🚧 **Asset Pipeline**
  * Tiled integration with custom metadata
  * Aseprite sprite sheet importing
  * Rive UI integration

## Future Enhancements

* **AI-Driven Asset Pipeline**
  * Procedural generation tools
  * AI-assisted sprite creation
  * Automated asset optimization

* **Audio**
  * Sound effects system
  * Voice over support (consider ElevenLabs)
  * 3D spatial audio

* **Advanced Gameplay**
  * More complex enemy AI
  * Advanced combat mechanics
  * Skill/ability system
  * Inventory management
