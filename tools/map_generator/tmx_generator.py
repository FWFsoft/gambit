#!/usr/bin/env python3
"""
TMX File Generator

Converts WFC-generated tile grids into Tiled .tmx format.
"""

import json
import argparse
from pathlib import Path
from typing import List
import xml.etree.ElementTree as ET
from xml.dom import minidom


class TMXGenerator:
    """Generates Tiled .tmx files from WFC grids."""

    def __init__(self, tiles_json_path: str):
        """Initialize with tileset information."""
        with open(tiles_json_path, 'r') as f:
            data = json.load(f)

        self.tileset_info = data["tileset"]

    def generate_tmx(self, grid: List[List[int]], output_path: str):
        """
        Generate a .tmx file from a WFC grid.

        Args:
            grid: 2D list of tile IDs (grid[y][x])
            output_path: Path to save the .tmx file
        """
        height = len(grid)
        width = len(grid[0]) if height > 0 else 0

        print(f"Generating TMX file: {width}x{height} map")

        # Create root map element
        map_elem = ET.Element("map", {
            "version": "1.10",
            "tiledversion": "1.10.2",
            "orientation": "isometric",
            "renderorder": "right-down",
            "width": str(width),
            "height": str(height),
            "tilewidth": str(self.tileset_info["tile_width"]),
            "tileheight": str(self.tileset_info["tile_height"]),
            "infinite": "0",
            "nextlayerid": "2",
            "nextobjectid": "1"
        })

        # Add tileset reference
        tileset_attrs = {
            "firstgid": str(self.tileset_info.get("firstgid", 1)),
            "name": self.tileset_info["name"],
            "tilewidth": str(self.tileset_info["tile_width"]),
            "tileheight": str(self.tileset_info["tile_height"]),
            "tilecount": str(self.tileset_info["columns"] * 7),  # 7 rows in the tileset
            "columns": str(self.tileset_info["columns"])
        }

        # Add spacing if present
        if "spacing" in self.tileset_info:
            tileset_attrs["spacing"] = str(self.tileset_info["spacing"])

        tileset_elem = ET.SubElement(map_elem, "tileset", tileset_attrs)

        # Add image source (relative path from output file)
        image_path = self.tileset_info["image"]
        spacing = self.tileset_info.get("spacing", 0)
        columns = self.tileset_info["columns"]
        rows = 7  # 7 rows in the tileset

        # Calculate image dimensions accounting for spacing
        image_width = (self.tileset_info["tile_width"] * columns) + (spacing * (columns - 1))
        image_height = (self.tileset_info["tile_height"] * rows) + (spacing * (rows - 1))

        ET.SubElement(tileset_elem, "image", {
            "source": image_path,
            "width": str(image_width),
            "height": str(image_height)
        })

        # Add Ground layer
        layer_elem = ET.SubElement(map_elem, "layer", {
            "id": "1",
            "name": "Ground",
            "width": str(width),
            "height": str(height)
        })

        data_elem = ET.SubElement(layer_elem, "data", {"encoding": "csv"})

        # Convert grid to CSV format
        # NOTE: TMX tile IDs are 1-indexed (firstgid=1), so we add 1 to each tile ID
        firstgid = self.tileset_info.get("firstgid", 1)

        csv_lines = []
        for row in grid:
            csv_line = ",".join(str(tile_id + firstgid) for tile_id in row)
            csv_lines.append(csv_line)

        csv_data = ",\n".join(csv_lines)
        data_elem.text = "\n" + csv_data + "\n"

        # TODO: Add collision objects based on collision_map
        # For now, we'll generate maps without collision objects
        # You can manually add them in Tiled or extend this generator later

        # Convert to pretty-printed XML
        xml_str = self._prettify_xml(map_elem)

        # Write to file
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(xml_str)

        print(f"Saved TMX file to: {output_path}")

    def _prettify_xml(self, elem: ET.Element) -> str:
        """Return a pretty-printed XML string."""
        rough_string = ET.tostring(elem, encoding='unicode')
        reparsed = minidom.parseString(rough_string)
        return reparsed.toprettyxml(indent="  ")


def main():
    parser = argparse.ArgumentParser(description="Convert WFC JSON grid to TMX format")
    parser.add_argument("--tiles", default="tiles_starter.json",
                       help="Path to tiles JSON file")
    parser.add_argument("--grid", required=True,
                       help="Path to WFC-generated grid JSON file")
    parser.add_argument("--output", required=True,
                       help="Output .tmx file path")

    args = parser.parse_args()

    # Load grid
    with open(args.grid, 'r') as f:
        grid = json.load(f)

    # Generate TMX
    generator = TMXGenerator(args.tiles)
    generator.generate_tmx(grid, args.output)

    print(f"\nSuccess! Generated TMX file")
    print(f"You can now load this in your game with:")
    print(f"  TiledMap map;")
    print(f"  map.load(\"{args.output}\");")


if __name__ == "__main__":
    main()
