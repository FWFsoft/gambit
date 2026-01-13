# Autonomous Visual Testing with Claude

## How It Works

Claude can run fully autonomous visual tests by:

1. **Launching the test environment** (server + client in background)
2. **Observing screenshots** periodically
3. **Writing commands dynamically** based on what it sees
4. **Verifying progression** and reporting results

## The Key Insight

The `TestInputReader` reopens `test_input.txt` **every frame**, which means:
- Claude can append commands while the client is running
- The client picks them up immediately (no restart needed)
- This enables a true interactive testing loop

## Workflow Example

### 1. Claude Starts the Test Environment

```bash
# Runs in background
./.claude/skills/visual-test/interactive_test.sh
```

This starts:
- Server on localhost:1234
- Client with --test-mode flag
- Screenshot capture every 0.5s

### 2. Claude Observes Initial State

After 2-3 seconds, Claude reads:
```bash
Read tool: test_output/frame_latest.png
```

Claude sees: "Black screen, game is loading..."

### 3. Claude Waits for Load

Claude appends to test_input.txt:
```
# Wait for game to fully load
WAIT 60
SCREENSHOT loaded_state
```

### 4. Claude Checks Next Screenshot

After waiting, Claude reads the screenshot again and sees:
"Title screen with 'Start Game' and 'Settings' options visible"

### 5. Claude Navigates Menu

Claude appends:
```
# Select "Start Game"
KEY_DOWN Enter
KEY_UP Enter
WAIT 60
SCREENSHOT after_start
```

### 6. Claude Verifies Character Select

Claude reads next screenshot and sees:
"Character select screen showing 4 characters"

Claude appends:
```
# Select character 2
KEY_DOWN Down
KEY_UP Down
WAIT 10
KEY_DOWN Enter
KEY_UP Enter
WAIT 60
SCREENSHOT character_selected
```

### 7. Claude Continues Testing

Claude keeps observing and commanding until test objectives are met:
- Menu navigation works ✓
- Character selection works ✓
- Game world loads ✓
- Player can move ✓

### 8. Claude Reports Results

```
✅ Visual Test Complete

Results:
- Title screen rendered correctly
- Menu navigation: PASS
- Character selection: PASS
- Game world loading: PASS
- Player movement: PASS

Screenshots saved:
- loaded_state.png
- after_start.png
- character_selected.png
- final_state.png
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
WAIT 30
KEY_UP W
SCREENSHOT moved_forward
```

### Check Logs
```bash
Read: test_output/client.log
Read: test_output/server.log
```

### Stop Test
```bash
# Kill the background processes
pkill -f "interactive_test.sh"
```

## Example: Testing Character Movement

```
1. Claude launches: interactive_test.sh (background)
2. Wait 3s
3. Read screenshot → sees game world
4. Append: "KEY_DOWN W" + "WAIT 30" + "KEY_UP W"
5. Wait 2s
6. Read screenshot → verifies player moved up
7. Append: "KEY_DOWN A" + "WAIT 30" + "KEY_UP A"
8. Wait 2s
9. Read screenshot → verifies player moved left
10. Report: "Movement system working correctly!"
```

## Benefits

✅ **No pre-scripting required** - Claude reacts to what it sees
✅ **Catches visual bugs** - Claude sees actual rendered output
✅ **Flexible testing** - Can adapt to unexpected states
✅ **Natural language results** - Claude reports findings in plain English
✅ **Faster iteration** - No need to write brittle test scripts

## Current Limitations

- ⏱️ **Timing sensitive** - Need to wait for screenshots to update
- 🔄 **No rollback** - Can't undo commands (client keeps running)
- 🖱️ **Keyboard only** - No mouse support yet
- 📊 **Visual only** - No direct game state introspection (just screenshots)

## Future Enhancements

- [ ] Game state output to JSON (Claude reads both visual + data)
- [ ] Mouse input support
- [ ] Automatic screenshot diff detection
- [ ] Expected state verification (golden images)
- [ ] Test recording/playback
