# Quickstart: WebAssembly Cross-Platform Build

**Feature**: 001-wasm-cross-platform

## Prerequisites

### Emscripten SDK

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source emsdk_env.sh  # Add to shell profile for persistence
```

Verify: `emcc --version` should print the Emscripten version.

### Existing Dependencies (Native)

All existing native dependencies remain required for native builds. See README.md.

## Building

### Native Build (unchanged)

```bash
mkdir -p build && cd build
cmake ..
make
```

### WASM Build

```bash
mkdir -p build-wasm && cd build-wasm
emcmake cmake ..
make
```

Output: `build-wasm/WasmClient.html`, `WasmClient.js`, `WasmClient.wasm`, `WasmClient.data`

### Both Targets

```bash
# Native
mkdir -p build && cd build && cmake .. && make && cd ..

# WASM
mkdir -p build-wasm && cd build-wasm && emcmake cmake .. && make && cd ..
```

## Running

### Browser (WASM) — Embedded Server Mode

Serve the WASM build with any static file server:

```bash
cd build-wasm
python3 -m http.server 8080
```

Open `http://localhost:8080/WasmClient.html` in a browser. The game runs with an embedded server — no external server process needed.

### Browser (WASM) — Networked Mode

1. Start the native server: `./build/Server`
2. Open `http://localhost:8080/WasmClient.html` in a browser
3. The browser client connects to the server via WebSocket

### Mixed Testing (Native + Browser)

```bash
# Terminal 1: Start server (accepts ENet + WebSocket)
./build/Server

# Terminal 2: Start native client
./build/Client

# Browser: Open WasmClient.html
# Both clients connect to the same server
```

### Development Environment (planned)

```bash
./scripts/dev-wasm.sh
# Starts: 1 server + 2 native clients + serves WASM build + opens browser
```

## Testing

### Native Tests (unchanged)

```bash
cd build && ctest
```

### WASM Tests (headless browser)

```bash
cd build-wasm && node WasmClient.js  # Basic smoke test
# Or with headless browser:
# npx playwright test  (future integration)
```

## Troubleshooting

### "emcmake not found"
Run `source /path/to/emsdk/emsdk_env.sh` to activate the Emscripten environment.

### Black screen in browser
Check browser developer console for WebGL2 errors. Ensure the browser supports WebGL2.

### Assets not loading
Verify the `.data` file is served alongside `.html`/`.js`/`.wasm`. All four files must be in the same directory.

### Connection refused (networked mode)
Ensure the native server is running and WebSocket port is accessible. Check for CORS issues if serving from a different origin.
