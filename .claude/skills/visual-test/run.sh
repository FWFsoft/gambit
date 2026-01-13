#!/bin/bash
# Visual Test Runner
# Starts the client in test mode for visual testing

set -e

echo "🎮 Visual Test Mode"
echo "===================="
echo ""

# Check if test input file exists
if [ ! -f "test_input.txt" ]; then
    echo "⚠️  test_input.txt not found. Creating example file..."
    cat > test_input.txt << 'EOF'
# Example visual test script
# Wait for game to initialize
WAIT 120

# Take initial screenshot
SCREENSHOT initial_state

# Move character
KEY_DOWN W
WAIT 30
KEY_UP W
SCREENSHOT moved_forward

# Wait and finish
WAIT 60
SCREENSHOT test_complete
EOF
    echo "✅ Created test_input.txt"
fi

# Create output directory
mkdir -p test_output

echo ""
echo "📸 Screenshots will be saved to: test_output/"
echo "📝 Input commands read from: test_input.txt"
echo ""
echo "⚠️  Make sure the server is running on localhost:1234"
echo ""
echo "Starting client in test mode..."
echo ""

# Run client in test mode
./build/Client --test-mode
