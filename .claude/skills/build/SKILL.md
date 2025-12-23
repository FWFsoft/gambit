---
name: build
description: Build the Gambit game engine (Server and Client executables). Use when the user wants to compile the project, needs to rebuild after code changes, or before running the server or client.
---

# Build Gambit

Builds both the Server and Client executables for the Gambit game engine.

## Instructions

1. Run the build command:
   ```bash
   make build
   ```

## Expected Output

On success, you should see:
- `build/Server` - The game server executable
- `build/Client` - The game client executable

## Error Handling

If CMake fails:
- Ensure dependencies are installed: SDL2, ENet, spdlog
- On macOS: `brew install cmake sdl2 enet spdlog`
- On Linux: `sudo apt-get install cmake libsdl2-dev libenet-dev libspdlog-dev`

If make fails:
- Check for compilation errors in the output
- Ensure C++17 compiler is available

## Notes

- The build is incremental - only changed files are recompiled
- Use the `clean` skill before building if you need a fresh rebuild
