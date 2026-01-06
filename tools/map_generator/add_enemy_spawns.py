#!/usr/bin/env python3
"""
Add enemy spawn points to a Tiled TMX map file.

Automatically places enemy spawns in strategic locations across the map.
Uses Poisson disc sampling to ensure good distribution.
"""

import xml.etree.ElementTree as ET
import random
import math
import argparse
from pathlib import Path


def distance(p1, p2):
    """Calculate Euclidean distance between two points."""
    return math.sqrt((p1[0] - p2[0])**2 + (p1[1] - p2[1])**2)


def generate_spawn_points(width, height, tile_width, tile_height, num_spawns, min_distance, edge_margin=3):
    """
    Generate spawn points using Poisson disc sampling.

    Args:
        width: Map width in tiles
        height: Map height in tiles
        tile_width: Tile width in pixels
        tile_height: Tile height in pixels (use tileHeight/4 for isometric)
        num_spawns: Number of spawns to generate
        min_distance: Minimum distance between spawns in world units
        edge_margin: Number of tiles to avoid from map edges (default: 3)

    Returns:
        List of (x, y) spawn positions in world coordinates
    """
    spawns = []
    attempts = 0
    max_attempts = num_spawns * 50  # Avoid infinite loop

    # Calculate valid tile range (stay within map bounds with margin)
    min_tile = edge_margin
    max_tile_x = width - edge_margin - 1
    max_tile_y = height - edge_margin - 1

    # Validate margin isn't too large for map
    if min_tile >= max_tile_x or min_tile >= max_tile_y:
        raise ValueError(f"Edge margin {edge_margin} too large for map {width}x{height}")

    # Convert tile coordinates to isometric world coordinates
    def tile_to_world(tile_x, tile_y):
        world_x = (tile_x - tile_y) * tile_width / 2.0
        world_y = (tile_x + tile_y) * tile_height / 4.0

        # Center the map at origin
        center_tile_x = (width - 1) / 2.0
        center_tile_y = (height - 1) / 2.0
        center_world_x = (center_tile_x - center_tile_y) * tile_width / 2.0
        center_world_y = (center_tile_x + center_tile_y) * tile_height / 4.0

        return world_x - center_world_x, world_y - center_world_y

    while len(spawns) < num_spawns and attempts < max_attempts:
        # Random INTEGER tile position (discrete grid positions only)
        # Use randint for exact tile centers, avoiding edges
        tile_x = random.randint(min_tile, max_tile_x)
        tile_y = random.randint(min_tile, max_tile_y)

        world_x, world_y = tile_to_world(tile_x, tile_y)

        # Check minimum distance from existing spawns
        too_close = False
        for existing_x, existing_y, _ in spawns:
            if distance((world_x, world_y), (existing_x, existing_y)) < min_distance:
                too_close = True
                break

        if not too_close:
            # Random enemy type weighted by rarity
            roll = random.random()
            if roll < 0.6:  # 60% slimes (common)
                enemy_type = "slime"
            elif roll < 0.85:  # 25% goblins (medium)
                enemy_type = "goblin"
            else:  # 15% skeletons (rare)
                enemy_type = "skeleton"

            spawns.append((world_x, world_y, enemy_type))

        attempts += 1

    return spawns


def add_spawns_to_tmx(tmx_path, spawns):
    """
    Add enemy spawn object layer to TMX file.

    Args:
        tmx_path: Path to TMX file
        spawns: List of (x, y, enemy_type) tuples
    """
    tree = ET.parse(tmx_path)
    root = tree.getroot()

    # Check if EnemySpawns layer already exists
    for layer in root.findall('objectgroup'):
        if layer.get('name') == 'EnemySpawns':
            root.remove(layer)
            print("Removed existing EnemySpawns layer")

    # Create object layer
    object_layer = ET.SubElement(root, 'objectgroup')
    object_layer.set('id', str(len(root.findall('layer')) + len(root.findall('objectgroup')) + 1))
    object_layer.set('name', 'EnemySpawns')

    # Add spawn points
    for idx, (x, y, enemy_type) in enumerate(spawns):
        obj = ET.SubElement(object_layer, 'object')
        obj.set('id', str(idx + 1))
        obj.set('name', f'Spawn_{enemy_type}_{idx+1:02d}')
        obj.set('x', str(x))
        obj.set('y', str(y))

        # Add point marker
        point = ET.SubElement(obj, 'point')

        # Add enemy_type property
        properties = ET.SubElement(obj, 'properties')
        prop = ET.SubElement(properties, 'property')
        prop.set('name', 'enemy_type')
        prop.set('type', 'string')
        prop.set('value', enemy_type)

    # Pretty print XML
    indent_xml(root)
    tree.write(tmx_path, encoding='utf-8', xml_declaration=True)

    print(f"Added {len(spawns)} enemy spawns to {tmx_path}")


def indent_xml(elem, level=0):
    """Add indentation to XML for pretty printing."""
    i = "\n" + level * "  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for child in elem:
            indent_xml(child, level + 1)
        if not child.tail or not child.tail.strip():
            child.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i


def main():
    parser = argparse.ArgumentParser(description='Add enemy spawns to TMX map')
    parser.add_argument('--input', required=True, help='Input TMX file')
    parser.add_argument('--slimes', type=int, default=10, help='Number of slime spawns')
    parser.add_argument('--goblins', type=int, default=6, help='Number of goblin spawns')
    parser.add_argument('--skeletons', type=int, default=3, help='Number of skeleton spawns')
    parser.add_argument('--min-distance', type=float, default=150.0,
                        help='Minimum distance between spawns (world units)')
    parser.add_argument('--edge-margin', type=int, default=3,
                        help='Number of tiles to avoid from map edges (default: 3)')

    args = parser.parse_args()

    # Parse TMX to get map dimensions
    tree = ET.parse(args.input)
    root = tree.getroot()

    width = int(root.get('width'))
    height = int(root.get('height'))
    tile_width = int(root.get('tilewidth'))
    tile_height = int(root.get('tileheight'))

    print(f"Map dimensions: {width}x{height} tiles ({tile_width}x{tile_height}px)")
    print(f"Valid spawn area: tiles [{args.edge_margin}, {width-args.edge_margin-1}] x [{args.edge_margin}, {height-args.edge_margin-1}]")

    total_spawns = args.slimes + args.goblins + args.skeletons
    print(f"Generating {total_spawns} spawns ({args.slimes} slimes, {args.goblins} goblins, {args.skeletons} skeletons)")

    # Generate spawn points
    spawns = generate_spawn_points(
        width, height, tile_width, tile_height,
        total_spawns, args.min_distance, args.edge_margin
    )

    # Assign enemy types based on requested counts
    enemy_types = (['slime'] * args.slimes +
                   ['goblin'] * args.goblins +
                   ['skeleton'] * args.skeletons)
    random.shuffle(enemy_types)

    # Update spawns with correct enemy types
    spawns = [(x, y, enemy_types[i]) for i, (x, y, _) in enumerate(spawns)]

    # Add to TMX
    add_spawns_to_tmx(args.input, spawns)

    # Print spawn distribution
    slime_count = sum(1 for _, _, t in spawns if t == 'slime')
    goblin_count = sum(1 for _, _, t in spawns if t == 'goblin')
    skeleton_count = sum(1 for _, _, t in spawns if t == 'skeleton')

    print(f"\nSpawn distribution:")
    print(f"  Slimes: {slime_count}")
    print(f"  Goblins: {goblin_count}")
    print(f"  Skeletons: {skeleton_count}")
    print(f"  Total: {len(spawns)}")


if __name__ == '__main__':
    main()
