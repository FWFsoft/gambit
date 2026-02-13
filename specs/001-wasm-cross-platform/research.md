# Research: WebAssembly Cross-Platform Support

**Feature**: 001-wasm-cross-platform
**Date**: 2026-01-29

## R-001: Emscripten SDL2 Port

**Decision**: Use Emscripten's built-in SDL2 port via `-sUSE_SDL=2` linker flag.

**Rationale**: Emscripten includes SDL2 as a first-class port. SDL2_image (`-sUSE_SDL_IMAGE=2`) and SDL2_mixer (`-sUSE_SDL_MIXER=2`) are also available. No `find_package` is needed — the toolchain provides everything. Existing SDL2 code (window creation, event polling, audio) works with minimal changes.

**Alternatives considered**:
- Manual SDL2 cross-compilation: More control but unnecessary complexity since the port is well-maintained.
- Dropping SDL2 for raw Emscripten APIs: Would require rewriting input, audio, and window management from scratch.

## R-002: OpenGL to WebGL2 Shader Migration

**Decision**: Target WebGL2 (OpenGL ES 3.0) with `#version 300 es` shaders. Conditionally compile shader version strings using `#ifdef __EMSCRIPTEN__`.

**Rationale**: WebGL2 is the closest browser equivalent to the engine's OpenGL 3.3 core profile. The `in`/`out` variable syntax and `texture()` function calls are identical — only the version directive and `precision mediump float;` qualifier need to change. Five source files contain shader strings that need updating.

**Alternatives considered**:
- WebGL1 (OpenGL ES 2.0): Would require rewriting shaders to use `attribute`/`varying` instead of `in`/`out`. Unnecessary since WebGL2 has near-universal browser support.
- WebGPU: Too bleeding-edge; browser support is still incomplete.

## R-003: GLAD Removal for Emscripten

**Decision**: Conditionally exclude GLAD on Emscripten builds. Use `<GLES3/gl3.h>` instead.

**Rationale**: Emscripten provides GL function bindings internally through `library_gl.js`. Dynamic GL function loading (GLAD's purpose) is not needed or supported in the browser. The `gladLoadGLLoader()` call in `Window.cpp` must be `#ifdef`'d out.

**Alternatives considered**: None viable — GLAD is fundamentally incompatible with Emscripten's GL implementation.

## R-004: Main Loop Restructuring

**Decision**: Use `emscripten_set_main_loop_arg()` with `fps=0` (requestAnimationFrame) on Emscripten builds. Refactor `GameLoop::run()` to extract a single-iteration method that can be called as a callback.

**Rationale**: Browsers use cooperative multitasking — a `while(running)` loop blocks the browser thread. The `emscripten_set_main_loop_arg` function registers a per-frame callback that yields to the browser between frames. Using `fps=0` syncs to the monitor refresh rate via `requestAnimationFrame`.

**Alternatives considered**:
- Asyncify: Allows keeping the `while` loop but adds code size overhead (~10-20%) and performance cost. Better suited for legacy ports where restructuring is impractical.
- `emscripten_set_main_loop` (no arg): Less flexible — can't pass state without globals.

## R-005: Virtual Filesystem for Asset Bundling

**Decision**: Use `--preload-file` to bundle all assets at build time. Map `assets/` directory to `/assets` in the virtual filesystem.

**Rationale**: Emscripten's MEMFS preload unpacks a `.data` file into an in-memory virtual filesystem before WASM execution begins. Standard C++ file I/O (`fopen`, `ifstream`, `std::filesystem`) works unchanged against this virtual FS. No code changes needed for asset loading.

**Alternatives considered**:
- `--embed-file`: Embeds assets directly in the WASM binary. Increases binary size and can't be cached separately.
- Runtime HTTP fetch: Requires async loading code and a loading screen. Unnecessary for the current asset size.

## R-006: WebSocket Transport Layer

**Decision**: Create an `INetworkTransport` abstraction with three implementations: `ENetTransport` (native networked), `WebSocketTransport` (browser networked via `<emscripten/websocket.h>`), and `InMemoryTransport` (embedded server mode).

**Rationale**: ENet uses raw UDP which browsers cannot access. Emscripten provides a native WebSocket API. The server must accept both ENet and WebSocket connections. A transport abstraction allows game logic to be transport-agnostic.

**Alternatives considered**:
- WebRTC DataChannels: UDP-like but requires STUN/TURN server setup and complex signaling. Overkill for local/LAN testing.
- Emscripten's POSIX socket emulation: Only supports TCP, not UDP. Would still require server-side changes.

**Key difference**: WebSockets are TCP-based (ordered, reliable). ENet supports unreliable/unordered channels. The browser transport path will have slightly different latency characteristics. For the engine's 60 FPS update rate with small packets, this is acceptable.

## R-007: Server Dual-Protocol Support

**Decision**: The native server must accept both ENet (UDP) and WebSocket (TCP) connections simultaneously on different ports. A lightweight WebSocket library (e.g., libwebsockets or uWebSockets) will be added to the server.

**Rationale**: Browser clients can only connect via WebSocket. Native clients use ENet. The server must bridge both transports into the same game session. Both transport types produce the same `NetworkPacketReceivedEvent` / `ClientConnectedEvent` / `ClientDisconnectedEvent` events via EventBus.

**Alternatives considered**:
- Separate WebSocket proxy process: Adds operational complexity (two processes to manage).
- ENet-over-WebSocket bridge: Exists in some projects but adds another dependency and latency.

## R-008: spdlog on Emscripten

**Decision**: Use spdlog in header-only mode with threading disabled (`SPDLOG_NO_THREAD_ID`, `SPDLOG_NO_TLS`).

**Rationale**: spdlog compiles with Emscripten but requires threading to be disabled since Emscripten's pthread support needs special HTTP headers (`COOP`/`COEP`). Emscripten automatically routes `stdout`/`stderr` to `console.log`/`console.error`, so spdlog output appears in the browser developer console.

**Alternatives considered**:
- Custom Emscripten log sink using `emscripten_log()`: Better browser console integration but unnecessary for initial port.
- Replacing spdlog entirely: Would affect native builds and existing code.

## R-009: ImGui on Emscripten

**Decision**: Use existing ImGui SDL2+OpenGL3 backends unchanged. Update the GLSL version string in `ImGui_ImplOpenGL3_Init()` to `"#version 300 es"` on Emscripten. Disable `imgui.ini` file saving.

**Rationale**: ImGui's official `example_sdl2_opengl3` already supports Emscripten with `#ifdef __EMSCRIPTEN__` guards in the backend code. The only change needed is the init string and disabling persistent settings (MEMFS is ephemeral).

**Alternatives considered**: None — ImGui works out of the box.

## R-010: tmxlite on Emscripten

**Decision**: Compile tmxlite as a source dependency with Emscripten. It is a C++ library with no platform-specific code beyond file I/O.

**Rationale**: tmxlite uses standard C++ file I/O which works unchanged on Emscripten's virtual filesystem. It needs to be compiled with the Emscripten toolchain rather than linked as a system library.

**Alternatives considered**:
- Pre-processing maps to a simpler format: Adds a build step and loses Tiled metadata.
- Loading maps from JSON manually: Reinvents tmxlite's functionality.

## R-011: CMake Emscripten Configuration

**Decision**: Add a conditional `if(EMSCRIPTEN)` block in CMakeLists.txt that replaces `find_package` calls with Emscripten's `-sUSE_*` flags and sets WebGL/filesystem/WebSocket linker options.

**Rationale**: Emscripten provides all major dependencies (SDL2, OpenGL, SDL2_image, SDL2_mixer) via built-in ports. The toolchain file sets `CMAKE_SYSTEM_NAME` to `Emscripten`, enabling `if(EMSCRIPTEN)` checks. Build invocation uses `emcmake cmake ..`.

**Key flags**:
```
-sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sUSE_SDL_MIXER=2
-sSDL2_IMAGE_FORMATS=["png","jpg"]
-sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1
-sALLOW_MEMORY_GROWTH=1
-lwebsocket.js
--preload-file assets@assets
```

## R-012: Embedded Server Mode Architecture

**Decision**: Create a unified `GameSession` class that can run server logic in-process with the client. The `InMemoryTransport` backend routes messages between server and client through a shared lock-free queue, preserving the EventBus-based architecture.

**Rationale**: In embedded mode, both `ServerGameState` and `ClientPrediction` run in the same process. The `InMemoryTransport` replaces network I/O with direct queue push/pop. Server and client code paths remain identical — they still send/receive the same binary packets, just without serialization overhead.

**Architecture**:
- `InMemoryTransport` implements `INetworkTransport`
- Two instances (server-side, client-side) share a pair of message queues
- `poll()` drains the incoming queue and publishes `NetworkPacketReceivedEvent`
- `send()` pushes to the outgoing queue (which is the other side's incoming queue)
- `GameLoop` ticks both server and client update logic in the same frame

**Alternatives considered**:
- Direct function calls between server/client: Breaks the event-based architecture and requires different code paths.
- Loopback WebSocket: Unnecessary overhead for in-process communication.
