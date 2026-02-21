#!/bin/bash
# Visual Test Runner - Playwright + WASM
# Runs the WASM client in headless Chromium via Playwright

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
cd "$PROJECT_DIR"

mkdir -p test_output

# Create default test_input.txt if missing
if [ ! -f "test_input.txt" ]; then
    cat > test_input.txt << 'EOF'
# Autonomous navigation test via Playwright + WASM
# WAITs are in milliseconds

# Wait for game to settle after load
WAIT 2000

# Get past title screen
KEY_PRESS Enter
WAIT 1000

# Character select - pick first character
SCREENSHOT character_select_start
KEY_PRESS Enter

# Wait for game world to load
WAIT 3000
SCREENSHOT spawn_location

# Enable debug rendering with F1
KEY_PRESS F1
WAIT 500
SCREENSHOT debug_enabled

# Explore east
KEY_DOWN D
WAIT 1000
KEY_UP D
SCREENSHOT explored_east

# Explore north
KEY_DOWN W
WAIT 2000
KEY_UP W
SCREENSHOT explored_north

# Move east more
KEY_DOWN D
WAIT 3000
KEY_UP D
SCREENSHOT moved_east

SCREENSHOT final_position
EOF
    echo "Created default test_input.txt"
fi

# Run the Playwright test script
uv run --with playwright python "$SCRIPT_DIR/playwright_test.py" "$@"
