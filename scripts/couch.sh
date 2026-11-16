#!/bin/bash
# Gambit couch co-op: 1 server + 2 clients on this machine, side by side,
# one game controller each. Ctrl+C stops everything.
#
#   Player 1: left window,  first controller  (keyboard + mouse also work)
#   Player 2: right window, second controller (silent, so the music and
#             effects are not doubled)
#
# Each window is 1280x720, so both fit side by side only on a desktop at
# least 2560 wide. On a smaller one the windows overlap: drag them where you
# want them, or pass the second window's position:
#
#   ./scripts/couch.sh [P2_X] [P2_Y]      (default 1280 60)

set -e

P2_X="${1:-1280}"
P2_Y="${2:-60}"

PIDS=()

cleanup() {
    echo ""
    echo "Shutting down all processes..."
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    echo "All processes stopped."
    exit 0
}

trap cleanup SIGINT SIGTERM EXIT

if [ ! -f "build/Server" ] || [ ! -f "build/Client" ]; then
    echo "Error: Executables not found. Run 'make build' first."
    exit 1
fi

echo "Starting Gambit couch co-op..."
echo "  - 1 Server (0.0.0.0:1234)"
echo "  - Player 1: left window,  controller 1"
echo "  - Player 2: right window, controller 2"
echo ""
echo "Press Ctrl+C to stop all processes..."
echo ""

./build/Server &
PIDS+=($!)
sleep 1

./build/Client --controller 0 --window-pos 0 60 &
PIDS+=($!)
sleep 1

SDL_AUDIODRIVER=dummy ./build/Client --controller 1 --window-pos "$P2_X" "$P2_Y" &
PIDS+=($!)

echo ""
echo "Couch co-op running!"
echo "PIDs: ${PIDS[*]}"
echo ""

wait
