# Gambit Game Engine

> **Alien Arena prototype:** see [PLAYTEST.md](PLAYTEST.md) for how to play, run a 2 to 4 player match, and tune it.

A C++ game engine for building 2D isometric ARPG games with a focus on debuggability, stability, and performance.

## Quick Start

### Prerequisites

Install required dependencies:

**macOS (Homebrew)**:
```bash
brew install cmake sdl2 enet spdlog
```

**Linux (Ubuntu/Debian)**:
```bash
sudo apt-get install cmake libsdl2-dev libenet-dev libspdlog-dev
```

### Building and Running

**Using Claude Code or Cursor Skills**:
```bash
/build      # Build the project
/dev        # Run full dev environment (1 server + 4 clients)
```

**Portable Make build** (requires CMake and a native build tool on PATH):
```bash
make build                         # fresh build: system-installed dependencies
VCPKG_ROOT="/path/to/vcpkg" make build
make build CMAKE_TOOLCHAIN_FILE="/path with spaces/custom.cmake"
make build CMAKE_TOOLCHAIN_FILE=   # explicitly use system deps, even with VCPKG_ROOT
```

`make build`, `make test` and `make test-coverage` use the same toolchain
selection: explicitly set `CMAKE_TOOLCHAIN_FILE` (environment or Make command
line), otherwise `$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`, otherwise the
existing build's cached toolchain, or system dependencies for a fresh build.
Relative toolchain paths resolve from the repository root; quote paths with
spaces. A configured but missing toolchain is an error, not a fallback.
No developer-specific vcpkg location is assumed and nothing is auto-installed.
System dependencies must satisfy **all** native `find_package`/library checks
in `CMakeLists.txt` (the prerequisite examples above are not an exhaustive
package list); vcpkg dependencies are listed in `vcpkg.json`.

Changing between toolchains, or between toolchain and system dependencies,
requires `make clean` first. This removes **all of `build/`**, including its
cache and reports; review/save artifacts before cleaning. With neither
variable set, incremental builds retain the existing cached toolchain rather
than silently switching dependency providers.

```bash
make clean
make test-coverage CMAKE_TOOLCHAIN_FILE="/path/to/custom.cmake"
# Coverage additionally requires CTest, lcov and genhtml.
```

**Direct CMake build** (manual toolchain/cache management):
```bash
cmake -S . -B build
cmake --build build --parallel 8
```

The Make recipe regression tests require only Python 3 and GNU Make, not
CMake or C++ dependencies:
```bash
python3 -m unittest discover -s tests -p test_portable_build.py -v
```

**Manual Run**:
```bash
./build/Server  # Start server
./build/Client  # Start client
```

## Lightweight Logic Tests

Run `make test-lightweight` with Python 3 and a C++17 compiler, without graphics
or network build dependencies. This is a limited subset, **not a full game
build**; see [exact scope and runner checks](docs/LIGHTWEIGHT_TESTS.md).

## Using Claude Code Skills

This project includes Claude Code skills that streamline development workflows. These skills work in both **Claude Code** (CLI) and **Cursor** (IDE).

### Available Skills

| Skill | Description |
|-------|-------------|
| `/build` | Build both Server and Client executables |
| `/clean` | Remove build artifacts for a fresh build |
| `/test` | Run all tests for the game engine |
| `/dev` | Full dev environment: builds + runs 1 server + 4 clients |
| `/run-server` | Start the game server on 0.0.0.0:1234 |
| `/run-client` | Start a single client (connects to 127.0.0.1:1234) |
| `/pre-commit` | Run pre-commit hooks (clang-format, clang-tidy) on all files |
| `/search <symbol>` | Find definitions and usages of a symbol across the entire codebase |

### How Skills Work

Skills follow the Claude Agent Skills format with YAML frontmatter. Each skill is a directory in `.claude/skills/` containing a `SKILL.md` file:

```
.claude/skills/
├── build/          # Build the project
├── clean/          # Clean build artifacts
├── dev/            # Development environment (1 server + 4 clients)
├── pre-commit/     # Run pre-commit hooks (clang-format, clang-tidy)
├── run-client/     # Run a single client
├── run-server/     # Run the server
├── search/         # Advanced code search tool
└── test/           # Run tests
```

Each `SKILL.md` has frontmatter that tells Claude when to use the skill:
```yaml
---
name: build
description: Build the Gambit game engine (Server and Client executables). Use when the user wants to compile the project...
---
```

When you invoke a skill with `/skill-name` or ask Claude to perform a related task, Claude automatically uses the appropriate skill.

### Using Skills in Claude Code (CLI)

1. **Install Claude Code**:
   ```bash
   npm install -g @anthropic/claude-code
   ```

2. **Navigate to the project**:
   ```bash
   cd /path/to/gambit
   ```

3. **Start Claude Code**:
   ```bash
   claude-code
   ```

4. **Use skills in conversation**:
   ```
   You: "Build the project"
   Claude: [Uses /build skill automatically]

   You: "Run the dev environment"
   Claude: [Uses /dev skill to start server + 4 clients]

   You: "/dev"
   Claude: [Directly executes the dev skill]
   ```

### Using Skills in Cursor

1. **Open the project in Cursor**:
   ```bash
   cursor /path/to/gambit
   ```

2. **Use skills via Chat (Cmd+L / Ctrl+L)**:
   ```
   You: "Build the project using the skill"
   Cursor: [Suggests running /build]

   You: "/build"
   Cursor: [Executes the build skill]

   You: "/dev"
   Cursor: [Runs the full dev environment]
   ```

3. **Skills appear in the command palette**:
   - Press `Cmd+Shift+P` (macOS) or `Ctrl+Shift+P` (Windows/Linux)
   - Type "Claude" to see available skills
   - Select a skill to execute it

### Asking Claude to Use Skills

You can ask Claude to use skills in natural language:

```
"Build the project"
"Run the development environment"
"Clean the build artifacts"
"Start just the server"
"Run a client instance"
```

Claude will automatically recognize when to use the appropriate skill.

### Direct Skill Invocation

You can also invoke skills directly by typing the skill name with a `/` prefix:

```
/build              # Build the project
/clean              # Clean build artifacts
/test               # Run all tests
/dev                # Full dev environment (1 server + 4 clients)
/run-server         # Start the game server
/run-client         # Start a game client
/pre-commit         # Run pre-commit hooks (clang-format, clang-tidy)
/search <symbol>    # Search for definitions and usages
```

## Development Workflow

### Quick Iteration

1. Make code changes
2. `/build` to rebuild
3. `/run-server` or `/run-client` to test

### Multiplayer Testing

Use `/dev` to simulate a 4-player co-op environment:
- Builds the project
- Starts 1 server
- Starts 4 clients (1-second delays between each)
- Press Enter to terminate all processes

### Code Search

Use `/search` to find symbols across the entire codebase:

```bash
/search Window          # Find all usages of Window class
/search pollEvents      # Find all usages of pollEvents method
/search NetworkClient   # Find all usages of NetworkClient
```

The search tool:
- Finds definitions (classes, functions, structs, etc.)
- Locates all usages across code, tests, examples, benchmarks
- Shows file paths, line numbers, and context
- Groups results by category (definitions, code, tests, etc.)
- More powerful than grep - understands C++ code structure

### Full Rebuild

```bash
/clean
/build
```

## Server-dependent integration test (opt-in)

`HeadlessMovement` is not registered in default test builds. To enable it,
configure a separate native build (using the same dependency/toolchain settings
as your regular build):

```bash
cmake -S . -B build-integration \
  -DGAMBIT_ENABLE_SERVER_INTEGRATION_TESTS=ON
cmake --build build-integration --target Server test_headless_movement -j8
# Terminal 1, from repository root (server on localhost:1234):
./build-integration/Server
# Terminal 2, from repository root:
(cd build-integration && ctest -L integration --output-on-failure)
```

The example uses system dependencies; if needed, add
`-DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/toolchain.cmake` when configuring.
The option defaults to OFF. Label selection alone does not enable tests.
CTest preserves the `integration;manual` labels and repository-root working
directory, but does not start the server. Opted-in builds also include this test
in unfiltered CTest runs. To remove it again, configure with
`-DGAMBIT_ENABLE_SERVER_INTEGRATION_TESTS=OFF`.

Registration regression (no game dependencies/server):
`python3 tests/test_server_integration_config.py -v`. CMake/CTest checks use a
stub and explicitly skip when tools are missing; this does not validate real
multiplayer movement.

## Project Structure

```
gambit/
├── .claude/
│   └── skills/         # Claude Code skills (SKILL.md format)
│       ├── build/      # Build the project
│       ├── clean/      # Clean build artifacts
│       ├── dev/        # Development environment (1 server + 4 clients)
│       ├── pre-commit/ # Run pre-commit hooks (clang-format, clang-tidy)
│       ├── run-client/ # Run a single client
│       ├── run-server/ # Run the server
│       ├── search/     # Advanced code search tool
│       └── test/       # Run tests
├── CMakeLists.txt      # Build configuration
├── ARCHITECTURE.md     # Detailed architecture documentation
├── CLAUDE.md           # Comprehensive guide for Claude
├── DESIGN.md           # Design requirements and decisions
├── KNOWN_BUGS.md       # List of known issues
├── README.md           # This file
├── include/            # Public headers
├── src/                # Implementation files
│   ├── client_main.cpp # Client entry point
│   └── server_main.cpp # Server entry point
└── build/              # Build output (generated)
```

## WebAssembly Build (Browser)

The engine can be compiled to WebAssembly to run in modern browsers.

### Prerequisites

**Install Emscripten SDK**:
```bash
# Clone the Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate the latest SDK
./emsdk install latest
./emsdk activate latest

# Add to your shell (add to ~/.bashrc or ~/.zshrc for persistence)
source ./emsdk_env.sh

# Verify installation
emcc --version
```

### Building for WebAssembly

```bash
# Create a separate build directory for WASM
mkdir -p build-wasm && cd build-wasm

# Configure with Emscripten
emcmake cmake ..

# Build
make

# Output files:
#   WasmClient.html  - HTML shell
#   WasmClient.js    - JavaScript glue code
#   WasmClient.wasm  - WebAssembly binary
#   WasmClient.data  - Preloaded assets
```

### Running in Browser

```bash
# Serve the build directory
cd build-wasm
python3 -m http.server 8080

# Open in browser
# http://localhost:8080/WasmClient.html
```

**Browser Requirements**:
- WebGL2 support (Chrome 56+, Firefox 51+, Safari 15+, Edge 79+)
- SharedArrayBuffer (for some features, requires HTTPS or localhost)

### WASM Build Features

| Feature | Status |
|---------|--------|
| Rendering (WebGL2) | ✓ Supported |
| Input (keyboard/mouse) | ✓ Supported |
| Audio (SDL2_mixer) | ✓ Supported |
| Networking | Stub (single-player only) |
| Embedded Server | Planned (Phase 4) |

### Using Claude Code Skills

```bash
/build-wasm    # Build the WASM client (planned)
/dev-wasm      # Full dev environment with browser client (planned)
```

## Architecture

- **Language**: C++17
- **Build System**: CMake
- **Graphics**: SDL2 + OpenGL (native) / WebGL2 (browser)
- **Networking**: ENet (native) / WebSocket (browser, planned)
- **Logging**: spdlog

See [DESIGN.md](DESIGN.md) for detailed design decisions and [CLAUDE.md](CLAUDE.md) for a comprehensive guide to the codebase.

## Key Features (Planned)

- **Event-Based Architecture**: Games operate over async events, not update/render loops
- **Fixed 60 FPS**: Update loop always fires; render loop drops frames as needed
- **Fast Build/Boot/Load**: Optimized for rapid iteration
- **Tiger Style Development**: Assert aggressively, fail fast
- **Asset Pipeline**: Tiled maps, Aseprite sprites, Rive UI
- **Multiplayer**: 4-player co-op PvE with matchmaking

## Working with Claude Code

### Getting Started

1. **Ask Claude to explain the codebase**:
   ```
   "Explain the networking architecture"
   "How does the window system work?"
   "Show me the event loop implementation"
   ```

2. **Request features**:
   ```
   "Add player movement with WASD keys"
   "Implement collision detection"
   "Create an asset loader for Tiled maps"
   ```

3. **Debug issues**:
   ```
   "Why are clients failing to connect?"
   "Find all network packet handling code"
   "Trace the rendering pipeline"
   ```

### Best Practices

- **Let Claude use skills**: Claude will automatically select the right skill for your request
- **Use `/dev` for testing**: Simulates full multiplayer environment
- **Leverage CLAUDE.md**: Claude reads this file to understand the project
- **Ask for explanations**: Claude can explain any part of the codebase

### Customizing Skills

Skills follow the SKILL.md format in `.claude/skills/`. You can:

1. **Edit existing skills**:
   ```bash
   vim .claude/skills/build/SKILL.md
   ```

2. **Create new skills**:
   ```bash
   mkdir -p .claude/skills/my-skill
   cat > .claude/skills/my-skill/SKILL.md << 'EOF'
   ---
   name: my-skill
   description: What this skill does and when to use it
   ---

   # My Skill

   ## Instructions
   1. Step one
   2. Step two
   EOF
   ```

3. **Use skills in combination**:
   ```
   "Clean the build and rebuild everything"
   Claude: [Uses /clean then /build]
   ```

For more on creating skills, see the [Claude Agent Skills documentation](https://platform.claude.com/docs/en/agents-and-tools/agent-skills/overview).

## Contributing

When adding features:
1. Follow the Tiger Style approach (assert on invalid states)
2. Maintain the event-based architecture
3. Keep code simple and direct
4. Update tests when available
5. Run `/build` to verify compilation

## Resources

- **Tiger Style**: https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md
- **ENet**: http://enet.bespin.org/
- **SDL2**: https://www.libsdl.org/
- **Tiled**: https://www.mapeditor.org/
- **Aseprite**: https://www.aseprite.org/
- **Rive**: https://rive.app/

## License

[Add your license here]
