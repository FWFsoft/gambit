---
name: search
description: Advanced code search tool that finds definitions, usages, tests, and references across the entire codebase. Use when you need to understand how a symbol is used throughout the project.
---

# Code Search

Intelligent code search that finds all references to symbols (functions, classes, variables, etc.) across code, tests, examples, benchmarks, and fuzz tests.

## Instructions

1. Run the search command with a symbol name:
   ```bash
   uv run python .claude/skills/search/search.py <symbol>
   ```

## Features

- Finds function/class definitions
- Locates all usages and references
- Searches across:
  - Source files (src/, include/)
  - Test files (tests/)
  - Examples (examples/)
  - Benchmarks (benchmarks/)
  - Fuzz tests (fuzz/)
- Shows context around each match
- Groups results by category

## Examples

Search for a function:
```bash
uv run python .claude/skills/search/search.py NetworkClient
```

Search for a method:
```bash
uv run python .claude/skills/search/search.py connect
```

Search for a variable:
```bash
uv run python .claude/skills/search/search.py server_address
```

## Notes

- More powerful than simple grep - understands C++ code structure
- Shows both definitions and all usages
- Helps trace how code flows through the system
