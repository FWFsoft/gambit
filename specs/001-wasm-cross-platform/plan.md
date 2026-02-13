# Implementation Plan: WebAssembly Cross-Platform Support

**Branch**: `001-wasm-cross-platform` | **Date**: 2026-01-29 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-wasm-cross-platform/spec.md`

## Summary

Port the Gambit C++17 game engine to WebAssembly via Emscripten so the isometric 2D game can run in modern browsers. This requires: a transport layer abstraction (replacing direct ENet usage), cross-platform shader support (OpenGL 3.3 ↔ WebGL2), main loop restructuring for browser compatibility, an embedded server mode for single-process local testing, and CMake build system changes to support both native and WASM targets from the same source tree.

## Technical Context

**Language/Version**: C++17
**Primary Dependencies**: SDL2 (Emscripten port), OpenGL/WebGL2, ENet (native), Emscripten WebSocket API (WASM), spdlog, ImGui, tmxlite, glm
**Storage**: Emscripten virtual filesystem (MEMFS) for bundled assets; native filesystem unchanged
**Testing**: ctest (native), headless browser execution (WASM)
**Target Platform**: Native (macOS, Linux, Windows) + WebAssembly (Chrome, Firefox, Safari, Edge)
**Project Type**: Single project, dual build targets
**Performance Goals**: Fixed 60 FPS update loop on both platforms; 100+ sprites at 60 FPS in browser
**Constraints**: WASM binary < 20MB compressed; WebSocket-only networking in browser; single-threaded JS environment
**Scale/Scope**: 4-player co-op multiplayer; mixed native + browser client sessions

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. AI-First Development | PASS | All new code is programmatic (transport interfaces, build targets). No GUI-dependent workflows added. |
| II. Fast Build/Boot/Load | PASS | WASM build uses Emscripten's incremental compilation. Asset bundling via `--preload-file` is a build step, not runtime. |
| III. Tiger Style Debuggability | PASS | spdlog routes to browser console. Assertions work in WASM (crash = error in console). Transport abstraction preserves event traceability. |
| IV. Agent-Testable by Default | PASS | Embedded server mode enables self-contained testing. WASM tests can run in headless browser. Existing headless test infrastructure is preserved. |
| V. High-Performance Multiplayer | PASS with note | WebSocket transport is TCP-based (vs ENet's UDP). Latency characteristics differ but are acceptable for 60 FPS update rate. The transport abstraction preserves client prediction and server reconciliation. |
| VI. AI-Generated Asset Pipeline | PASS | Asset pipeline is unchanged. Assets are bundled at build time via `--preload-file` using the same formats. |
| VII. Simplicity & Direct Control | PASS | Transport abstraction solves a concrete problem (cross-platform networking). No speculative abstractions added. `#ifdef __EMSCRIPTEN__` guards are minimal and contained. |

**Gate result**: PASS — no violations.

## Project Structure

### Documentation (this feature)

```text
specs/001-wasm-cross-platform/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Phase 0 research findings
├── data-model.md        # Transport layer data model
├── quickstart.md        # Build and run instructions
├── contracts/
│   ├── transport-interface.h   # INetworkTransport / IServerTransport interfaces
│   ├── build-targets.cmake     # CMake structure contract
│   └── shader-compat.md        # Shader cross-platform rules
└── tasks.md             # Phase 2 output (via /speckit.tasks)
```

### Source Code (repository root)

```text
include/
├── transport/
│   ├── INetworkTransport.h       # Client transport interface
│   ├── IServerTransport.h        # Server transport interface
│   ├── TransportEvent.h          # Unified transport event struct
│   ├── ENetTransport.h           # Native client transport (ENet)
│   ├── ENetServerTransport.h     # Native server transport (ENet)
│   ├── WebSocketTransport.h      # WASM client transport (WebSocket)
│   ├── WebSocketServerTransport.h # Native server WebSocket support
│   ├── InMemoryTransport.h       # Embedded mode client transport
│   └── InMemoryServerTransport.h # Embedded mode server transport
├── InMemoryChannel.h             # Shared queue for embedded mode
├── GameSession.h                 # Embedded mode orchestrator
├── NetworkServer.h               # Modified: uses IServerTransport
├── NetworkClient.h               # Modified: uses INetworkTransport
├── GameLoop.h                    # Modified: extractable tick()
└── ShaderCompat.h                # Cross-platform shader version helpers

src/
├── transport/
│   ├── ENetTransport.cpp
│   ├── ENetServerTransport.cpp
│   ├── WebSocketTransport.cpp        # WASM only
│   ├── WebSocketServerTransport.cpp  # Native only
│   ├── InMemoryTransport.cpp
│   └── InMemoryServerTransport.cpp
├── GameSession.cpp
├── NetworkServer.cpp             # Modified: delegates to transport
├── NetworkClient.cpp             # Modified: delegates to transport
├── GameLoop.cpp                  # Modified: tick() extraction
├── Window.cpp                    # Modified: #ifdef for GLAD/GLES3
├── SpriteRenderer.cpp            # Modified: shader version
├── TileRenderer.cpp              # Modified: shader version
├── CollisionDebugRenderer.cpp    # Modified: shader version
├── MusicZoneDebugRenderer.cpp    # Modified: shader version
├── ObjectiveDebugRenderer.cpp    # Modified: shader version
├── wasm_client_main.cpp          # WASM entry point (embedded + networked modes)
└── client_main.cpp               # Modified: uses transport abstraction

CMakeLists.txt                    # Modified: WASM target + Emscripten conditionals
```

**Structure Decision**: Single project with conditional compilation. The `transport/` subdirectory groups all transport implementations. Platform-specific code is isolated to transport implementations and `#ifdef __EMSCRIPTEN__` guards in 6 renderer/window files. The same `CMakeLists.txt` handles both native and WASM targets.

## Complexity Tracking

No constitution violations — no entries needed.

## Post-Design Constitution Re-Check

| Principle | Status | Delta from Pre-Design |
|-----------|--------|-----------------------|
| I. AI-First | PASS | No change |
| II. Fast Build/Boot | PASS | No change |
| III. Tiger Style | PASS | No change |
| IV. Agent-Testable | PASS | Embedded mode improves testability |
| V. Multiplayer | PASS | Transport abstraction preserves all multiplayer systems |
| VI. Asset Pipeline | PASS | No change |
| VII. Simplicity | PASS | Transport abstraction justified by concrete cross-platform need. ~10 new files, all with clear single responsibility. |
