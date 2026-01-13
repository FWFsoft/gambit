#!/bin/bash
# Interactive Visual Test Runner
# Claude can observe screenshots and write commands dynamically

set -e

# Cleanup function
cleanup() {
    echo ""
    echo "🛑 Stopping test processes..."
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
    fi
    if [ ! -z "$CLIENT_PID" ]; then
        kill $CLIENT_PID 2>/dev/null || true
    fi
    echo "✅ Cleanup complete"
}

trap cleanup EXIT INT TERM

echo "🎮 Interactive Visual Test Mode"
echo "================================"
echo ""

# Create output directory
mkdir -p test_output

# Initialize test_input.txt with just a wait command
cat > test_input.txt << 'EOF'
# Visual test - Claude will add commands here dynamically
# Initial wait for game to load
WAIT 120
EOF

echo "📝 Initialized test_input.txt"
echo ""

# Start server in background
echo "🚀 Starting server..."
./build/Server > test_output/server.log 2>&1 &
SERVER_PID=$!
echo "   Server PID: $SERVER_PID"
sleep 2

# Start client in test mode in background
echo "🚀 Starting client in test mode..."
./build/Client --test-mode > test_output/client.log 2>&1 &
CLIENT_PID=$!
echo "   Client PID: $CLIENT_PID"
echo ""

echo "✅ Test environment running!"
echo ""
echo "📸 Screenshots: test_output/frame_latest.png (updates every 0.5s)"
echo "📝 Commands: test_input.txt (append commands, client reads them live)"
echo "📋 Logs: test_output/server.log, test_output/client.log"
echo ""
echo "🤖 Claude can now:"
echo "   1. Read test_output/frame_latest.png to see game state"
echo "   2. Append commands to test_input.txt (client picks them up)"
echo "   3. Verify game progression by observing screenshots"
echo ""
echo "Press Ctrl+C to stop the test"
echo ""

# Keep script running and monitor
echo "Monitoring screenshots (press Ctrl+C to stop)..."
LAST_MTIME=0
while true; do
    sleep 2

    # Check if screenshot file exists and has been updated
    if [ -f "test_output/frame_latest.png" ]; then
        CURRENT_MTIME=$(stat -f %m "test_output/frame_latest.png" 2>/dev/null || echo 0)
        if [ "$CURRENT_MTIME" != "$LAST_MTIME" ]; then
            echo "[$(date +%H:%M:%S)] 📸 Screenshot updated"
            LAST_MTIME=$CURRENT_MTIME
        fi
    fi

    # Check if processes are still running
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "❌ Server process died"
        break
    fi
    if ! kill -0 $CLIENT_PID 2>/dev/null; then
        echo "❌ Client process died"
        break
    fi
done
