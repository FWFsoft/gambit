<!--
  SYNC IMPACT REPORT
  ==================
  Version change: (new) → 1.0.0
  Ratification: Initial constitution creation

  Added principles:
    - I. AI-First Development
    - II. Fast Build/Boot/Load
    - III. Tiger Style Debuggability
    - IV. Agent-Testable by Default
    - V. High-Performance Multiplayer
    - VI. AI-Generated Asset Pipeline
    - VII. Simplicity & Direct Control

  Added sections:
    - Technology & Constraints
    - Development Workflow

  Templates requiring updates:
    - .specify/templates/plan-template.md ✅ no changes needed
      (Constitution Check section is generic; principles map cleanly)
    - .specify/templates/spec-template.md ✅ no changes needed
      (User story / requirements structure compatible)
    - .specify/templates/tasks-template.md ✅ no changes needed
      (Phase structure accommodates test-first and headless testing)
    - .specify/templates/commands/*.md — no files exist, N/A

  Deferred items:
    - RATIFICATION_DATE set to today (first adoption)
-->

# Gambit Constitution

## Core Principles

### I. AI-First Development

Gambit is built and tested primarily by AI agents (e.g., Claude Code).
All engine code, game code ("Blue Zone"), and tooling MUST be
structured so an AI agent can understand, modify, build, and verify
changes autonomously.

- Code MUST be self-documenting with clear naming and minimal
  indirection so agents can navigate without human guidance.
- Every system MUST expose a programmatic interface (CLI, API, or
  event) that an agent can invoke without a GUI.
- Human-in-the-loop is for design decisions and review, not for
  routine build/test/deploy workflows.

### II. Fast Build/Boot/Load

The engine MUST build, boot, and load fast enough for tight AI-driven
iteration loops. There is no hot reload; speed comes from a short
full-cycle rebuild.

- Incremental builds MUST complete in seconds, not minutes.
- The engine MUST boot to a testable state without manual
  interaction (no dialogs, no prompts).
- Asset loading MUST be lazy or parallelized so startup is not
  blocked by large asset trees.

### III. Tiger Style Debuggability

Following Tiger Style, the engine MUST crash early and loudly on
invalid state rather than silently propagating errors.

- All invariants MUST be guarded by assertions that crash the
  process with a clear message and location.
- Structured logging via spdlog MUST be present at system boundaries
  (network, file I/O, state transitions) so failures are traceable.
- Error messages MUST include enough context (values, IDs, frame
  numbers) to diagnose the problem without a debugger attached.
- Silent failures (swallowed exceptions, default fallbacks that hide
  bugs) are prohibited.

### IV. Agent-Testable by Default

Every feature MUST be verifiable by the agent harness without human
visual inspection. Testing is the primary quality gate.

- Unit tests MUST cover core systems (EventBus, networking,
  prediction, collision, animation, game loop).
- A headless mode MUST exist that runs the full game loop without
  SDL windows, OpenGL, or ImGui so tests can run in CI and agent
  environments.
- Integration tests MUST use EventCapture, InputScript, and
  EventLogger to assert on game behavior programmatically.
- New features MUST NOT be considered complete until they have
  corresponding automated tests that an agent can execute via
  `ctest` or the `/test` skill.

### V. High-Performance Multiplayer

Gambit is a 2D isometric engine optimized for 4-player co-op PvE
over a client-server architecture.

- The update loop MUST fire at a fixed 60 FPS; the render loop MAY
  drop frames but the update loop MUST NOT.
- Client prediction and server reconciliation MUST be implemented
  for responsive local input.
- Remote player interpolation MUST smooth network jitter.
- The network protocol MUST use ENet (UDP) with a binary format
  designed for minimal bandwidth.

### VI. AI-Generated Asset Pipeline

Assets for Blue Zone (sprites, tilesets, UI elements) MUST be
producible via AI generation tools, primarily PixelLab.ai, alongside
traditional tools (Aseprite, Tiled, Rive).

- The asset pipeline MUST accept standard formats (PNG sprite
  sheets, Tiled JSON/TMX, Aseprite files) without manual
  conversion steps.
- AI-generated assets MUST go through the same pipeline as
  hand-crafted assets; no special-case loaders.
- Asset metadata (hitboxes, anchor points, animation frames) MUST
  be defined in the asset files themselves (Aseprite tags, Tiled
  custom properties), not in engine code.

### VII. Simplicity & Direct Control

The engine follows a "high-code" philosophy: APIs over UIs,
developer understanding over hand-holding abstractions.

- No abstraction layer MUST exist solely for organizational
  purposes; every abstraction MUST solve a concrete, demonstrated
  problem.
- Event-based architecture MUST be used instead of coupling game
  logic to update/render loops (no "MonoBehavior" pattern).
- Dependencies MUST be kept minimal; each dependency MUST justify
  its inclusion by replacing significant complexity.

## Technology & Constraints

- **Language**: C++17
- **Build System**: CMake
- **Graphics**: SDL2 + OpenGL (2D isometric rendering)
- **Networking**: ENet (UDP); Steamworks API planned for lobbying
- **Logging**: spdlog (structured, fast)
- **UI**: ImGui (debug/settings overlay)
- **Asset Tools**: Tiled (maps), Aseprite (sprites), Rive (UI),
  PixelLab.ai (AI-generated sprites/tilesets)
- **Testing**: ctest + custom headless harness
  (EventCapture, InputScript, EventLogger)
- **Target Platforms**: PC (Windows, Linux, macOS); others are
  stretch goals
- **Performance Target**: Fixed 60 FPS update loop, frame-dropping
  render loop
- **Included Game**: "Blue Zone" (working title) — 2D isometric
  ARPG, 4-player co-op PvE

## Development Workflow

- **Build**: `/build` skill or `cmake --build build`
- **Test**: `/test` skill or `ctest` in the build directory;
  integration tests require a running server
- **Dev Environment**: `/dev` skill starts 1 server + 4 clients
  for multiplayer testing
- **Code Quality**: `/pre-commit` runs clang-format and clang-tidy
  on all source files before commit
- **Search**: `/search <symbol>` for definition and usage lookup
- **Visual Testing**: `/visual-test` for agent-driven visual
  verification when headless is insufficient
- All workflows MUST be invocable by an AI agent without manual
  GUI interaction

## Governance

- This constitution is the authoritative source of project
  principles. It supersedes informal conventions and ad-hoc
  decisions.
- Amendments MUST be documented with a version bump, rationale,
  and updated date.
- Version follows semantic versioning: MAJOR for principle
  removals or incompatible redefinitions, MINOR for new principles
  or material expansions, PATCH for clarifications and wording.
- All feature specs and implementation plans MUST pass a
  Constitution Check (see plan-template.md) before work begins.
- Compliance is verified during code review: reviewers MUST
  flag violations of any numbered principle.

**Version**: 1.0.0 | **Ratified**: 2026-01-29 | **Last Amended**: 2026-01-29
