#!/usr/bin/env python3
"""
Tile Analyzer - Helper tool to analyze tileset PNGs and define tile metadata.

This tool helps you:
1. Visualize the tileset with grid overlays
2. Identify tile positions (row, col) for tiles.json
3. View tiles and their collision maps side-by-side
4. Export tile definitions in the correct JSON format
"""

import json
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: Pillow is required. Install with: pip install Pillow")
    sys.exit(1)


class TileAnalyzer:
    def __init__(self, tileset_path, collision_path=None, tile_width=128, tile_height=128):
        self.tileset_path = Path(tileset_path)
        self.collision_path = Path(collision_path) if collision_path else None
        self.tile_width = tile_width
        self.tile_height = tile_height

        # Load images
        self.tileset_img = Image.open(self.tileset_path).convert("RGBA")
        self.collision_img = Image.open(self.collision_path).convert("RGBA") if self.collision_path else None

        # Calculate grid dimensions
        self.columns = self.tileset_img.width // self.tile_width
        self.rows = self.tileset_img.height // self.tile_height

        print(f"Loaded tileset: {self.tileset_path.name}")
        print(f"Dimensions: {self.tileset_img.width}x{self.tileset_img.height}")
        print(f"Tile size: {self.tile_width}x{self.tile_height}")
        print(f"Grid: {self.columns} columns x {self.rows} rows = {self.columns * self.rows} tiles")

    def create_grid_overlay(self, output_path="tileset_grid.png"):
        """Create a visualization with grid lines and tile IDs."""
        # Create a copy to draw on
        img = self.tileset_img.copy()
        draw = ImageDraw.Draw(img)

        # Try to use a font, fall back to default if not available
        try:
            font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 20)
        except:
            font = ImageFont.load_default()

        # Draw grid lines
        for col in range(self.columns + 1):
            x = col * self.tile_width
            draw.line([(x, 0), (x, img.height)], fill=(255, 0, 0, 128), width=2)

        for row in range(self.rows + 1):
            y = row * self.tile_height
            draw.line([(0, y), (img.width, y)], fill=(255, 0, 0, 128), width=2)

        # Draw tile IDs and positions
        for row in range(self.rows):
            for col in range(self.columns):
                tile_id = row * self.columns + col
                x = col * self.tile_width + 5
                y = row * self.tile_height + 5

                text = f"ID:{tile_id}\n({row},{col})"

                # Draw text with background
                bbox = draw.textbbox((x, y), text, font=font)
                draw.rectangle(bbox, fill=(0, 0, 0, 180))
                draw.text((x, y), text, fill=(255, 255, 0, 255), font=font)

        img.save(output_path)
        print(f"Grid overlay saved to: {output_path}")
        return output_path

    def create_side_by_side(self, output_path="tileset_comparison.png"):
        """Create side-by-side comparison of tileset and collision map."""
        if not self.collision_img:
            print("No collision map provided, skipping side-by-side comparison")
            return None

        # Create new image with both side by side
        width = self.tileset_img.width + self.collision_img.width + 20
        height = max(self.tileset_img.height, self.collision_img.height)

        combined = Image.new("RGBA", (width, height), (50, 50, 50, 255))
        combined.paste(self.tileset_img, (0, 0))
        combined.paste(self.collision_img, (self.tileset_img.width + 20, 0))

        # Add labels
        draw = ImageDraw.Draw(combined)
        try:
            font = ImageFont.truetype("/System/Library/Fonts/Helvetica.ttc", 30)
        except:
            font = ImageFont.load_default()

        draw.text((10, 10), "TILESET", fill=(255, 255, 0, 255), font=font)
        draw.text((self.tileset_img.width + 30, 10), "COLLISION MAP", fill=(255, 255, 0, 255), font=font)

        combined.save(output_path)
        print(f"Side-by-side comparison saved to: {output_path}")
        return output_path

    def extract_tile(self, tile_id, output_path=None):
        """Extract a specific tile by ID."""
        row = tile_id // self.columns
        col = tile_id % self.columns

        x = col * self.tile_width
        y = row * self.tile_height

        tile = self.tileset_img.crop((x, y, x + self.tile_width, y + self.tile_height))

        if output_path:
            tile.save(output_path)
            print(f"Tile {tile_id} saved to: {output_path}")

        return tile

    def list_all_tiles(self):
        """Print a list of all tile IDs and their grid positions."""
        print("\nAll Tiles:")
        print("ID  | Row | Col | Position")
        print("----|-----|-----|----------")
        for row in range(self.rows):
            for col in range(self.columns):
                tile_id = row * self.columns + col
                print(f"{tile_id:3d} | {row:3d} | {col:3d} | ({col * self.tile_width}, {row * self.tile_height})")

    def generate_skeleton_json(self, output_path="tiles_skeleton.json"):
        """Generate a skeleton tiles.json with all tile IDs populated."""
        tiles = []

        for row in range(self.rows):
            for col in range(self.columns):
                tile_id = row * self.columns + col

                tile_def = {
                    "id": tile_id,
                    "name": f"tile_{tile_id}",
                    "row": row,
                    "col": col,
                    "description": "TODO: Add description",
                    "edges": {
                        "north": "TODO",
                        "south": "TODO",
                        "east": "TODO",
                        "west": "TODO"
                    },
                    "weight": 1,
                    "collision_tile_id": tile_id,
                    "notes": "Auto-generated - needs manual review"
                }
                tiles.append(tile_def)

        skeleton = {
            "tiles": tiles,
            "note": "This is an auto-generated skeleton. Review and update edge types, weights, and descriptions."
        }

        with open(output_path, 'w') as f:
            json.dump(skeleton, f, indent=2)

        print(f"Skeleton JSON saved to: {output_path}")
        return output_path


def main():
    if len(sys.argv) < 2:
        print("Usage: python tile_analyzer.py <tileset.png> [collision_map.png]")
        print("\nExample:")
        print("  python tile_analyzer.py ../../assets/maps/all_tiles.png ../../assets/maps/collison_map.png")
        sys.exit(1)

    tileset_path = sys.argv[1]
    collision_path = sys.argv[2] if len(sys.argv) > 2 else None

    analyzer = TileAnalyzer(tileset_path, collision_path)

    print("\n" + "="*60)
    print("Generating analysis outputs...")
    print("="*60)

    # Generate all outputs
    analyzer.create_grid_overlay("tileset_grid.png")
    if collision_path:
        analyzer.create_side_by_side("tileset_comparison.png")
    analyzer.list_all_tiles()
    analyzer.generate_skeleton_json("tiles_skeleton.json")

    print("\n" + "="*60)
    print("Analysis complete!")
    print("="*60)
    print("\nNext steps:")
    print("1. Open 'tileset_grid.png' to see tile IDs overlaid on the tileset")
    print("2. Use the grid to identify which tiles are which")
    print("3. Edit 'tiles_skeleton.json' to add proper names, edge types, and descriptions")
    print("4. Merge your edits into 'tiles.json'")


if __name__ == "__main__":
    main()
