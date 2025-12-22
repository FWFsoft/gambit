---
name: dev
description: Full development environment - builds the project and runs 1 server plus 4 clients. Use when the user wants to test multiplayer, simulate 4-player co-op, or run the complete dev environment.
---

# Development Environment

Builds the Gambit game engine and launches a full development environment with 1 server and 4 clients to simulate multiplayer gameplay.

## Instructions

1. Build the project:
   ```bash
   mkdir -p build
   cd build
   cmake ..
   make
   cd ..
   ```

2. Start the server in the background:
   ```bash
   ./build/Server &
   SERVER_PID=$!
   echo "Server started (PID: $SERVER_PID)"
   ```

3. Start 4 clients with delays:
   ```bash
   CLIENT_PIDS=()
   for i in {1..4}; do
       ./build/Client &
       CLIENT_PIDS+=($!)
       echo "Client $i started (PID: ${CLIENT_PIDS[$i-1]})"
       sleep 1
   done
   ```

4. Display running processes:
   ```bash
   echo ""
   echo "Development environment running:"
   echo "  1 Server (PID: $SERVER_PID)"
   echo "  4 Clients (PIDs: ${CLIENT_PIDS[@]})"
   echo ""
   ```

5. Wait for user to press Enter to terminate:
   ```bash
   read -p "Press Enter to terminate all processes..."
   ```

6. Gracefully shut down all processes:
   ```bash
   echo "Shutting down..."
   kill -SIGINT $SERVER_PID 2>/dev/null || true
   for pid in "${CLIENT_PIDS[@]}"; do
       kill -SIGINT $pid 2>/dev/null || true
   done

   wait $SERVER_PID 2>/dev/null || true
   for pid in "${CLIENT_PIDS[@]}"; do
       wait $pid 2>/dev/null || true
   done

   echo "All processes terminated."
   ```

## What This Simulates

This environment simulates the target 4-player co-op PvE gameplay:
- 1 Server managing game state
- 4 Clients representing players

## Expected Behavior

You should see:
- Server starts and listens on port 1234
- Each client connects with a 1-second delay
- 4 SDL2 windows open (one per client)
- All clients successfully connect to the server

## Stopping the Environment

- Press Enter in the terminal to terminate all processes gracefully
- All server and client processes will shut down cleanly

## Use Cases

- Testing multiplayer interactions
- Verifying network synchronization
- Debugging client-server communication
- Simulating full game sessions

## Notes

- This is the primary development workflow for multiplayer testing
- The 1-second delays between client starts prevent connection race conditions
- All processes run locally for easy debugging
