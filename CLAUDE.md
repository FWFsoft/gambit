# Gambit Game Engine - Claude Code Guide

## Overview

Gambit is a C++ game engine designed for building 2D isometric ARPG games with a focus on debuggability, stability, and performance. The engine follows a "high-code" philosophy, prioritizing APIs over UIs and developer understanding over hand-holding abstractions.

## Project Architecture

### Core Design Principles

- **Tiger Style Development**: Assert on negative space, crashing early to catch bugs
- **Fast Build/Boot/Load**: Optimized for rapid iteration cycles
- **Event-Based Architecture**: Games built on the engine operate over asynchronous events rather than traditional update/render loops
- **Fixed 60 FPS**: Update loop always fires, render loop drops frames that aren't ready in 16ms

### Current Implementation Status

The engine is in early development with the following components:

- **Networking**: Client-Server architecture using ENet for P2P networking
- **Window Management**: SDL2-based window system with OpenGL rendering
- **Logging**: spdlog-based logging infrastructure
- **File System**: Basic file system utilities

### Technology Stack

- **Language**: C++17
- **Build System**: CMake
- **Graphics**: SDL2 + OpenGL
- **Networking**: ENet (with planned Steamworks integration for lobbying)
- **Logging**: spdlog

### Project Structure

```
gambit/
├── .claude/
│   └── skills/             # Claude Code skills (SKILL.md format)
│       ├── build/          # Build the project
│       ├── clean/          # Clean build artifacts
│       ├── dev/            # Development environment (1 server + 4 clients)
│       ├── run-client/     # Run a single client
│       ├── run-server/     # Run the server
│       └── test/           # Run tests
├── CMakeLists.txt          # CMake build configuration
├── CLAUDE.md               # This file - comprehensive guide for Claude
├── DESIGN.md               # Detailed design requirements and decisions
├── README.md               # User-facing documentation
├── include/                # Public header files
│   ├── FileSystem.h
│   ├── Logger.h
│   ├── NetworkClient.h
│   ├── NetworkServer.h
│   └── Window.h
├── src/                    # Implementation files
│   ├── client_main.cpp     # Client executable entry point
│   ├── server_main.cpp     # Server executable entry point
│   ├── FileSystem.cpp
│   ├── Logger.cpp
│   ├── NetworkClient.cpp
│   ├── NetworkServer.cpp
│   └── Window.cpp
└── build/                  # Build output directory (generated)
```

## Dependencies

### Required

- **CMake** (>= 3.16)
- **SDL2**: Graphics and window management
- **ENet**: UDP networking library
- **spdlog**: Fast C++ logging library

### Installation

**macOS (via Homebrew)**:
```bash
brew install cmake sdl2 enet spdlog
```

**Linux (Ubuntu/Debian)**:
```bash
sudo apt-get install cmake libsdl2-dev libenet-dev libspdlog-dev
```

## Building the Project

### Manual Build

```bash
mkdir -p build
cd build
cmake ..
make
```

### Using Claude Skills

```bash
# Build the project
/build

# Clean build artifacts
/clean
```

The build process creates two executables:
- `build/Server`: The game server
- `build/Client`: The game client

## Running the Project

### Server

Start the game server:
```bash
./build/Server
```

The server listens on `0.0.0.0:1234` for client connections.

### Client

Start a game client:
```bash
./build/Client
```

Clients connect to `127.0.0.1:1234` by default.

### Using Claude Skills

```bash
# Run server only
/run-server

# Run a single client
/run-client

# Run full dev environment (1 server + 4 clients)
/dev
```

## Development Workflow

### Full Development Environment

The `/dev` skill (or `./dev` script) provides a complete local development environment:
1. Builds the project
2. Starts 1 server instance
3. Starts 4 client instances (with 1-second delays between each)
4. Waits for Enter key to terminate all processes gracefully

This simulates a multiplayer environment for testing 4-player co-op gameplay.

### Quick Iteration

For rapid iteration during development:
1. Make code changes
2. Run `/build` to rebuild
3. Run `/run-server` or `/run-client` as needed
4. The fast build/boot cycle enables quick testing without save-states

## Testing

Currently, there is no formal test suite. The `/test` skill is available for when tests are added.

### Future Testing Strategy

Based on the Tiger Style approach:
- Extensive assertions in code to crash on invalid states
- Fuzz testing for critical systems (networking, asset loading, etc.)
- Integration tests for multiplayer scenarios

## Planned Features

See DESIGN.md for detailed requirements. Key upcoming features:

### Asset Pipeline
- **Tiled Integration**: Import maps from Tiled with custom metadata
- **Aseprite Integration**: Seamless sprite sheet importing
- **Rive Integration**: UI building and animation

### Gameplay Systems
- **Update/Render Loops**: Event-based architecture with 60 FPS update loop
- **Multiplayer**: 4-player co-op PvE with matchmaking
- **Physics**: TBD - may use a library if needed
- **Scene Graph**: Event-driven scene transitions

### Editor Support
- **Level Editor**: Tiled-based with custom object metadata
- **Fast Reload**: Quick build/boot for rapid playtesting

## Common Tasks with Claude Code

### Adding New Features

```
"Add a new rendering system component"
"Implement player movement with collision detection"
"Create an asset loader for Tiled maps"
```

Claude will explore the codebase, understand existing patterns, and implement features following the engine's architecture.

### Debugging

```
"Why are clients failing to connect to the server?"
"Find all network packet handling code"
"Trace the window event loop"
```

### Refactoring

```
"Extract common initialization code from client_main and server_main"
"Refactor the NetworkClient to use async events"
```

## Git Workflow

Current branch: `main`

The repository uses `main` as the primary branch. When creating pull requests, target `main`.

## Additional Resources

- **DESIGN.md**: Comprehensive design document with requirements, decisions, and work breakdown
- **Tiger Style**: https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md
- **ENet Documentation**: http://enet.bespin.org/
- **Tiled**: https://www.mapeditor.org/
- **Aseprite**: https://www.aseprite.org/
- **Rive**: https://rive.app/

## Notes for Claude

- This engine prioritizes **performance and debuggability** over convenience
- Follow the **Tiger Style** approach: assert aggressively, fail fast
- Maintain the **event-based architecture** - avoid coupling to update/render loops
- Keep code **simple and direct** - avoid unnecessary abstractions
- **No hot reload** - rely on fast build/boot cycles instead
- Games run at **fixed 60 FPS** - render loop can drop frames, update loop never does
