#!/usr/bin/env python3
"""
Batch generate Venoxarid isometric tiles using PixelLab MCP server.

This script generates 25-28 tiles organized into themed groups:
- Light Moss (5 tiles): Common, walkable ground
- Dense Moss (5 tiles): Medium density vegetation
- Bioluminescent (6 tiles): Glowing features for visual interest
- Root Systems (4 tiles): Organic root networks
- Toxic Transitions (3 tiles): Edges to toxic pools
- Toxic Pools (2-3 tiles): Liquid hazards

Outputs:
- Individual tiles saved to pixellab_tiles/venoxarid_full/
- Manifest JSON for pixellab_processor.py
"""

import json
import time
from pathlib import Path
from typing import List, Dict, Any
import sys

# Try to import MCP functions (will work when run via Claude Code)
try:
    from anthropic import Anthropic
    MCP_AVAILABLE = True
except ImportError:
    MCP_AVAILABLE = False
    print("Warning: MCP not available in this context")


# Tile definitions organized by theme
TILE_GROUPS = {
    "light_moss": [
        {
            "name": "sparse_light_moss",
            "description": "sparse light green moss on dark organic ground",
            "theme": "light_moss",
        },
        {
            "name": "light_moss_patches",
            "description": "light moss patches with small yellow roots",
            "theme": "light_moss",
        },
        {
            "name": "thin_moss_spores",
            "description": "thin moss covering with scattered pink spores",
            "theme": "light_moss",
        },
        {
            "name": "light_moss_biolum",
            "description": "light green moss with small bioluminescent spots",
            "theme": "light_moss",
        },
        {
            "name": "moss_transition",
            "description": "sparse moss transitioning to darker moss",
            "theme": "light_moss",
        },
    ],
    "dense_moss": [
        {
            "name": "dense_dark_moss",
            "description": "dense dark green moss carpet",
            "theme": "dense_moss",
        },
        {
            "name": "thick_moss_mushrooms",
            "description": "thick moss coverage with small mushrooms",
            "theme": "dense_moss",
        },
        {
            "name": "heavy_moss_fungi",
            "description": "heavy dark moss with pink bioluminescent fungi",
            "theme": "dense_moss",
        },
        {
            "name": "dense_moss_roots",
            "description": "dense moss with twisted yellow root systems",
            "theme": "dense_moss",
        },
        {
            "name": "dark_moss_flowers",
            "description": "dark green moss with scattered pink flowers",
            "theme": "dense_moss",
        },
    ],
    "bioluminescent": [
        {
            "name": "pink_mushroom_clusters",
            "description": "moss with large glowing pink mushroom clusters",
            "theme": "bioluminescent",
        },
        {
            "name": "yellow_glowing_vines",
            "description": "dark moss with bright yellow glowing vines",
            "theme": "bioluminescent",
        },
        {
            "name": "pink_spore_clouds",
            "description": "moss ground with pink bioluminescent spore clouds",
            "theme": "bioluminescent",
        },
        {
            "name": "yellow_root_glow",
            "description": "thick moss with glowing yellow root networks",
            "theme": "bioluminescent",
        },
        {
            "name": "mixed_glow_fungi",
            "description": "moss with pink and yellow glowing fungi mix",
            "theme": "bioluminescent",
        },
        {
            "name": "dense_pink_mushrooms",
            "description": "dark ground with dense pink bioluminescent mushrooms",
            "theme": "bioluminescent",
        },
    ],
    "root_systems": [
        {
            "name": "twisted_yellow_roots",
            "description": "moss with twisted yellow organic root systems",
            "theme": "root_systems",
        },
        {
            "name": "tangled_vine_networks",
            "description": "tangled glowing yellow vine networks on moss",
            "theme": "root_systems",
        },
        {
            "name": "roots_pink_spores",
            "description": "moss with thick organic yellow roots and pink spores",
            "theme": "root_systems",
        },
        {
            "name": "root_clusters",
            "description": "yellow root clusters breaking through moss",
            "theme": "root_systems",
        },
    ],
    "toxic_transitions": [
        {
            "name": "moss_to_toxic_edge",
            "description": "moss edge transitioning to toxic green liquid",
            "theme": "toxic_transition",
        },
        {
            "name": "contaminated_moss",
            "description": "contaminated moss with yellow-green toxic seepage",
            "theme": "toxic_transition",
        },
        {
            "name": "toxic_pool_forming",
            "description": "moss ground with toxic green pool forming",
            "theme": "toxic_transition",
        },
    ],
    "toxic_pools": [
        {
            "name": "bubbling_toxic_pool",
            "description": "bubbling toxic green liquid pool surface",
            "theme": "toxic_pool",
        },
        {
            "name": "acidic_sludge",
            "description": "toxic green acidic sludge with yellow foam",
            "theme": "toxic_pool",
        },
        {
            "name": "toxic_algae",
            "description": "toxic pool with pink bioluminescent algae",
            "theme": "toxic_pool",
        },
    ],
}

# Common PixelLab parameters for all tiles
PIXELLAB_PARAMS = {
    "size": 64,
    "view": "high top-down",
    "tile_shape": "thin tile",
    "detail": "medium detail",
    "shading": "medium shading",
    "outline": "lineless",
}


def create_tile_batch() -> List[Dict[str, Any]]:
    """
    Create batch of tile jobs with PixelLab.

    Returns:
        List of tile job info with tile_id and metadata
    """
    print("=" * 80)
    print("VENOXARID TILESET BATCH GENERATION")
    print("=" * 80)
    print(f"\nGenerating {sum(len(tiles) for tiles in TILE_GROUPS.values())} tiles...")
    print(f"Parameters: {PIXELLAB_PARAMS}\n")

    tile_jobs = []

    for group_name, tiles in TILE_GROUPS.items():
        print(f"\n{group_name.upper()} ({len(tiles)} tiles):")
        print("-" * 40)

        for tile_def in tiles:
            print(f"  - {tile_def['name']}: '{tile_def['description']}'")

            # This is a placeholder - actual MCP calls would be made by Claude
            tile_jobs.append({
                "name": tile_def["name"],
                "description": tile_def["description"],
                "theme": tile_def["theme"],
                "tile_id": None,  # Will be filled by MCP call
                "status": "pending",
            })

    print(f"\n\nTotal tiles queued: {len(tile_jobs)}")
    print("\nNOTE: This script outputs the tile definitions.")
    print("Claude Code will make the actual MCP calls to generate tiles.\n")

    return tile_jobs


def save_manifest(tile_jobs: List[Dict[str, Any]], output_path: Path):
    """
    Save manifest JSON for pixellab_processor.py

    Args:
        tile_jobs: List of tile metadata with PixelLab tile IDs
        output_path: Path to save manifest JSON
    """
    manifest = {
        "tiles": tile_jobs,
        "pixellab_params": PIXELLAB_PARAMS,
        "tileset_name": "venoxarid_organic_full",
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"Manifest saved to: {output_path}")


def main():
    """Main entry point for batch generation."""
    # Create tile batch
    tile_jobs = create_tile_batch()

    # Save manifest (without tile_ids yet - Claude will fill these in)
    manifest_path = Path(__file__).parent / "venoxarid_manifest_full.json"
    save_manifest(tile_jobs, manifest_path)

    print("\n" + "=" * 80)
    print("NEXT STEPS:")
    print("=" * 80)
    print("1. Claude Code will call mcp__pixellab__create_isometric_tile for each tile")
    print("2. Poll mcp__pixellab__get_isometric_tile until all complete")
    print("3. Update manifest with tile_ids")
    print("4. Run pixellab_processor.py to create sprite sheet:")
    print("   uv run python pixellab_processor.py \\")
    print("     --manifest venoxarid_manifest_full.json \\")
    print("     --output-sheet ../../assets/maps/venoxarid_tileset_full.png \\")
    print("     --output-json tiles_venoxarid_full.json \\")
    print("     --columns 7 \\")
    print("     --spacing 4 \\")
    print("     --tile-size 64")
    print("=" * 80)


if __name__ == "__main__":
    main()
