---
name: test
description: Run all tests for the Gambit game engine. Use when the user wants to run tests, verify functionality, or check that code changes haven't broken anything.
---

# Run Tests

Builds the project (if needed) and runs all tests using CTest.

## Instructions

1. Run tests:
   ```bash
   make test
   ```

2. Run tests with coverage:
   ```bash
   make test-coverage
   ```

## Test Structure

Tests are organized into modular suites:
- `PlayerMovement` - Player movement and bounds tests
- `NetworkProtocol` - Network serialization/deserialization tests
- `EventBus` - Event bus pub/sub tests
- `InputSystem` - Input handling tests
- `RemotePlayerInterpolation` - Remote player interpolation tests
- `ClientPrediction` - Client-side prediction tests

## Expected Output

On success, you should see:
```
100% tests passed, 0 tests failed out of 6
```

On failure, CTest will show which test suites failed with error details.

## Notes

- Tests use CTest for better isolation and parallel execution
- Each test suite is a separate executable in `build/test_*`
- Run specific tests with: `cd build && ctest -R <TestName>`
- Run tests in parallel with: `cd build && ctest -j8`
- Coverage reports available at `build/coverage/index.html`
