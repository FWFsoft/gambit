---
name: run-client
description: Start a single Gambit game client that connects to 127.0.0.1:1234. Use when the user wants to run a client, test client functionality, or manually connect to a server.
---

# Run Gambit Client

Starts a single Gambit game client that connects to the server at 127.0.0.1:1234.

## Instructions

1. Verify the Client executable exists:
   ```bash
   if [ ! -f "build/Client" ]; then
       echo "Error: Client executable not found. Run the 'build' skill first."
       exit 1
   fi
   ```

2. Start the client:
   ```bash
   echo "Starting Gambit Client (connecting to 127.0.0.1:1234)..."
   ./build/Client
   ```

## Client Details

- **Server Address**: 127.0.0.1:1234
- **Protocol**: ENet (UDP-based)
- **Window**: Opens an SDL2 window (800x600)
- **Frame Rate**: ~60 FPS (16ms delay)

## Expected Behavior

When running, you should see:
```
[HH:MM:SS] [info] Logger initialized
[HH:MM:SS] [info] Connected to 127.0.0.1:1234
```

An 800x600 window titled "Gambit Client" will open.

## Stopping the Client

- Close the window to disconnect and exit
- Or press `Ctrl+C` in the terminal

## Prerequisites

- A server must be running on 127.0.0.1:1234
- Use the `run-server` skill to start a server first

## Notes

- The client runs in the foreground with a graphical window
- For testing multiplayer, use the `dev` skill instead (starts 1 server + 4 clients)
- Each client instance requires a separate terminal
