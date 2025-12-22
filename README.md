# Gambit Game Engine

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

**Manual Build**:
```bash
mkdir -p build && cd build
cmake .. && make
```

**Manual Run**:
```bash
./build/Server  # Start server
./build/Client  # Start client
```

## Using Claude Code Skills

This project includes Claude Code skills that streamline development workflows. These skills work in both **Claude Code** (CLI) and **Cursor** (IDE).

### Available Skills

| Skill | Description |
|-------|-------------|
| `/build` | Build both Server and Client executables |
| `/clean` | Remove build artifacts for a fresh build |
| `/run-server` | Start the game server on 0.0.0.0:1234 |
| `/run-client` | Start a single client (connects to 127.0.0.1:1234) |
| `/dev` | Full dev environment: builds + runs 1 server + 4 clients |
| `/test` | Run tests (placeholder for when tests are added) |

### How Skills Work

Skills follow the Claude Agent Skills format with YAML frontmatter. Each skill is a directory in `.claude/skills/` containing a `SKILL.md` file:

```
.claude/skills/
├── build/
│   └── SKILL.md
├── dev/
│   └── SKILL.md
└── ...
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
/build
/dev
/clean
/run-server
/run-client
/test
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

### Full Rebuild

```bash
/clean
/build
```

## Project Structure

```
gambit/
├── .claude/
│   └── skills/         # Claude Code skills (SKILL.md format)
│       ├── build/
│       ├── clean/
│       ├── dev/
│       ├── run-client/
│       ├── run-server/
│       └── test/
├── CMakeLists.txt      # Build configuration
├── CLAUDE.md           # Comprehensive guide for Claude
├── DESIGN.md           # Design requirements and decisions
├── README.md           # This file
├── include/            # Public headers
├── src/                # Implementation files
│   ├── client_main.cpp # Client entry point
│   └── server_main.cpp # Server entry point
└── build/              # Build output (generated)
```

## Architecture

- **Language**: C++17
- **Build System**: CMake
- **Graphics**: SDL2 + OpenGL
- **Networking**: ENet (P2P with client-server topology)
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
