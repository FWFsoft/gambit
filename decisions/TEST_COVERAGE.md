# Test Coverage Audit - Gambit Engine

**Date:** 2024-12-22
**Status:** In Progress

## Current Test Coverage

### Existing Tests (4 total)

**Location:** `src/test_main.cpp`

1. ✅ `ClientInputSerialization` - Tests ClientInputPacket round-trip (6 bytes)
2. ✅ `StateUpdateSerialization` - Tests StateUpdatePacket with 2 players (60 bytes)
3. ✅ `PlayerJoinedSerialization` - Tests PlayerJoinedPacket (8 bytes)
4. ✅ `PlayerLeftSerialization` - Tests PlayerLeftPacket (5 bytes)

**Coverage:** Phase 3 (Network Protocol) only - basic happy path serialization

### Coverage Gaps by Phase

| Phase | Component | Current Coverage | Missing Tests |
|-------|-----------|------------------|---------------|
| **1** | EventBus | ❌ 0% | Pub/sub, type safety, multiple subscribers, unsubscribe |
| **1** | GameLoop | ❌ 0% | Fixed timestep, frame counting, stop/start |
| **2** | InputSystem | ❌ 0% | WASD state, sequence numbering, event publishing |
| **3** | NetworkProtocol | ⚠️ 30% | Edge cases: 0 players, 100+ players, malformed packets |
| **4** | Player (applyInput) | ❌ 0% | Movement, diagonal normalization, bounds clamping |
| **5** | ServerGameState | ❌ 0% | Player spawning, input processing, state broadcast |
| **6** | ClientPrediction | ❌ 0% | Prediction, reconciliation, input history |
| **7** | RemotePlayerInterpolation | ❌ 0% | Snapshot buffering, lerp, edge cases |

**Overall Coverage:** ~5% (4 tests covering only serialization basics)

## Test Strategy

### Unit Tests (Target: 80% code coverage)

Tests for individual components in isolation.

#### Phase 1: Event System Foundation

**EventBus Tests:**
- ✅ Subscribe and publish single event
- ✅ Multiple subscribers to same event type
- ✅ Multiple event types
- ✅ Type safety (compile-time checks)
- ✅ Publish with no subscribers (no-op)
- ✅ Order of execution (subscribers called in order)
- ✅ Clear all handlers

**GameLoop Tests:**
- ✅ Fixed 60 FPS timestep (16.67ms)
- ✅ Frame number increments
- ✅ UpdateEvent published every frame
- ✅ RenderEvent published with interpolation
- ✅ Stop/start functionality
- ⚠️ Performance: Assert if frame time > 33ms (Tiger Style)

#### Phase 2: Input Handling

**InputSystem Tests:**
- ✅ WASD key down/up state tracking
- ✅ Arrow keys mapped correctly
- ✅ Input sequence increments every frame
- ✅ LocalInputEvent published on UpdateEvent
- ✅ Multiple keys pressed simultaneously
- ✅ Rapid key press/release

#### Phase 3: Network Protocol (Expand existing)

**NetworkProtocol Edge Cases:**
- ✅ StateUpdate with 0 players
- ✅ StateUpdate with 1 player
- ✅ StateUpdate with 4 players (current max)
- ✅ StateUpdate with 100 players (stress test)
- ✅ StateUpdate with 1000 players (stress test)
- ✅ Malformed packet detection (too small, wrong type)
- ✅ Endianness (little-endian serialization)
- ✅ Float precision (round-trip accuracy)
- ✅ Packet size validation (Tiger Style asserts)

#### Phase 4: Player Entity & Movement

**Player Tests:**
- ✅ Movement in 8 directions (N, NE, E, SE, S, SW, W, NW)
- ✅ Diagonal normalization (velocity = 141 px/s, not 200)
- ✅ World bounds clamping (0-800, 0-600)
- ✅ Speed constant (200 px/s)
- ✅ Stationary player (no input)
- ✅ Delta time scaling (movement scales with deltaTime)

#### Phase 5: Server Authority

**ServerGameState Tests:**
- ✅ Player spawning with random position
- ✅ Color assignment (cycling through 4 colors)
- ✅ Input sequence validation (reject old inputs)
- ✅ State broadcast to all clients
- ✅ Player join/leave events
- ✅ Input processing and movement application
- ✅ Server tick increment

#### Phase 6: Client-Side Prediction

**ClientPrediction Tests:**
- ✅ Immediate input application (0ms latency)
- ✅ Input history buffering (last 60 inputs)
- ✅ Reconciliation with server state
- ✅ Input replay after reconciliation
- ✅ Prediction error detection (> 50px warning)
- ✅ Input history cleanup (remove old inputs)
- ✅ Sequence number tracking

#### Phase 7: Remote Player Interpolation

**RemotePlayerInterpolation Tests:**
- ✅ Snapshot buffering (3 snapshots max)
- ✅ Linear interpolation between snapshots
- ✅ Interpolation with 0.0, 0.5, 1.0 factor
- ✅ Player join/leave handling
- ✅ Skip local player (not in buffer)
- ✅ Query all remote player IDs
- ✅ Interpolation with 1 snapshot (no lerp)
- ✅ Interpolation with 2 snapshots (lerp)

### Integration Tests

End-to-end tests with real server and clients.

#### Multi-Client Tests
- ✅ 1 server + 1 client connection
- ✅ 1 server + 4 clients (full game)
- ✅ Client disconnects mid-game
- ✅ Late-joining client receives all players
- ✅ All clients disconnect (empty server)

#### Stress Tests
- ✅ 100 concurrent clients
- ✅ 1000 concurrent clients
- ✅ Packet loss simulation (10%, 20%, 50%)
- ✅ High latency simulation (100ms, 500ms)
- ✅ Sustained 60 FPS for 5 minutes (4 clients)

### Performance Tests (Tiger Style)

#### Assertions
- ✅ Client FPS never drops below 58 (< 30 = crash)
- ✅ Server TPS never drops below 58 (< 30 = crash)
- ✅ Prediction error < 50px (warn if exceeded)
- ✅ Packet size within expected bounds
- ✅ Input latency < 5ms (local application)

#### Benchmarks
- ✅ Serialization performance (1M packets/sec target)
- ✅ Deserialization performance
- ✅ EventBus overhead (< 1μs per publish)
- ✅ Interpolation computation (< 100ns per player)

### Code Coverage Target

**Minimum:** 80% line coverage
**Ideal:** 90% line coverage
**Critical paths:** 100% coverage (input processing, state sync, prediction)

**Excluded from coverage:**
- Logger calls
- Main entry points (client_main.cpp, server_main.cpp)
- SDL initialization code

## Test Infrastructure

### Tools

1. **Test Framework:** Custom macro-based (current)
   - Alternative: Google Test (recommended for future)
   - Alternative: Catch2

2. **Code Coverage:** gcov + lcov
   - Generate HTML reports
   - Line and branch coverage
   - Integration with CI/CD

3. **Valgrind/ASan:** Memory leak detection
   - Ensure no leaks in long-running tests
   - Check all allocations are freed

4. **Stress Testing:** Custom scripts
   - Spawn N clients via shell scripts
   - Monitor CPU, memory, bandwidth
   - Automated pass/fail criteria

### CMake Configuration

```cmake
# Add coverage flags for Debug builds
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()
```

### Makefile Targets

```makefile
test:           # Run all tests
test-coverage:  # Generate coverage report
test-stress:    # Run stress tests (100, 1000 players)
test-valgrind:  # Run tests under Valgrind
```

## Implementation Plan

### Phase 1: Infrastructure (Priority: High)
1. ✅ Set up gcov/lcov for code coverage
2. ✅ Add CMake coverage flags
3. ✅ Create Makefile target for coverage reports
4. ✅ Document how to run tests with coverage

### Phase 2: Unit Tests (Priority: High)
1. ✅ EventBus tests (7 tests)
2. ✅ InputSystem tests (6 tests)
3. ✅ Player movement tests (6 tests)
4. ✅ NetworkProtocol edge cases (9 tests, expand existing 4)
5. ✅ ClientPrediction tests (7 tests)
6. ✅ RemotePlayerInterpolation tests (8 tests)
7. ✅ ServerGameState tests (7 tests)

**Total new unit tests:** ~50

### Phase 3: Integration Tests (Priority: Medium)
1. ✅ Multi-client connection tests (5 tests)
2. ✅ Stress tests for 100 players (1 test)
3. ✅ Stress tests for 1000 players (1 test)

**Total integration tests:** ~7

### Phase 4: Performance Tests (Priority: Low)
1. ✅ Benchmark serialization
2. ✅ Benchmark EventBus
3. ✅ Benchmark interpolation
4. ✅ Sustained FPS test

**Total performance tests:** ~4

**Grand Total:** ~65 tests (up from 4)

## Tiger Style Testing Principles

### Assert on Negative Space

All tests should have assertions that crash on invalid state:
```cpp
TEST(PlayerMovement_BoundsCheck) {
    Player player;
    player.x = -100.0f;  // Invalid
    applyInput(player, false, false, false, false, 16.67f);
    // Should assert/crash (Tiger Style)
}
```

### Fuzz Testing

Generate random inputs to find edge cases:
```cpp
TEST(NetworkProtocol_FuzzTest) {
    for (int i = 0; i < 10000; i++) {
        uint8_t randomData[100];
        fillRandom(randomData);
        // Should not crash, should detect malformed packets
        tryDeserialize(randomData);
    }
}
```

### Crash Early

Tests should fail loudly and immediately:
```cpp
assert(serialized.size() == expectedSize);  // ✅ Immediate crash
if (serialized.size() != expectedSize) return false;  // ❌ Silent failure
```

## Expected Outcomes

### Before
- 4 tests
- ~5% code coverage
- No edge case testing
- No stress testing
- No performance validation

### After
- 65+ tests
- 80%+ code coverage
- Edge cases covered (0, 1, 4, 100, 1000 players)
- Stress tests validate scalability
- Performance benchmarks establish baselines
- Tiger Style assertions throughout

## Test Execution

### Running Tests

```bash
# Run all unit tests
make test

# Run with coverage
make test-coverage
# Open coverage report: build/coverage/index.html

# Run stress tests
make test-stress

# Run with memory leak detection
make test-valgrind
```

### CI/CD Integration

Future: Run tests on every commit
- Unit tests: < 5 seconds
- Integration tests: < 30 seconds
- Stress tests: < 2 minutes
- Coverage report: Fail if < 80%

## Next Steps

1. **Immediate:** Set up code coverage tooling
2. **Week 1:** Write all unit tests (Phases 1-7)
3. **Week 2:** Write integration and stress tests
4. **Week 3:** Add performance benchmarks
5. **Week 4:** Review coverage, fill gaps, document

---

**Last Updated:** 2024-12-22
**Coverage Status:** 5% → Target: 80%
**Test Count:** 4 → Target: 65+
