---
name: test
description: Run all tests for the Gambit game engine. Use when the user wants to run tests, verify functionality, or check that code changes haven't broken anything.
---

# Run Tests

Runs all tests for the Gambit game engine.

## Current Status

**The test framework is not yet implemented.**

## Instructions

When tests are available, this skill will execute them. For now:

1. Check if a tests directory exists:
   ```bash
   if [ -d "tests" ] || [ -d "test" ]; then
       # Run tests here when implemented
       echo "Tests found but framework not configured"
   else
       echo "No tests directory found."
   fi
   ```

## Future Implementation

When adding tests, consider these frameworks:

### Google Test (Recommended for C++)
```cmake
# In CMakeLists.txt
enable_testing()
find_package(GTest REQUIRED)

add_executable(TestRunner
    tests/test_network.cpp
    tests/test_window.cpp
)
target_link_libraries(TestRunner GTest::GTest GTest::Main)
add_test(NAME AllTests COMMAND TestRunner)
```

Then run with:
```bash
cd build
ctest
```

### Custom Integration Tests
For multiplayer scenarios:
```bash
# tests/integration_test.sh
./build/Server &
SERVER_PID=$!
sleep 1

./build/Client --test-mode &
CLIENT_PID=$!

# Run test scenarios
# Check results
# Clean up

kill $SERVER_PID $CLIENT_PID
```

## Tiger Style Testing

Following the Tiger Style approach:
- **Assert on negative space**: Crash on invalid states
- **Fuzz testing**: Test networking, asset loading, physics
- **Integration tests**: Test full multiplayer sessions

## Next Steps

To add tests:
1. Create a `tests/` directory
2. Choose a test framework (Google Test recommended)
3. Update `CMakeLists.txt` to build tests
4. Update this skill to run them

## Notes

- Tests should verify both single-player and multiplayer scenarios
- Focus on networking, asset pipeline, and game state synchronization
- Fuzz testing is critical for robustness (Tiger Style principle)
