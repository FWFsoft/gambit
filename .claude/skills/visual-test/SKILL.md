---
name: visual-test
description: Run visual testing where Claude can see the game window and provide inputs
---

# Visual Testing Skill

This skill enables visual testing of the Gambit game client. The client runs in test mode, captures screenshots intermittently, and reads input commands from a file. Claude can:
- View screenshots to see the game state
- Write input commands to control the game
- Verify that menu navigation and gameplay work correctly

## How It Works

1. **Screenshot Capture**: Client captures screenshots every 30 frames (0.5s at 60fps) to `test_output/frame_latest.png`
2. **Input Commands**: Client reads commands from `test_input.txt` and simulates keyboard input
3. **Claude Analysis**: Claude reads screenshots, analyzes game state, and writes new input commands
4. **Progression Testing**: Focus is on verifying game progression (menu navigation, gameplay) rather than pixel-perfect rendering

## Usage

### Autonomous Interactive Testing

```bash
/visual-test
```

This launches an autonomous test where Claude:
1. Starts server and client in background with test mode
2. Monitors screenshots as they're captured
3. Reads `test_output/frame_latest.png` to see game state
4. Writes commands to `test_input.txt` based on what it sees
5. Client picks up commands immediately (file is reopened each frame)
6. Claude verifies game progression and reports results

### Manual Testing

You can also run the test script directly and interact manually:

```bash
./.claude/skills/visual-test/interactive_test.sh
```

Then:
- Read `test_output/frame_latest.png` to see what's happening
- Append commands to `test_input.txt` to control the game
- Client executes them automatically

## Input Command Format

The `test_input.txt` file supports these commands:

```
# Wait for N frames (60 = 1 second at 60fps)
WAIT 60

# Press key down
KEY_DOWN W

# Release key
KEY_UP W

# Take a named screenshot
SCREENSHOT my_test_frame

# Comments start with #
```

### Supported Keys

W, A, S, D, Up, Down, Left, Right, Space, Enter, Escape, E, I, Tab

## Example Test Script

```
# Wait for game to load
WAIT 120

# Navigate main menu
KEY_DOWN Down
KEY_UP Down
WAIT 10

# Press Enter to select
KEY_DOWN Enter
KEY_UP Enter
WAIT 60

# Move player forward
KEY_DOWN W
WAIT 30
KEY_UP W

# Capture verification screenshot
SCREENSHOT player_moved
```

## Output Files

- `test_output/frame_latest.png` - Most recent periodic screenshot
- `test_output/*.png` - Named screenshots from SCREENSHOT commands

## Notes

- Client must be started with `--test-mode` flag
- Server must be running on localhost:1234
- Screenshots are captured automatically every 30 frames
- Input commands are read and processed each frame
- Focus on gameplay progression, not pixel-perfect verification
