#!/usr/bin/env python3
"""
Advanced code search tool for the Gambit game engine.

Finds definitions and references to symbols across the entire codebase,
including code, tests, examples, benchmarks, and fuzz tests.
"""

import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict, Set


@dataclass
class Match:
    """Represents a single search match."""
    file: Path
    line_num: int
    line: str
    category: str  # 'definition', 'usage', 'test', 'example', 'benchmark', 'fuzz'

    def __str__(self):
        return f"{self.file}:{self.line_num}: {self.line.strip()}"


class CodeSearch:
    """Intelligent code search across the codebase."""

    # Directories to search
    SEARCH_PATHS = {
        'code': ['src', 'include'],
        'test': ['tests'],
        'example': ['examples'],
        'benchmark': ['benchmarks'],
        'fuzz': ['fuzz'],
    }

    # File extensions to search
    CODE_EXTENSIONS = {'.cpp', '.h', '.hpp', '.cc', '.c'}

    def __init__(self, root_dir: Path):
        self.root_dir = root_dir

    def categorize_file(self, file: Path) -> str:
        """Determine the category of a file based on its path."""
        rel_path = file.relative_to(self.root_dir)
        parts = rel_path.parts

        if len(parts) == 0:
            return 'code'

        first_dir = parts[0]
        if first_dir in ('tests', 'test'):
            return 'test'
        elif first_dir == 'examples':
            return 'example'
        elif first_dir in ('benchmarks', 'bench'):
            return 'benchmark'
        elif first_dir == 'fuzz':
            return 'fuzz'
        else:
            return 'code'

    def is_definition(self, line: str, symbol: str) -> bool:
        """
        Heuristically determine if a line contains a definition.

        Looks for patterns like:
        - class Foo
        - struct Foo
        - void foo(
        - int foo =
        - typedef ... foo
        - using foo =
        """
        # Remove leading whitespace and comments
        stripped = line.strip()

        # Skip comment-only lines
        if stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('/*'):
            return False

        # Common definition patterns
        definition_patterns = [
            rf'\bclass\s+{re.escape(symbol)}\b',
            rf'\bstruct\s+{re.escape(symbol)}\b',
            rf'\benum\s+{re.escape(symbol)}\b',
            rf'\bunion\s+{re.escape(symbol)}\b',
            rf'\btypedef\s+.*\b{re.escape(symbol)}\b',
            rf'\busing\s+{re.escape(symbol)}\s*=',
            # Function definition (return type, symbol, opening paren)
            rf'\b\w+[\s\*&]+{re.escape(symbol)}\s*\(',
            rf'^{re.escape(symbol)}\s*\(',  # Constructor
            # Member variable or global variable
            rf'\b\w+[\s\*&]+{re.escape(symbol)}\s*[=;]',
        ]

        for pattern in definition_patterns:
            if re.search(pattern, stripped):
                return True

        return False

    def search_symbol(self, symbol: str) -> List[Match]:
        """Search for a symbol across the codebase."""
        matches = []

        # Build list of all files to search
        all_files = set()  # Use set to avoid duplicates
        for category, dirs in self.SEARCH_PATHS.items():
            for dir_name in dirs:
                dir_path = self.root_dir / dir_name
                if dir_path.exists():
                    for ext in self.CODE_EXTENSIONS:
                        all_files.update(dir_path.rglob(f'*{ext}'))

        # Search each file
        for file_path in all_files:
            try:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    for line_num, line in enumerate(f, 1):
                        # Check if symbol appears in line
                        # Use word boundary to avoid partial matches
                        if re.search(rf'\b{re.escape(symbol)}\b', line):
                            category = self.categorize_file(file_path)

                            # Determine if this is a definition or usage
                            if self.is_definition(line, symbol):
                                subcategory = 'definition'
                            else:
                                subcategory = category

                            match = Match(
                                file=file_path.relative_to(self.root_dir),
                                line_num=line_num,
                                line=line,
                                category=subcategory
                            )
                            matches.append(match)
            except Exception as e:
                # Skip files that can't be read
                continue

        return matches

    def print_results(self, symbol: str, matches: List[Match]):
        """Print search results in a nice format."""
        if not matches:
            print(f"No matches found for '{symbol}'")
            return

        # Group matches by category
        by_category = defaultdict(list)
        for match in matches:
            by_category[match.category].append(match)

        # Print summary
        print(f"\nFound {len(matches)} matches for '{symbol}':\n")

        # Print definitions first
        if 'definition' in by_category:
            print(f"DEFINITIONS ({len(by_category['definition'])}):")
            print("-" * 80)
            for match in by_category['definition']:
                print(f"  {match}")
            print()

        # Print other categories
        category_order = ['code', 'test', 'example', 'benchmark', 'fuzz']
        for category in category_order:
            if category in by_category:
                matches_in_cat = by_category[category]
                print(f"{category.upper()} USAGES ({len(matches_in_cat)}):")
                print("-" * 80)
                for match in matches_in_cat:
                    print(f"  {match}")
                print()


def main():
    """Main entry point."""
    if len(sys.argv) != 2:
        print("Usage: uv run python search.py <symbol>")
        print("\nExamples:")
        print("  uv run python search.py NetworkClient")
        print("  uv run python search.py connect")
        print("  uv run python search.py server_address")
        sys.exit(1)

    symbol = sys.argv[1]

    # Find project root (.claude/skills/search -> .claude/skills -> .claude -> root)
    script_dir = Path(__file__).parent
    root_dir = script_dir.parent.parent.parent

    # Create searcher and run search
    searcher = CodeSearch(root_dir)
    matches = searcher.search_symbol(symbol)
    searcher.print_results(symbol, matches)


if __name__ == '__main__':
    main()
