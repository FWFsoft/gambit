# Headless Test

Run the Gambit client in headless mode for automated testing and verification without graphics.

## Instructions

Headless testing allows you to verify game logic changes by running the client without graphics and observing events.

### Basic Usage

1. Run headless client for a specific number of frames:
   ```bash
   ./build/HeadlessClient --frames 60
   ```

2. Run with event logging enabled:
   ```bash
   ./build/HeadlessClient --frames 120 --log-events
   ```

3. Run with verbose event logging:
   ```bash
   ./build/HeadlessClient --frames 60 --verbose
   ```

### Running Integration Tests

HeadlessMovement requires an externally managed server on localhost:1234.
It is **not registered by default** (`GAMBIT_ENABLE_SERVER_INTEGRATION_TESTS=OFF`).
Opt in at configure time, using a separate build to keep ordinary `make test`
server-free. From repository root (with the usual build dependencies available):

```bash
cmake -S . -B build-integration \
  -DGAMBIT_ENABLE_SERVER_INTEGRATION_TESTS=ON
cmake --build build-integration --target Server test_headless_movement -j8
# Terminal 1, from repository root:
./build-integration/Server
# Terminal 2, from repository root:
(cd build-integration && ctest -L integration --output-on-failure)
```

The example uses system dependencies. If needed, add
`-DCMAKE_TOOLCHAIN_FILE=/absolute/path/to/toolchain.cmake` when configuring.
Labels only select registered tests; `ctest -L integration` does not enable
anything. The enabled test retains `integration;manual` labels and runs from
repository root for assets. CTest does not manage the server. Unfiltered CTest
in this opted-in build also runs it. Disable again with
`cmake -S . -B build-integration -DGAMBIT_ENABLE_SERVER_INTEGRATION_TESTS=OFF`.

Validate registration without game dependencies/server using
`python3 tests/test_server_integration_config.py -v`. This includes a stub
execution fixture, not a real server integration test; absent CMake/CTest is
reported as a skip.

Or run the test executable directly from project root:

```bash
# Start server in background
./build-integration/Server > /dev/null 2>&1 &
SERVER_PID=$!

# Run test
./build-integration/test_headless_movement

# Cleanup
kill $SERVER_PID
```

### Command-Line Options

- `--frames N` - Run for exactly N frames then exit (default: unlimited)
- `--log-events` - Enable event logging to stdout
- `--verbose` - Enable verbose event logging (includes UpdateEvent, RenderEvent)
- `--help` - Show help message

## Expected Output

On success, you should see:

```
[info] === Headless Client Starting ===
[info] Max frames: 60
[info] Event logging: disabled
[info] Connected to 127.0.0.1:1234
[info] MockWindow created: Headless Gambit Client (1280x720)
...
[info] === Reached max frames (60) - stopping ===
[info] === Headless Client Shutting Down ===
[info] Total frames executed: 60
```

For integration tests:
```
[info] === Running Headless Movement Tests ===
...
[info] HeadlessMovement_RightMovement: PASSED
[info]   Start position: (200.000000, 700.000000)
[info]   End position: (250.000000, 695.000000)
[info]   Delta X: 50.000000
[info]   Right inputs captured: 30
...
[info] === All Headless Movement Tests Passed ===
```

## Error Handling

**Server not running:**
```
[error] Connection to 127.0.0.1:1234 failed.
```
Solution: Start the server first with `./build/Server`

**Assets not found:**
```
ERROR: Failed opening assets/test_map.tmx
ERROR: Reason: File was not found
```
Solution: Run from project root directory, not from build/

**Test assertion failure:**
```
Assertion failed: (deltaX > 0.0f && "Player should have moved right")
```
Solution: Check game logic - the test detected unexpected behavior. Review the assertion message for details.

## Writing Custom Headless Tests

Create a new test file in `tests/` directory:

```cpp
#include "test_utils.h"
#include "InputScript.h"
#include "EventBus.h"
// ... other includes

TEST(MyCustomTest) {
  resetEventBus();

  // Setup client, server connection, etc.
  NetworkClient client;
  client.initialize();
  client.connect(Config::Network::SERVER_ADDRESS, Config::Network::PORT);

  // Create input script
  InputScript inputScript;
  inputScript.addMove(10, 30, false, true, false, false); // Move right

  // Capture events
  EventCapture<LocalInputEvent> inputs;
  EventCapture<NetworkPacketReceivedEvent> packets;

  // Run headless client
  runHeadlessClientForFrames(client, prediction, window, 60);

  // Assert on results
  inputs.assertAtLeast(20); // Captured at least 20 input events
  packets.assertCount(60);  // Got state updates every frame

  client.disconnect();
}
```

### EventCapture API

Available assertion methods:

- `assertCount(size_t expected)` - Assert exact count
- `assertAtLeast(size_t minimum)` - Assert minimum count
- `assertAny(predicate)` - Assert at least one event matches predicate
- `assertAll(predicate)` - Assert all events match predicate
- `countMatching(predicate)` - Count events matching predicate
- `first()`, `last()`, `at(index)` - Access specific events

### InputScript API

Methods for scripting input:

- `addKeyPress(frame, key, duration)` - Press and release a key
- `addKeyDown(frame, key)` - Press key down
- `addKeyUp(frame, key)` - Release key
- `addMove(startFrame, duration, left, right, up, down)` - Movement (WASD)

Example movement patterns:

```cpp
InputScript script;

// Move right for 30 frames starting at frame 10
script.addMove(10, 30, false, true, false, false);

// Move diagonally (up-right) for 20 frames
script.addMove(10, 20, false, true, true, false);

// Attack at frame 50
script.addKeyPress(50, SDLK_SPACE, 1);

// Use item at frame 60
script.addKeyPress(60, SDLK_e, 1);
```

## Notes

- **Server requirement**: Headless client requires a running server on localhost:1234
- **Working directory**: Tests must run from project root to access assets/
- **Event-driven**: Uses the same EventBus architecture as the main client
- **No graphics**: Completely headless - no SDL window, OpenGL, or ImGui
- **Performance**: Much faster than regular client (no rendering overhead)
- **CI/CD ready**: Can run in automated pipelines with proper server setup

## Use Cases

### For Claude (AI) Verification

When making changes to game logic, Claude can:

1. Modify the code (e.g., damage calculation, movement speed)
2. Create or run a headless test that exercises the changed code
3. Verify the change works by checking event outputs
4. Get immediate feedback without manual testing

Example workflow:
```
Claude modifies: src/ServerGameState.cpp (damage formula)
Claude opts in as above, starts Server, then runs from repository root:
(cd build-integration && ctest -R '^HeadlessMovement$' --output-on-failure)
Output: PASSED - damage was 25, expected 25 ✓
Result: Change verified automatically
```

### For Developers

- Test multiplayer scenarios without launching 4 clients
- Verify networking logic (prediction, reconciliation)
- Debug input handling and game state changes
- Regression testing for gameplay features
- CI/CD integration for automated testing

### Current Limitations

- Cannot test rendering or visual features
- Cannot test UI interactions (uses HeadlessUISystem)
- Requires manual server management (not automated)
- Single-client tests only (multi-client harness not yet implemented)

## Future Enhancements

Potential additions mentioned in the planning phase:

- Multi-client test harness (spawn multiple headless clients)
- Event recording/replay system for deterministic testing
- JSON-based input scripting (load scripts from files)
- Performance benchmarking mode
- Fuzzing support for edge case discovery
- CI integration with automatic server lifecycle management
