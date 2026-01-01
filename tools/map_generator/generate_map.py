#!/usr/bin/env python3
"""
Complete map generation pipeline: WFC + TMX export

Usage:
    python generate_map.py --width 20 --height 20 --output ../../assets/generated_map.tmx
"""

import argparse
import json
import sys
from pathlib import Path

# Import our modules
from wfc_generator import WFCGenerator
from tmx_generator import TMXGenerator


def main():
    parser = argparse.ArgumentParser(
        description="Generate procedural isometric maps using Wave Function Collapse"
    )
    parser.add_argument("--tiles", default="tiles_starter.json",
                       help="Path to tiles JSON file (default: tiles_starter.json)")
    parser.add_argument("--width", type=int, default=20,
                       help="Map width in tiles (default: 20)")
    parser.add_argument("--height", type=int, default=20,
                       help="Map height in tiles (default: 20)")
    parser.add_argument("--seed", type=int, default=None,
                       help="Random seed for reproducibility")
    parser.add_argument("--output", required=True,
                       help="Output .tmx file path (e.g., ../../assets/generated_map.tmx)")
    parser.add_argument("--save-grid", action="store_true",
                       help="Also save the raw grid as JSON for debugging")

    args = parser.parse_args()

    print("="*60)
    print("Wave Function Collapse Map Generator")
    print("="*60)
    print(f"Tiles: {args.tiles}")
    print(f"Dimensions: {args.width}x{args.height}")
    print(f"Output: {args.output}")
    if args.seed:
        print(f"Seed: {args.seed}")
    print("="*60)
    print()

    # Step 1: Generate map with WFC
    print("[1/2] Generating map with Wave Function Collapse...")
    generator = WFCGenerator(args.tiles)
    grid = generator.generate(args.width, args.height, args.seed)

    if grid is None:
        print("\nERROR: Map generation failed!")
        print("This can happen if the adjacency rules are too restrictive.")
        print("Try:")
        print("  - Using a larger map size")
        print("  - Adding more tile variety")
        print("  - Relaxing edge constraints")
        sys.exit(1)

    # Optionally save grid JSON for debugging
    if args.save_grid:
        grid_path = args.output.replace(".tmx", "_grid.json")
        with open(grid_path, 'w') as f:
            json.dump(grid, f, indent=2)
        print(f"Saved grid JSON to: {grid_path}")

    print()

    # Step 2: Export to TMX
    print("[2/2] Exporting to TMX format...")
    tmx_generator = TMXGenerator(args.tiles)
    tmx_generator.generate_tmx(grid, args.output)

    print()
    print("="*60)
    print("SUCCESS!")
    print("="*60)
    print(f"Generated map saved to: {args.output}")
    print()
    print("Next steps:")
    print(f"  1. Load the map in your game:")
    print(f"       TiledMap map;")
    print(f"       map.load(\"{args.output}\");")
    print()
    print(f"  2. Or open in Tiled editor to inspect/modify:")
    print(f"       tiled {args.output}")


if __name__ == "__main__":
    main()
