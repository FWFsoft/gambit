---
name: pre-commit
description: Run pre-commit hooks (clang-format, clang-tidy) on all files. Use when the user wants to format code, run linters, or prepare code for commit.
---

# Run Pre-commit Hooks

Runs all configured pre-commit hooks (clang-format and clang-tidy) on the codebase.

## Instructions

1. Run the pre-commit command:
   ```bash
   make pre-commit
   ```

## Expected Output

On success, you should see:
```
clang-format............................Passed
clang-tidy..............................Passed
```

On failure, you will see:
- Which files failed the checks
- What changes were made (for auto-fixable issues)
- Error messages for issues that need manual fixing

## Configured Hooks

- **clang-format**: Formats C++ code using Google style
- **clang-tidy**: Lints C++ code and auto-fixes errors when possible

## Notes

- clang-format will automatically reformat files that don't match the style
- clang-tidy will attempt to auto-fix errors with `--fix-errors`
- Some clang-tidy issues may require manual intervention
- Run this before committing code to ensure consistency
