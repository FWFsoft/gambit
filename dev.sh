#!/bin/bash
# Gambit Development Environment
# Starts 1 server + 4 clients, handles cleanup on Ctrl+C

set -e

# Track all child processes
PIDS=()

# Cleanup function
cleanup() {
    echo ""
    echo "Shutting down all processes..."
    for pid in "${PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    echo "All processes stopped."
    exit 0
}

# Set up trap for Ctrl+C (SIGINT) and other termination signals
trap cleanup SIGINT SIGTERM EXIT

# Check if executables exist
if [ ! -f "build/Server" ] || [ ! -f "build/Client" ]; then
    echo "Error: Executables not found. Run 'make build' first."
    exit 1
fi

echo "Starting Gambit development environment..."
echo "  - 1 Server (0.0.0.0:1234)"
echo "  - 4 Clients (connecting to 127.0.0.1:1234)"
echo ""
echo "Press Ctrl+C to stop all processes..."
echo ""

# Start server
echo "Starting server..."
./build/Server &
PIDS+=($!)
sleep 1

# Start 4 clients
echo "Starting 4 clients..."
for i in {1..4}; do
    ./build/Client &
    PIDS+=($!)
    sleep 1
done

echo ""
echo "Development environment running!"
echo "PIDs: ${PIDS[*]}"
echo ""

# Wait for all processes (or until Ctrl+C)
wait
