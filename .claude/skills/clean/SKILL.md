---
name: clean
description: Remove build artifacts for a fresh rebuild. Use when the user wants to clean the project, start fresh, or troubleshoot build issues.
---

# Clean Build Artifacts

Removes the build directory and all compiled artifacts to enable a fresh rebuild.

## Instructions

1. Check if the build directory exists:
   ```bash
   if [ -d "build" ]; then
       rm -rf build
       echo "Build artifacts cleaned!"
   else
       echo "No build directory found - nothing to clean."
   fi
   ```

## When to Use

Use this skill when:
- You encounter strange build errors
- Dependencies have changed
- You want to ensure a completely fresh build
- Before running the `build` skill if you suspect stale artifacts

## Notes

- This is a destructive operation - all build artifacts will be removed
- You will need to run the `build` skill after cleaning
- The source code is never affected, only the build/ directory
