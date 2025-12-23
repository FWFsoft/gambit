---
name: test
description: Run all tests for the Gambit game engine. Use when the user wants to run tests, verify functionality, or check that code changes haven't broken anything.
---

# Run Tests

Builds the project (if needed) and runs all tests.

## Instructions

1. Run the test command:
   ```bash
   make test-coverage
   ```

## Expected Output

On success, you should see:
```
All tests passed!
```
And can view coverage in `build/coverage/index.html`
Or by using `make test-coverage-open`

On failure, you will see details about which tests failed.

## Notes

- Tests are automatically built if they don't exist or if code has changed
- Coverage will be printed, which you can use to tell if you need to increase
coverage for certain files
