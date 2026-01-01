#!/usr/bin/env python3
"""
Wave Function Collapse Map Generator

Generates procedural isometric maps using the WFC algorithm.
"""

import argparse
import json
import random
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional


class Tile:
    """Represents a tile with its adjacency rules."""

    def __init__(self, tile_data: dict):
        self.id = tile_data["id"]
        self.name = tile_data["name"]
        self.edges = tile_data["edges"]  # {north, south, east, west}
        self.weight = tile_data.get("weight", 1)
        self.row = tile_data.get("row", 0)
        self.col = tile_data.get("col", 0)

    def __repr__(self):
        return f"Tile({self.id}, {self.name})"

    def can_connect(self, other: 'Tile', direction: str) -> bool:
        """
        Check if this tile can connect to another tile in a given direction.

        Args:
            other: The other tile to check
            direction: One of 'north', 'south', 'east', 'west'

        Returns:
            True if tiles can be adjacent
        """
        # Map direction to the opposite edge on the other tile
        opposite = {
            'north': 'south',
            'south': 'north',
            'east': 'west',
            'west': 'east'
        }

        my_edge = self.edges[direction]
        their_edge = other.edges[opposite[direction]]

        return my_edge == their_edge


class WFCGenerator:
    """Wave Function Collapse map generator."""

    def __init__(self, tiles_json_path: str):
        """Initialize the generator with tile definitions."""
        with open(tiles_json_path, 'r') as f:
            data = json.load(f)

        self.tileset_info = data["tileset"]
        self.tiles = [Tile(t) for t in data["tiles"]]
        self.tile_by_id = {t.id: t for t in self.tiles}

        print(f"Loaded {len(self.tiles)} tile definitions")

        # Precompute adjacency rules
        self.adjacency_rules = self._compute_adjacency_rules()

    def _compute_adjacency_rules(self) -> Dict[int, Dict[str, Set[int]]]:
        """
        Precompute which tiles can be adjacent to each other.

        Returns:
            Dict mapping tile_id -> direction -> set of valid neighbor tile IDs
        """
        rules = {}

        for tile in self.tiles:
            rules[tile.id] = {
                'north': set(),
                'south': set(),
                'east': set(),
                'west': set()
            }

            for other in self.tiles:
                for direction in ['north', 'south', 'east', 'west']:
                    if tile.can_connect(other, direction):
                        rules[tile.id][direction].add(other.id)

        return rules

    def generate(self, width: int, height: int, seed: Optional[int] = None) -> List[List[int]]:
        """
        Generate a map using Wave Function Collapse.

        Args:
            width: Width of the map in tiles
            height: Height of the map in tiles
            seed: Random seed for reproducible generation

        Returns:
            2D list of tile IDs (grid[y][x])
        """
        if seed is not None:
            random.seed(seed)

        print(f"Generating {width}x{height} map...")

        # Initialize grid - each cell starts with all possible tiles
        all_tile_ids = set(t.id for t in self.tiles)
        possibilities = [[all_tile_ids.copy() for _ in range(width)] for _ in range(height)]

        # Track collapsed cells
        collapsed = [[False] * width for _ in range(height)]

        # WFC main loop
        iterations = 0
        max_iterations = width * height * 100

        while not self._is_fully_collapsed(collapsed) and iterations < max_iterations:
            iterations += 1

            # Find cell with minimum entropy (fewest possibilities, but > 1)
            min_cell = self._find_min_entropy_cell(possibilities, collapsed)

            if min_cell is None:
                # All cells are collapsed
                break

            y, x = min_cell

            # Collapse the cell - choose one tile randomly (weighted)
            chosen_tile = self._collapse_cell(possibilities[y][x])
            possibilities[y][x] = {chosen_tile}
            collapsed[y][x] = True

            if iterations % 10 == 0:
                collapsed_count = sum(sum(row) for row in collapsed)
                print(f"  Iteration {iterations}: Collapsed {collapsed_count}/{width * height} cells")

            # Propagate constraints to neighbors
            if not self._propagate(possibilities, collapsed, x, y, width, height):
                print("ERROR: Contradiction detected! Map generation failed.")
                print(f"  Failed at cell ({x}, {y})")
                return None

        if iterations >= max_iterations:
            print(f"WARNING: Max iterations ({max_iterations}) reached")
            return None

        # Convert possibilities to final grid
        grid = [[list(possibilities[y][x])[0] for x in range(width)] for y in range(height)]

        print(f"Generation complete in {iterations} iterations")
        return grid

    def _is_fully_collapsed(self, collapsed: List[List[bool]]) -> bool:
        """Check if all cells are collapsed."""
        return all(all(row) for row in collapsed)

    def _find_min_entropy_cell(self, possibilities: List[List[Set[int]]],
                                collapsed: List[List[bool]]) -> Optional[Tuple[int, int]]:
        """Find the cell with minimum entropy (fewest possibilities, but > 0)."""
        min_entropy = float('inf')
        min_cell = None

        for y in range(len(possibilities)):
            for x in range(len(possibilities[0])):
                if collapsed[y][x]:
                    continue

                entropy = len(possibilities[y][x])

                if entropy == 0:
                    # Contradiction - no valid tiles for this cell
                    return None

                # Add small random noise to break ties randomly
                entropy_with_noise = entropy + random.random() * 0.1

                if entropy_with_noise < min_entropy:
                    min_entropy = entropy_with_noise
                    min_cell = (y, x)

        return min_cell

    def _collapse_cell(self, possible_tiles: Set[int]) -> int:
        """Choose one tile from the possibilities, weighted by tile weights."""
        tiles = [self.tile_by_id[tid] for tid in possible_tiles]
        weights = [t.weight for t in tiles]

        chosen = random.choices(tiles, weights=weights, k=1)[0]
        return chosen.id

    def _propagate(self, possibilities: List[List[Set[int]]],
                   collapsed: List[List[bool]],
                   x: int, y: int, width: int, height: int) -> bool:
        """
        Propagate constraints from a collapsed cell to its neighbors.

        Returns:
            False if a contradiction is detected, True otherwise
        """
        # Stack of cells to propagate from
        stack = [(x, y)]

        while stack:
            cx, cy = stack.pop()
            current_tiles = possibilities[cy][cx]

            # Check all 4 neighbors
            neighbors = [
                (cx, cy - 1, 'north'),  # North neighbor
                (cx, cy + 1, 'south'),  # South neighbor
                (cx + 1, cy, 'east'),   # East neighbor
                (cx - 1, cy, 'west'),   # West neighbor
            ]

            for nx, ny, direction in neighbors:
                # Skip out of bounds
                if nx < 0 or nx >= width or ny < 0 or ny >= height:
                    continue

                # Skip already collapsed cells
                if collapsed[ny][nx]:
                    continue

                # Compute valid tiles for neighbor based on current cell
                valid_for_neighbor = set()
                for current_tile_id in current_tiles:
                    valid_for_neighbor.update(self.adjacency_rules[current_tile_id][direction])

                # Intersect with neighbor's current possibilities
                old_possibilities = possibilities[ny][nx]
                new_possibilities = old_possibilities & valid_for_neighbor

                if len(new_possibilities) == 0:
                    # Contradiction!
                    return False

                # If we reduced possibilities, add neighbor to stack for further propagation
                if len(new_possibilities) < len(old_possibilities):
                    possibilities[ny][nx] = new_possibilities
                    stack.append((nx, ny))

        return True

    def save_as_json(self, grid: List[List[int]], output_path: str):
        """Save generated grid as JSON (for debugging)."""
        with open(output_path, 'w') as f:
            json.dump(grid, f, indent=2)
        print(f"Saved map grid to: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="Generate WFC maps")
    parser.add_argument("--tiles", default="tiles_starter.json",
                       help="Path to tiles JSON file")
    parser.add_argument("--width", type=int, default=20,
                       help="Map width in tiles")
    parser.add_argument("--height", type=int, default=20,
                       help="Map height in tiles")
    parser.add_argument("--seed", type=int, default=None,
                       help="Random seed for reproducibility")
    parser.add_argument("--output", default="generated_map.json",
                       help="Output file path (.json or .tmx)")

    args = parser.parse_args()

    # Generate map
    generator = WFCGenerator(args.tiles)
    grid = generator.generate(args.width, args.height, args.seed)

    if grid is None:
        print("Map generation failed!")
        sys.exit(1)

    # Save as JSON for now (TMX export coming next)
    generator.save_as_json(grid, args.output)

    print(f"\nSuccess! Generated {args.width}x{args.height} map")
    print(f"Output: {args.output}")


if __name__ == "__main__":
    main()
