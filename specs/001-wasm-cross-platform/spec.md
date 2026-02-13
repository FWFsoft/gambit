# Feature Specification: WebAssembly Cross-Platform Support

**Feature Branch**: `001-wasm-cross-platform`
**Created**: 2026-01-29
**Status**: Draft
**Input**: User description: "I want to make an isometric 2D game engine that's easy to test in different environments. Ideally I think we should commit to making a version that works with web-assembly, so we can test running it in the browser, as well as figure out how to do easy multiplayer tests"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build and Run Engine in Browser (Priority: P1)

A developer builds the Gambit engine targeting WebAssembly and opens the resulting page in a web browser. The isometric 2D game renders in a canvas element with the same visual fidelity as the native desktop build. Input (keyboard/mouse) works identically to the native version.

**Why this priority**: Browser deployment is the core ask — it enables instant sharing, zero-install testing, and broadens the audience for playtesting. Everything else depends on having a working WASM build.

**Independent Test**: Build the engine with a WASM toolchain, serve the output locally, open in a browser, and verify that the game window renders and accepts input.

**Acceptance Scenarios**:

1. **Given** the engine source code, **When** a developer runs the WASM build target, **Then** the build produces browser-loadable artifacts (HTML, JS glue, WASM binary) without errors.
2. **Given** the WASM build artifacts served via a local web server, **When** a user opens the page in a modern browser (Chrome, Firefox, Safari, Edge), **Then** the game canvas renders the isometric scene at a playable frame rate.
3. **Given** the game running in the browser, **When** the user provides keyboard and mouse input, **Then** the game responds identically to the native desktop build.

---

### User Story 2 - Multiplayer Testing with Minimal Setup (Priority: P1)

A developer starts a game server (native or WASM-based) and connects multiple clients — some native, some browser-based — to test multiplayer gameplay. For browser-only testing, the WASM build runs the server embedded in-process, so opening a single browser tab gives a fully playable local game with no external server required. For multi-client testing, the setup requires only a single command or script to launch the server and multiple clients, similar to the existing `/dev` skill but with browser clients included.

**Why this priority**: Easy multiplayer testing is equally critical to the user's goals. Being able to mix native and browser clients dramatically simplifies testing with remote collaborators (just share a URL).

**Independent Test**: Run the server, open 2+ browser tabs as clients, verify they connect, see each other's state, and can interact in the shared game world.

**Acceptance Scenarios**:

1. **Given** the WASM build with embedded server mode, **When** a user opens the game in a browser tab, **Then** the game starts a local session with a playable character and no external server required.
2. **Given** a running game server, **When** a browser-based client navigates to the game URL, **Then** it connects to the server and joins the game session.
3. **Given** multiple connected clients (mix of native and browser), **When** one client moves their character, **Then** all other clients see the movement reflected within an acceptable latency window.
4. **Given** 4 clients connected (the engine's target player count), **When** all clients are actively playing, **Then** the game maintains playable performance on all clients.

---

### User Story 3 - Cross-Platform Development Workflow (Priority: P2)

A developer iterates on game code using their normal native build/test cycle, then periodically builds and tests the WASM target to verify cross-platform compatibility. The build system supports both targets from the same source tree without requiring separate project configurations.

**Why this priority**: Developers need a smooth workflow that doesn't force them to choose between native and WASM builds. This ensures the WASM target doesn't become a second-class citizen.

**Independent Test**: Modify a game system (e.g., movement speed), build both native and WASM targets, and verify the change is reflected in both.

**Acceptance Scenarios**:

1. **Given** the engine source code, **When** a developer builds both native and WASM targets, **Then** both builds succeed from the same source tree without code duplication.
2. **Given** a code change to a shared system, **When** both targets are rebuilt, **Then** the change is reflected identically in both the native and browser versions.
3. **Given** the WASM build, **When** it is served locally, **Then** the developer can test it without any additional toolchain setup beyond the initial WASM SDK installation.

---

### User Story 4 - Automated Cross-Environment Testing (Priority: P3)

A developer runs the test suite and it executes tests against both the native and WASM builds. Headless browser testing validates that the WASM build passes the same functional tests as the native build, enabling CI/CD integration.

**Why this priority**: Automated testing across environments catches platform-specific bugs early, but manual testing is sufficient in the short term.

**Independent Test**: Run the test suite with a flag or target that includes WASM tests, verify tests execute in a headless browser and report results.

**Acceptance Scenarios**:

1. **Given** the existing test suite, **When** a developer runs WASM-targeted tests, **Then** core engine tests execute in a headless browser environment and report pass/fail results.
2. **Given** a CI/CD pipeline, **When** a commit is pushed, **Then** both native and WASM tests run automatically and report results.

---

### Edge Cases

- What happens when a browser client loses network connectivity mid-game? The engine should handle disconnection gracefully, notifying remaining clients and allowing reconnection.
- What happens when a browser doesn't support WebGL2 or required WebAssembly features? The game should display a clear error message indicating minimum browser requirements.
- How does the engine handle browser tab backgrounding (which throttles timers/requestAnimationFrame)? The game loop should detect when the tab regains focus and reconcile state rather than trying to "catch up" on missed frames.
- What happens when the WASM build exceeds typical browser memory limits? The engine should stay within reasonable memory budgets and report an error if limits are approached.
- How does the engine handle mixed-latency scenarios where native clients have near-zero latency and browser clients have higher latency? The existing networking model (client prediction + server reconciliation) should handle this transparently.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The engine MUST compile to WebAssembly from the same C++ source used for native builds, using Emscripten as the compilation toolchain.
- **FR-002**: The WASM build MUST render the isometric 2D scene in an HTML canvas using WebGL, maintaining visual parity with the native OpenGL rendering.
- **FR-003**: The WASM build MUST support keyboard and mouse input through browser event handling, mapped to the same input system used by the native build.
- **FR-004**: The WASM build MUST connect to the game server using WebSocket transport, since raw UDP (ENet) is not available in browsers. The server MUST accept both ENet (native) and WebSocket (browser) connections.
- **FR-005**: The build system MUST support a WASM build target alongside the existing native target, triggered by a distinct CMake preset or toolchain file.
- **FR-006**: The WASM build MUST produce a self-contained set of artifacts (HTML page, JavaScript glue code, WASM binary) that can be served by any static web server.
- **FR-007**: The engine MUST abstract the networking transport layer so that game logic does not depend on whether the underlying transport is ENet or WebSocket.
- **FR-008**: The development environment MUST support launching a mixed session of native and browser clients for multiplayer testing.
- **FR-009**: The WASM build MUST support the engine's fixed 60 FPS update loop using `requestAnimationFrame` or equivalent browser scheduling.
- **FR-010**: The engine MUST handle browser-specific lifecycle events (tab backgrounding, focus loss) without crashing or corrupting game state.
- **FR-013**: The WASM build MUST bundle all game assets at build time into the output package, so the browser build is fully self-contained and requires no runtime asset fetching.
- **FR-011**: The engine MUST support an embedded server mode where server logic runs in-process with the client, enabling a fully self-contained local game session without a separate server process. This mode MUST be available on both native and WASM builds.
- **FR-012**: In embedded server mode, the server and client MUST communicate via a shared in-memory message queue that preserves the existing async event-based architecture. Server and client code paths MUST remain structurally identical to the networked case.

### Key Entities

- **Build Target**: Represents a platform compilation target (native or WASM), including toolchain configuration, output artifacts, and platform-specific abstractions.
- **Transport Layer**: An abstraction over the network protocol that provides a unified interface for sending/receiving game messages. Three backends: ENet (native networked), WebSocket (browser networked), and In-Memory Queue (embedded server mode). All backends expose the same async message interface.
- **Platform Abstraction**: A set of interfaces that isolate platform-specific behavior (rendering backend, input handling, audio, file I/O) from shared game logic.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The engine runs in all major modern browsers (Chrome, Firefox, Safari, Edge) at a sustained 60 FPS for a standard test scene with at least 100 on-screen sprites.
- **SC-002**: A developer can go from source checkout to a running browser build in under 10 minutes of setup (installing Emscripten SDK + building).
- **SC-003**: 4 players can play together with a mix of native and browser clients, with all clients experiencing less than 200ms of perceived input-to-visual-update latency on a local network.
- **SC-004**: The same unit test suite passes on both native and WASM builds with no platform-specific test exclusions for core game logic.
- **SC-005**: The WASM binary size is under 20MB (compressed) to ensure reasonable load times over typical broadband connections.
- **SC-006**: A new developer can launch a multiplayer test session (1 server + 4 clients, at least one browser-based) using a single command.
- **SC-007**: A developer can open the WASM build in a single browser tab and immediately play a local game session with no external server process running.

## Clarifications

### Session 2026-01-29

- Q: Should the engine support running server and client together in a single process for local testing, especially in the browser? → A: Yes — embedded server mode, where server logic runs in-process with the client. This is essential for WASM builds where launching a separate server process is not possible.
- Q: How should server and client communicate in embedded server mode? → A: Shared in-memory message queue, preserving the existing async event-based architecture. Server and client push/pop from a shared queue, so their code paths remain structurally identical to the networked case.
- Q: Should embedded server mode be available only in WASM builds or also native? → A: Both platforms. Embedded mode works on native and WASM, enabling quick solo testing on either target and simplifying development/debugging of the feature on native before porting to WASM.
- Q: How should the WASM build handle asset loading without filesystem access? → A: Bundle all assets at build time via Emscripten's virtual filesystem. The engine's existing file I/O code works unchanged through Emscripten's FS API.

## Assumptions

- **Emscripten** is the appropriate WASM compilation toolchain for this C++ codebase, as it has mature SDL2 and OpenGL ES/WebGL support.
- **WebSocket** is the browser-side network transport, since browsers cannot use raw UDP. The server will need to accept both ENet and WebSocket connections.
- The existing **SDL2** abstraction layer will map to Emscripten's SDL2 port for the browser build, minimizing rendering and input code changes.
- **WebGL2** is the target browser graphics API, corresponding to the engine's OpenGL usage. Older browsers without WebGL2 are not supported.
- The engine's **spdlog** logging will need a browser-compatible backend (e.g., console.log via Emscripten).
- Audio, file I/O, and other platform services will need browser-specific implementations but are out of scope for the initial WASM milestone unless otherwise specified.
