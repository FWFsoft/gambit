# Tasks: WebAssembly Cross-Platform Support

**Input**: Design documents from `/specs/001-wasm-cross-platform/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create directory structure and transport interface headers needed by all subsequent phases

- [x] T001 Create transport directory structure: `include/transport/` and `src/transport/`
- [x] T002 [P] Create `INetworkTransport` interface in `include/transport/INetworkTransport.h` per contracts/transport-interface.h
- [x] T003 [P] Create `IServerTransport` interface in `include/transport/IServerTransport.h` per contracts/transport-interface.h
- [x] T004 [P] Create `TransportEvent` struct in `include/transport/TransportEvent.h` with CONNECT, RECEIVE, DISCONNECT, NONE types
- [x] T005 [P] Create `ShaderCompat.h` in `include/ShaderCompat.h` with cross-platform GLSL version string helpers per contracts/shader-compat.md

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Refactor existing NetworkServer, NetworkClient, and GameLoop to use transport abstractions. All existing native functionality MUST continue working after this phase.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete. All existing tests must still pass.

- [x] T006 Implement `ENetTransport` (client-side) wrapping existing ENet logic in `include/transport/ENetTransport.h` and `src/transport/ENetTransport.cpp`
- [x] T007 Implement `ENetServerTransport` (server-side) wrapping existing ENet logic in `include/transport/ENetServerTransport.h` and `src/transport/ENetServerTransport.cpp`
- [x] T008 Refactor `NetworkClient` in `include/NetworkClient.h` and `src/NetworkClient.cpp` to accept `INetworkTransport*` instead of raw ENet. Delegate connect/send/run to transport. Preserve all EventBus publishing.
- [x] T009 Refactor `NetworkServer` in `include/NetworkServer.h` and `src/NetworkServer.cpp` to accept `IServerTransport*` instead of raw ENet. Delegate poll/send/broadcast to transport. Preserve all EventBus publishing.
- [x] T010 Update `src/server_main.cpp` to create `ENetServerTransport` and pass to `NetworkServer`
- [x] T011 Update `src/client_main.cpp` to create `ENetTransport` and pass to `NetworkClient`
- [x] T012 Update `src/headless_client_main.cpp` to create `ENetTransport` and pass to `NetworkClient`
- [x] T013 Extract `GameLoop::tick()` method from `GameLoop::run()` in `include/GameLoop.h` and `src/GameLoop.cpp`. Native `run()` calls `tick()` in a while loop. Add `#ifdef __EMSCRIPTEN__` path using `emscripten_set_main_loop_arg`.
- [x] T014 Update `CMakeLists.txt` to compile transport source files for Server, Client, and HeadlessClient targets
- [ ] T015 Verify all existing tests pass with the transport abstraction refactor (`/test`)

**Checkpoint**: All existing native functionality works identically through the transport abstraction. No behavioral changes.

---

## Phase 3: User Story 1 - Build and Run Engine in Browser (Priority: P1) 🎯 MVP

**Goal**: The engine compiles to WebAssembly via Emscripten and renders the isometric scene in a browser with keyboard/mouse input.

**Independent Test**: Build with `emcmake cmake .. && make`, serve with `python3 -m http.server`, open in browser, verify canvas renders and input works.

### Implementation for User Story 1

- [x] T016 [US1] Add Emscripten conditional block to `CMakeLists.txt`: detect `EMSCRIPTEN`, set `-sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sUSE_SDL_MIXER=2 -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1`, skip `find_package` for SDL2/OpenGL/GLAD/SDL2_image/SDL2_mixer on Emscripten
- [x] T017 [US1] Add WasmClient target to `CMakeLists.txt` with Emscripten linker flags, `--preload-file assets@assets`, `.html` output suffix, and spdlog defines (`SPDLOG_NO_THREAD_ID`, `SPDLOG_NO_TLS`)
- [x] T018 [P] [US1] Update `src/Window.cpp`: add `#ifdef __EMSCRIPTEN__` to use `<GLES3/gl3.h>` instead of `<glad/glad.h>`, skip `gladLoadGLLoader()`, change ImGui init to `"#version 300 es"`
- [x] T019 [P] [US1] Update shader version strings in `src/SpriteRenderer.cpp` to use `ShaderCompat.h` for cross-platform `#version` directive
- [x] T020 [P] [US1] Update shader version strings in `src/TileRenderer.cpp` to use `ShaderCompat.h`
- [x] T021 [P] [US1] Update shader version strings in `src/CollisionDebugRenderer.cpp` to use `ShaderCompat.h`
- [x] T022 [P] [US1] Update shader version strings in `src/MusicZoneDebugRenderer.cpp` to use `ShaderCompat.h`
- [x] T023 [P] [US1] Update shader version strings in `src/ObjectiveDebugRenderer.cpp` to use `ShaderCompat.h`
- [x] T024 [US1] Create `src/wasm_client_main.cpp` entry point: initialize SDL2/WebGL window, create stub transport (no networking yet), set up GameLoop with `emscripten_set_main_loop_arg`, render isometric scene
- [x] T025 [US1] Handle tmxlite compilation with Emscripten in `CMakeLists.txt`: add tmxlite source files to WasmClient target or configure as Emscripten-compatible library
- [x] T026 [US1] Handle ImGui `imgui.ini` persistence: set `io.IniFilename = nullptr` on Emscripten builds in `src/Window.cpp` (already done at line 105)
- [ ] T027 [US1] Build WasmClient with `emcmake cmake .. && make` and verify it produces `.html`, `.js`, `.wasm`, `.data` output files (BLOCKED: Emscripten SDK not installed)
- [ ] T028 [US1] Serve WASM build locally and verify: canvas renders, isometric scene displays, keyboard/mouse input works in Chrome and Firefox (BLOCKED: requires T027)

**Checkpoint**: The engine renders and accepts input in a browser. No networking yet — single-player rendering only.

---

## Phase 4: User Story 2 - Multiplayer Testing with Minimal Setup (Priority: P1)

**Goal**: Browser clients connect to the native server via WebSocket. Embedded server mode enables single-browser-tab local play. Mixed native + browser client sessions work.

**Independent Test**: Start native server, open browser client, verify connection and state sync. Then test embedded mode: open WASM build without server, verify local game plays.

### Implementation for User Story 2

#### Embedded Server Mode (InMemory Transport)

- [x] T029 [P] [US2] Create `InMemoryChannel` in `include/InMemoryChannel.h` with bidirectional message queues per contracts/transport-interface.h
- [x] T030 [P] [US2] Implement `InMemoryTransport` (client-side) in `include/transport/InMemoryTransport.h` and `src/transport/InMemoryTransport.cpp`: push to clientToServer queue, poll from serverToClient queue
- [x] T031 [P] [US2] Implement `InMemoryServerTransport` (server-side) in `include/transport/InMemoryServerTransport.h` and `src/transport/InMemoryServerTransport.cpp`: push to serverToClient queue, poll from clientToServer queue
- [x] T032 [US2] Create `GameSession` orchestrator in `include/GameSession.h` and `src/GameSession.cpp`: instantiate `InMemoryChannel`, create `InMemoryServerTransport` + `InMemoryTransport`, wire `ServerGameState` + `NetworkClient`, tick both in same `GameLoop`
- [x] T033 [US2] Update `src/wasm_client_main.cpp` to support embedded server mode: create `GameSession` when no server URL specified, use `InMemoryTransport` for networking
- [x] T034 [US2] Add `GameSession` as a native build option: update `src/client_main.cpp` with a `--embedded` flag that creates `GameSession` instead of `ENetTransport` for quick solo testing

#### WebSocket Transport (Browser Networking)

- [ ] T035 [US2] Implement `WebSocketTransport` (client-side, WASM only) in `include/transport/WebSocketTransport.h` and `src/transport/WebSocketTransport.cpp` using `<emscripten/websocket.h>`: connect via `ws://`, send binary, poll received messages via callbacks
- [ ] T036 [US2] Add WebSocket server library dependency to `CMakeLists.txt` for native Server target (e.g., uWebSockets or libwebsockets)
- [ ] T037 [US2] Implement `WebSocketServerTransport` (native server-side) in `include/transport/WebSocketServerTransport.h` and `src/transport/WebSocketServerTransport.cpp`: accept WebSocket connections on a configurable port, translate to `TransportEvent`
- [ ] T038 [US2] Update `src/server_main.cpp` to create both `ENetServerTransport` and `WebSocketServerTransport`, passing both to `NetworkServer` (or running both transports and merging events)
- [ ] T039 [US2] Update `src/wasm_client_main.cpp` to support networked mode: when a server URL is provided, create `WebSocketTransport` instead of embedded `GameSession`
- [ ] T040 [US2] Add `-lwebsocket.js` linker flag to WasmClient target in `CMakeLists.txt`

#### Dev Environment Integration

- [ ] T041 [US2] Create `scripts/dev-wasm.sh`: builds native Server + WasmClient, starts server, starts `python3 -m http.server` for WASM files, opens browser tab, waits for Enter to kill all processes
- [ ] T042 [US2] Verify: start native server, open browser WasmClient, confirm WebSocket connection, player joins, movement syncs
- [ ] T043 [US2] Verify: open WASM build without server (embedded mode), confirm local game plays with no external dependencies
- [ ] T044 [US2] Verify: mixed session — 1 native client + 1 browser client connected to same server, verify state syncs between both

**Checkpoint**: Full multiplayer works with browser clients. Embedded mode provides zero-setup local play. Mixed native + browser sessions function.

---

## Phase 5: User Story 3 - Cross-Platform Development Workflow (Priority: P2)

**Goal**: Developers can build both native and WASM targets from the same source tree with no code duplication. Single workflow for both.

**Independent Test**: Modify a game constant (e.g., player speed), rebuild both targets, verify change reflected in both native and browser builds.

### Implementation for User Story 3

- [ ] T045 [US3] Add `/build-wasm` Claude skill in `.claude/skills/build-wasm/SKILL.md` that runs `emcmake cmake` + `make` in `build-wasm/` directory
- [ ] T046 [US3] Add `/dev-wasm` Claude skill in `.claude/skills/dev-wasm/SKILL.md` that invokes `scripts/dev-wasm.sh`
- [ ] T047 [US3] Update `/build` skill to optionally accept a `--wasm` flag or document how to build WASM alongside native
- [ ] T048 [US3] Verify: modify `include/config/PlayerConfig.h` (e.g., change movement speed), rebuild both native and WASM, confirm both reflect the change
- [ ] T049 [US3] Document Emscripten SDK setup in `README.md` under a new "WebAssembly Build" section

**Checkpoint**: A single source tree builds to both native and WASM with documented workflows and Claude skills.

---

## Phase 6: User Story 4 - Automated Cross-Environment Testing (Priority: P3)

**Goal**: Unit tests run on both native and WASM builds. WASM tests execute in a headless browser environment.

**Independent Test**: Run `ctest` for native, run WASM tests via Node.js or headless browser, compare results.

### Implementation for User Story 4

- [ ] T050 [US4] Configure WasmClient test targets in `CMakeLists.txt`: compile unit tests (test_eventbus, test_network_protocol, test_collision, test_gameloop, etc.) with Emscripten
- [ ] T051 [US4] Add Node.js test runner script in `scripts/test-wasm.sh` that executes WASM test binaries via `node` (Emscripten `.js` output can run in Node.js)
- [ ] T052 [US4] Add `/test-wasm` Claude skill in `.claude/skills/test-wasm/SKILL.md` that runs `scripts/test-wasm.sh`
- [ ] T053 [US4] Verify: core unit tests (test_eventbus, test_network_protocol, test_collision, test_gameloop) pass on both native and WASM
- [ ] T054 [US4] Add WASM test target to CI/CD pipeline configuration (if CI config exists)

**Checkpoint**: Same test suite runs and passes on both native and WASM targets.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories

- [ ] T055 [P] Add browser tab backgrounding handling in `src/wasm_client_main.cpp`: detect visibility change via `emscripten_set_visibilitychange_callback`, pause/resume game loop
- [ ] T056 [P] Add WebGL2 feature detection in `src/wasm_client_main.cpp`: check for WebGL2 support on startup, display error message if unavailable
- [ ] T057 [P] Add browser memory budget monitoring: log WASM memory usage periodically, warn if approaching limits
- [ ] T058 [P] Validate WASM binary size is under 20MB compressed
- [ ] T059 Run quickstart.md validation: follow every step in `specs/001-wasm-cross-platform/quickstart.md` and verify accuracy

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (interfaces must exist). BLOCKS all user stories.
- **User Story 1 (Phase 3)**: Depends on Phase 2 (transport abstraction must be in place)
- **User Story 2 (Phase 4)**: Depends on Phase 3 (WASM build must work before adding networking)
- **User Story 3 (Phase 5)**: Depends on Phase 3 (both build targets must exist)
- **User Story 4 (Phase 6)**: Depends on Phase 3 (WASM build must compile tests)
- **Polish (Phase 7)**: Depends on Phase 3 at minimum

### User Story Dependencies

- **User Story 1 (P1)**: Foundational → US1. No dependencies on other stories. **MVP target.**
- **User Story 2 (P1)**: US1 → US2. Requires working WASM build before adding networking/embedded mode.
- **User Story 3 (P2)**: US1 → US3. Can run in parallel with US2 (different files).
- **User Story 4 (P3)**: US1 → US4. Can run in parallel with US2 and US3.

### Within Each Phase

- Tasks marked [P] within a phase can run in parallel
- Unmarked tasks should run sequentially in order
- Verify/checkpoint tasks must run last in their phase

### Parallel Opportunities

- T002, T003, T004, T005 (Phase 1 interface headers) — all parallel
- T018-T023 (Phase 3 shader updates) — all parallel, different files
- T029, T030, T031 (Phase 4 InMemory transport) — all parallel, different files
- T045, T046, T049 (Phase 5 skills/docs) — all parallel
- T055, T056, T057, T058 (Phase 7 polish) — all parallel

---

## Parallel Example: User Story 1

```text
# Phase 1 — All interface headers in parallel:
T002: Create INetworkTransport interface in include/transport/INetworkTransport.h
T003: Create IServerTransport interface in include/transport/IServerTransport.h
T004: Create TransportEvent struct in include/transport/TransportEvent.h
T005: Create ShaderCompat.h in include/ShaderCompat.h

# Phase 3 — All shader updates in parallel (after T018):
T019: Update SpriteRenderer.cpp shader strings
T020: Update TileRenderer.cpp shader strings
T021: Update CollisionDebugRenderer.cpp shader strings
T022: Update MusicZoneDebugRenderer.cpp shader strings
T023: Update ObjectiveDebugRenderer.cpp shader strings
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T005)
2. Complete Phase 2: Foundational (T006-T015)
3. Complete Phase 3: User Story 1 (T016-T028)
4. **STOP and VALIDATE**: Engine renders in browser with input working
5. This is a deployable, demonstrable milestone

### Incremental Delivery

1. Setup + Foundational → Transport abstraction works, native unchanged
2. Add User Story 1 → Engine runs in browser (MVP!)
3. Add User Story 2 → Multiplayer works with browser clients + embedded mode
4. Add User Story 3 → Developer workflow polished with skills and docs
5. Add User Story 4 → Automated cross-environment testing
6. Each story adds value without breaking previous stories

### Parallel Team Strategy

With multiple developers after Foundational is complete:

- Developer A: User Story 1 (must go first — other stories depend on it)
- After US1 is done:
  - Developer A: User Story 2 (networking)
  - Developer B: User Story 3 (workflow/skills) — can run in parallel with US2
  - Developer C: User Story 4 (testing) — can run in parallel with US2

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- The Foundational phase is critical — all existing native tests must still pass after the transport refactor
- Embedded server mode (US2) works on both native and WASM per clarification decisions
- Shader changes (T018-T023) are mechanical and low-risk — same GLSL body, different version directive
- WebSocket server library choice (T036) may require evaluation — uWebSockets and libwebsockets are both viable
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
