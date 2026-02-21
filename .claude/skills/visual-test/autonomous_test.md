# Autonomous Visual Testing with Claude

## How It Works

Claude can run fully autonomous visual tests by:

1. **Launching Playwright** to open the WASM client in headless Chromium
2. **Observing screenshots** periodically (updated every 1s)
3. **Writing commands dynamically** based on what it sees
4. **Reading console logs** for game state information
5. **Verifying progression** and reporting results

## The Key Insight

The Python script polls `test_input.txt` **every 200ms**, which means:
- Claude can append commands while the test is running
- The script picks them up on the next poll cycle (no restart needed)
- This enables a true interactive testing loop
- Console output is also captured for debugging

## Workflow Example

### 1. Claude Starts the Test Environment

```bash
# Ensure HTTP server is running in build-wasm6/
# Then run the visual test
./.claude/skills/visual-test/run.sh --idle-timeout 30
```

This:
- Opens headless Chromium to the WASM client
- Waits for the WASM module to load
- Starts capturing screenshots every 1s
- Polls test_input.txt for commands

### 2. Claude Observes Initial State

After a few seconds, Claude reads:
```bash
Read tool: test_output/frame_latest.png
```

Claude sees: "Title screen with menu options visible"

### 3. Claude Navigates Menu

Claude appends to test_input.txt:
```
# Select "Start Game"
KEY_PRESS Enter
WAIT 1000
SCREENSHOT after_start
```

### 4. Claude Checks Next Screenshot

After waiting, Claude reads the screenshot again and sees:
"Character select screen showing 4 characters"

### 5. Claude Selects Character

Claude appends:
```
# Select character 2
KEY_PRESS Down
WAIT 200
KEY_PRESS Enter
WAIT 2000
SCREENSHOT character_selected
```

### 6. Claude Continues Testing

Claude keeps observing and commanding until test objectives are met:
- Menu navigation works
- Character selection works
- Game world loads
- Player can move

### 7. Claude Reports Results

```
Visual Test Complete

Results:
- Title screen rendered correctly
- Menu navigation: PASS
- Character selection: PASS
- Game world loading: PASS
- Player movement: PASS

Screenshots saved in test_output/
Console log: test_output/console.log
```

## Commands Claude Can Use

While the test is running, Claude can:

### Read Current State
```bash
Read: test_output/frame_latest.png
```

### Add Input Commands
```bash
Edit: test_input.txt
# Append new commands:
KEY_DOWN W
WAIT 500
KEY_UP W
SCREENSHOT moved_forward
```

### Check Console Logs
```bash
Read: test_output/console.log
```

## Example: Testing Character Movement

```
1. Claude launches: run.sh --idle-timeout 30 (background)
2. Wait 3s
3. Read screenshot -> sees game world
4. Append: "KEY_DOWN W" + "WAIT 500" + "KEY_UP W"
5. Wait 2s
6. Read screenshot -> verifies player moved up
7. Append: "KEY_DOWN A" + "WAIT 500" + "KEY_UP A"
8. Wait 2s
9. Read screenshot -> verifies player moved left
10. Report: "Movement system working correctly!"
```

## Benefits

- **No pre-scripting required** - Claude reacts to what it sees
- **Catches visual bugs** - Claude sees actual rendered output
- **Flexible testing** - Can adapt to unexpected states
- **Natural language results** - Claude reports findings in plain English
- **Console access** - Game logs captured alongside screenshots
- **No native build needed** - Uses the WASM build in headless Chromium
- **No separate server** - WASM client has embedded server

## Current Limitations

- **Timing sensitive** - Need to wait for screenshots to update (~1s interval)
- **No rollback** - Can't undo commands (game keeps running)
- **Visual only** - No direct game state introspection (screenshots + console logs)
- **Requires HTTP server** - Must be serving build-wasm6/ on port 8080

## Future Enhancements

- [ ] Game state output to JSON (Claude reads both visual + data)
- [ ] Automatic screenshot diff detection
- [ ] Expected state verification (golden images)
- [ ] Test recording/playback
