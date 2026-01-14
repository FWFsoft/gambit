#!/bin/bash
# Visual Test Runner
# Starts server and client for autonomous visual testing

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

echo "🎮 Visual Test Mode"
echo "===================="
echo ""

# Check if test input file exists
if [ ! -f "test_input.txt" ]; then
    echo "⚠️  test_input.txt not found. Creating navigation test..."
    cat > test_input.txt << 'EOF'
# Autonomous navigation test to Scrapyard_01 at (1184, -4112)

# Get past title screen
WAIT 60
KEY_DOWN Enter
KEY_UP Enter
WAIT 60

# Character select - pick first character with Enter
SCREENSHOT character_select_start
KEY_DOWN Enter
KEY_UP Enter

# Wait for game world to load
WAIT 150
SCREENSHOT spawn_location

# Enable debug rendering with F1
KEY_DOWN F1
KEY_UP F1
WAIT 10
SCREENSHOT debug_enabled

# Explore and navigate
KEY_DOWN D
WAIT 60
KEY_UP D
SCREENSHOT explored_east

KEY_DOWN W
WAIT 120
KEY_UP W
SCREENSHOT explored_north

KEY_DOWN D
WAIT 180
KEY_UP D
SCREENSHOT moved_east

SCREENSHOT final_position
EOF
    echo "✅ Created test_input.txt"
fi

# Create output directory
mkdir -p test_output

echo ""
echo "📸 Screenshots: test_output/"
echo "📝 Commands: test_input.txt"
echo ""

# Start server in background
echo "🚀 Starting server..."
./build/Server > test_output/server.log 2>&1 &
SERVER_PID=$!
echo "   Server PID: $SERVER_PID"
sleep 2

# Start client in test mode
echo "🚀 Starting client in test mode..."
./build/Client --test-mode > test_output/client.log 2>&1 &
CLIENT_PID=$!
echo "   Client PID: $CLIENT_PID"
echo ""

echo "✅ Test environment running!"
echo ""
echo "📋 Logs: test_output/server.log, test_output/client.log"
echo "🔍 Monitoring test progress (press Ctrl+C to stop)..."
echo ""

# Monitor screenshots
LAST_MTIME=0
TIMEOUT=180  # 3 minutes max
START_TIME=$(date +%s)

while true; do
    sleep 2

    # Check timeout
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))
    if [ $ELAPSED -gt $TIMEOUT ]; then
        echo "⏱️  Test timeout reached ($TIMEOUT seconds)"
        break
    fi

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
        echo "✅ Client finished"
        break
    fi
done

echo ""
echo "🎬 Test complete!"
echo "📊 Check test_output/ for screenshots and logs"
