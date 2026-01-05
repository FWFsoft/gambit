#!/bin/bash

# Headless Test Runner
# Usage: ./run.sh [--frames N] [--log-events] [--verbose]

set -e

# Change to project root (assumes script is in .claude/skills/headless-test/)
cd "$(dirname "$0")/../../.."

# Check if HeadlessClient exists
if [ ! -f "./build/HeadlessClient" ]; then
    echo "Error: HeadlessClient not found. Run 'make build' first."
    exit 1
fi

# Check if server is running
if ! nc -z 127.0.0.1 1234 2>/dev/null; then
    echo "Warning: Server doesn't appear to be running on localhost:1234"
    echo "Start server first: ./build/Server"
    exit 1
fi

# Run headless client with provided arguments
./build/HeadlessClient "$@"
