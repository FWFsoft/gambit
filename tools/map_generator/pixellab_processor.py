#!/usr/bin/env python3
"""
PixelLab Tile Processor

Fetches tiles from PixelLab, organizes them into a tileset sprite sheet,
and generates metadata JSON for WFC generation.
"""

import argparse
import base64
import json
import sys
from pathlib import Path
from typing import Dict, List, Optional
from PIL import Image
import io


class PixelLabProcessor:
    """Process PixelLab tiles into a tileset."""

    def __init__(self, output_dir: Path):
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.tiles_dir = output_dir / "pixellab_tiles"
        self.tiles_dir.mkdir(exist_ok=True)

    def load_tile_manifest(self, manifest_path: str) -> Dict:
        """Load tile manifest with PixelLab IDs and metadata."""
        with open(manifest_path, 'r') as f:
            return json.load(f)

    def create_tileset_sheet(self, tile_files: List[Path],
                            columns: int = 7,
                            spacing: int = 4,
                            tile_size: int = 64) -> Image.Image:
        """
        Create a tileset sprite sheet from individual tile images.

        Args:
            tile_files: List of paths to tile PNG files
            columns: Number of columns in the sprite sheet
            spacing: Pixels between tiles
            tile_size: Size of each tile (assumes square)

        Returns:
            PIL Image of the complete tileset
        """
        num_tiles = len(tile_files)
        rows = (num_tiles + columns - 1) // columns

        # Calculate total dimensions with spacing
        sheet_width = columns * tile_size + (columns - 1) * spacing
        sheet_height = rows * tile_size + (rows - 1) * spacing

        # Create RGBA image with transparent background
        sheet = Image.new('RGBA', (sheet_width, sheet_height), (0, 0, 0, 0))

        # Place each tile
        for idx, tile_path in enumerate(tile_files):
            row = idx // columns
            col = idx % columns

            x = col * (tile_size + spacing)
            y = row * (tile_size + spacing)

            tile_img = Image.open(tile_path)

            # Verify size
            if tile_img.size != (tile_size, tile_size):
                print(f"Warning: {tile_path.name} is {tile_img.size}, expected ({tile_size}, {tile_size})")
                # Resize if needed
                tile_img = tile_img.resize((tile_size, tile_size), Image.Resampling.LANCZOS)

            sheet.paste(tile_img, (x, y))
            print(f"  Placed tile {idx}: {tile_path.name} at ({x}, {y})")

        return sheet

    def generate_tile_metadata(self, manifest: Dict, columns: int = 7) -> Dict:
        """
        Generate WFC-compatible tile metadata JSON.

        Args:
            manifest: Tile manifest with PixelLab IDs and descriptions
            columns: Number of columns in sprite sheet

        Returns:
            Tile configuration dictionary
        """
        tiles = []

        for idx, tile_entry in enumerate(manifest['tiles']):
            row = idx // columns
            col = idx % columns

            tile = {
                'id': idx,
                'name': tile_entry['name'],
                'row': row,
                'col': col,
                'edges': tile_entry.get('edges', {
                    'north': 'flat_moss',
                    'south': 'flat_moss',
                    'east': 'flat_moss',
                    'west': 'flat_moss'
                }),
                'weight': tile_entry.get('weight', 5),
                'pixellab_tile_id': tile_entry['pixellab_id'],
                'description': tile_entry.get('description', '')
            }
            tiles.append(tile)

        config = {
            'tileset': {
                'name': manifest.get('tileset_name', 'venoxarid_organic'),
                'image': '../../assets/maps/venoxarid_tileset.png',
                'tile_width': manifest.get('tile_width', 64),
                'tile_height': manifest.get('tile_height', 64),
                'spacing': manifest.get('spacing', 4),
                'columns': columns,
                'firstgid': 1,
                'description': manifest.get('description', 'Venoxarid organic terrain tileset')
            },
            'edge_types': manifest.get('edge_types', [
                'flat_moss',
                'flat_moss_dense',
                'flat_moss_vine',
                'flat_moss_root',
                'toxic_pool',
                'void'
            ]),
            'tiles': tiles
        }

        return config


def main():
    parser = argparse.ArgumentParser(
        description="Process PixelLab tiles into a tileset sprite sheet"
    )
    parser.add_argument('--manifest', required=True,
                       help='JSON manifest with tile IDs and metadata')
    parser.add_argument('--output-sheet',
                       default='../../assets/maps/venoxarid_tileset.png',
                       help='Output path for tileset sprite sheet')
    parser.add_argument('--output-json',
                       default='tiles_venoxarid.json',
                       help='Output path for tile configuration JSON')
    parser.add_argument('--columns', type=int, default=7,
                       help='Number of columns in sprite sheet')
    parser.add_argument('--spacing', type=int, default=4,
                       help='Pixels between tiles')
    parser.add_argument('--tile-size', type=int, default=64,
                       help='Tile size in pixels (square)')

    args = parser.parse_args()

    # Load manifest
    with open(args.manifest, 'r') as f:
        manifest = json.load(f)

    print(f"Loaded manifest with {len(manifest['tiles'])} tiles")

    # Get tile files from manifest
    tile_files = []
    for tile_entry in manifest['tiles']:
        tile_path = Path(tile_entry['file'])
        if not tile_path.exists():
            print(f"Error: Tile file not found: {tile_path}")
            sys.exit(1)
        tile_files.append(tile_path)

    # Create processor
    processor = PixelLabProcessor(Path.cwd())

    # Generate sprite sheet
    print(f"\nCreating {args.columns}-column sprite sheet...")
    sheet = processor.create_tileset_sheet(
        tile_files,
        columns=args.columns,
        spacing=args.spacing,
        tile_size=args.tile_size
    )

    # Save sprite sheet
    output_sheet_path = Path(args.output_sheet)
    output_sheet_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_sheet_path)
    print(f"\nSaved sprite sheet: {output_sheet_path}")
    print(f"  Dimensions: {sheet.size}")

    # Generate metadata JSON
    print(f"\nGenerating tile metadata...")
    tile_config = processor.generate_tile_metadata(manifest, args.columns)

    with open(args.output_json, 'w') as f:
        json.dump(tile_config, f, indent=2)
    print(f"Saved tile configuration: {args.output_json}")

    print(f"\n✓ Processing complete!")
    print(f"  Tileset: {output_sheet_path}")
    print(f"  Config: {args.output_json}")
    print(f"  Tiles: {len(tile_files)}")


if __name__ == '__main__':
    main()
