---
name: visual-test
description: Run visual testing where Claude can see the game window and provide inputs
---

# Visual Testing Skill

This skill enables visual testing of the Gambit game client using Playwright to control the WASM build in headless Chromium. Claude can:
- View screenshots to see the game state
- Write input commands to control the game
- Verify that menu navigation and gameplay work correctly
- Read console logs from the game

## How It Works

1. **Playwright + WASM**: The WASM client runs in headless Chromium via Playwright - no native build or separate server needed
2. **Screenshot Capture**: Periodic screenshots every 1s to `test_output/frame_latest.png`
3. **Input Commands**: Python script reads commands from `test_input.txt` and dispatches keyboard/mouse input via Playwright
4. **Console Capture**: Game console output is captured to `test_output/console.log`
5. **Claude Analysis**: Claude reads screenshots, analyzes game state, and writes new input commands

## Prerequisites

- WASM build in `build-wasm6/` (run `cd build-wasm6 && emcmake cmake .. && make`)
- HTTP server on port 8080: `cd build-wasm6 && python3 -m http.server 8080`
- Python with `uv` available
- Playwright Chromium browser: `uv run --with playwright python -m playwright install chromium`

## Usage

### Autonomous Interactive Testing

```bash
/visual-test
```

This launches an autonomous test where Claude:
1. Runs Playwright to open the WASM client in headless Chromium
2. Waits for the WASM module to load
3. Reads `test_output/frame_latest.png` to see game state
4. Writes commands to `test_input.txt` based on what it sees
5. Python script picks up commands every 200ms
6. Claude verifies game progression and reports results

### Direct Invocation

```bash
./.claude/skills/visual-test/run.sh
```

With options:
```bash
./.claude/skills/visual-test/run.sh --url http://localhost:8080/WasmClient.html --timeout 180 --idle-timeout 10
```

## Input Command Format

The `test_input.txt` file supports these commands:

```
# Wait for N milliseconds
WAIT 2000

# Press key down
KEY_DOWN W

# Release key
KEY_UP W

# Press and release key (convenience)
KEY_PRESS Enter

# Take a named screenshot
SCREENSHOT my_test_frame

# Move mouse to canvas-relative coordinates
MOUSE_MOVE 400 300

# Click at canvas-relative coordinates
MOUSE_CLICK 400 300

# Comments start with #
```

### Supported Keys

W, A, S, D, E, I, F1, F2, F3, Up, Down, Left, Right, Space, Enter, Escape, Tab

## Example Test Script

```
# Wait for game to settle after load
WAIT 2000

# Navigate main menu
KEY_PRESS Down
WAIT 500

# Press Enter to select
KEY_PRESS Enter
WAIT 1000

# Move player forward
KEY_DOWN W
WAIT 500
KEY_UP W

# Capture verification screenshot
SCREENSHOT player_moved
```

## Output Files

- `test_output/frame_latest.png` - Most recent periodic screenshot (updated every 1s)
- `test_output/*.png` - Named screenshots from SCREENSHOT commands
- `test_output/final_state.png` - Screenshot captured when test ends
- `test_output/console.log` - Game console output captured from the browser

## Notes

- No separate game server needed - the WASM client has an embedded server
- WAIT values are in **milliseconds** (not frames)
- The Python script polls `test_input.txt` every 200ms for new commands
- After 5s with no new commands, the test finishes automatically (configurable via `--idle-timeout`)
- Overall timeout defaults to 180s (configurable via `--timeout`)
- Focus on gameplay progression, not pixel-perfect verification
