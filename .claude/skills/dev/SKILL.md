---
name: dev
description: Full development environment - builds the project and runs 1 server plus 4 clients. Use when the user wants to test multiplayer, simulate 4-player co-op, or run the complete dev environment.
---

# Development Environment

Builds the project and runs a full development environment with 1 server and 4 clients.

## Instructions

1. Run the dev command:
   ```bash
   make dev
   ```

## What This Does

1. Builds the project (if needed)
2. Starts 1 server instance on 0.0.0.0:1234
3. Waits 1 second for server to initialize
4. Starts 4 client instances with 1-second delays between each
5. Waits for you to press Enter to stop all processes

## Expected Behavior

You should see:
```
Starting Gambit development environment...
  - 1 Server (0.0.0.0:1234)
  - 4 Clients (connecting to 127.0.0.1:1234)

Starting server...
Starting 4 clients...

Development environment running!
Press Enter to stop all processes...
```

Then 4 game windows will open, each with a different colored player (Red, Green, Blue, Yellow).

## Stopping the Environment

- Press `Enter` in the terminal to stop all processes gracefully
- All server and client processes will be terminated automatically

## Notes

- This simulates a 4-player multiplayer game
- Each client gets a unique color assigned by the server
- Perfect for testing networked gameplay features
- The server and clients run in the background
- All processes are cleaned up when you press Enter
