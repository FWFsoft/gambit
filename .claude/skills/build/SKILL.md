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

## Dependency toolchain

The same selection applies to `make build`, `make test` and
`make test-coverage`: explicit `CMAKE_TOOLCHAIN_FILE` (environment or Make
argument), otherwise `VCPKG_ROOT`'s standard toolchain, otherwise retain the
existing cached toolchain, or use system dependencies for a fresh build.

```bash
VCPKG_ROOT="/path/to/vcpkg" make build
make build CMAKE_TOOLCHAIN_FILE="/path with spaces/custom.cmake"
make build CMAKE_TOOLCHAIN_FILE=  # explicit system packages, overrides VCPKG_ROOT
```

Missing selected toolchains fail; no install or developer-specific fallback.
Switching a configured build's toolchain/provider requires `make clean`
first (deletes all `build/` artifacts). See README for complete setup notes.

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
