#!/usr/bin/env python3
"""
Add objectives to a Tiled TMX map file.

Automatically places objectives in strategic locations across the map.
Uses Poisson disc sampling to ensure good distribution.
For CaptureOutpost objectives, also generates enemy spawns within the zone.
"""

import xml.etree.ElementTree as ET
import random
import math
import argparse
from pathlib import Path


def distance(p1, p2):
    """Calculate Euclidean distance between two points."""
    return math.sqrt((p1[0] - p2[0]) ** 2 + (p1[1] - p2[1]) ** 2)


def tile_to_world(tile_x, tile_y, width, height, tile_width, tile_height):
    """Convert tile coordinates to isometric world coordinates."""
    world_x = (tile_x - tile_y) * tile_width / 2.0
    world_y = (tile_x + tile_y) * tile_height / 4.0

    # Center the map at origin
    center_tile_x = (width - 1) / 2.0
    center_tile_y = (height - 1) / 2.0
    center_world_x = (center_tile_x - center_tile_y) * tile_width / 2.0
    center_world_y = (center_tile_x + center_tile_y) * tile_height / 4.0

    return world_x - center_world_x, world_y - center_world_y


def generate_objective_points(
    width, height, tile_width, tile_height, num_objectives, min_distance, edge_margin=4
):
    """
    Generate objective points using Poisson disc sampling.

    Args:
        width: Map width in tiles
        height: Map height in tiles
        tile_width: Tile width in pixels
        tile_height: Tile height in pixels
        num_objectives: Number of objectives to generate
        min_distance: Minimum distance between objectives in world units
        edge_margin: Number of tiles to avoid from map edges

    Returns:
        List of (x, y) objective positions in world coordinates
    """
    points = []
    attempts = 0
    max_attempts = num_objectives * 100  # Avoid infinite loop

    min_tile = edge_margin
    max_tile_x = width - edge_margin - 1
    max_tile_y = height - edge_margin - 1

    if min_tile >= max_tile_x or min_tile >= max_tile_y:
        raise ValueError(
            f"Edge margin {edge_margin} too large for map {width}x{height}"
        )

    while len(points) < num_objectives and attempts < max_attempts:
        tile_x = random.randint(min_tile, max_tile_x)
        tile_y = random.randint(min_tile, max_tile_y)

        world_x, world_y = tile_to_world(
            tile_x, tile_y, width, height, tile_width, tile_height
        )

        # Check minimum distance from existing points
        too_close = False
        for existing_x, existing_y in points:
            if distance((world_x, world_y), (existing_x, existing_y)) < min_distance:
                too_close = True
                break

        if not too_close:
            points.append((world_x, world_y))

        attempts += 1

    return points


def generate_enemies_in_zone(
    zone_x, zone_y, radius, num_enemies, width, height, tile_width, tile_height
):
    """
    Generate enemy spawn points within an objective zone.

    Args:
        zone_x, zone_y: Center of the zone in world coordinates
        radius: Zone radius
        num_enemies: Number of enemies to place
        width, height: Map dimensions in tiles
        tile_width, tile_height: Tile dimensions

    Returns:
        List of (x, y, enemy_type) tuples
    """
    enemies = []
    attempts = 0
    max_attempts = num_enemies * 50
    min_spacing = 30.0  # Minimum distance between enemies

    while len(enemies) < num_enemies and attempts < max_attempts:
        # Random position within zone radius
        angle = random.uniform(0, 2 * math.pi)
        dist = random.uniform(0, radius * 0.8)  # Stay within 80% of radius

        x = zone_x + dist * math.cos(angle)
        y = zone_y + dist * math.sin(angle)

        # Check minimum distance from existing enemies
        too_close = False
        for ex, ey, _ in enemies:
            if distance((x, y), (ex, ey)) < min_spacing:
                too_close = True
                break

        if not too_close:
            # Random enemy type (weighted toward goblins/skeletons for outposts)
            roll = random.random()
            if roll < 0.4:
                enemy_type = "goblin"
            elif roll < 0.8:
                enemy_type = "skeleton"
            else:
                enemy_type = "slime"

            enemies.append((x, y, enemy_type))

        attempts += 1

    return enemies


def add_objectives_to_tmx(
    tmx_path,
    scrapyard_count,
    outpost_count,
    scrapyard_radius=50.0,
    outpost_radius=100.0,
    enemies_per_outpost=3,
    min_distance=200.0,
    edge_margin=4,
):
    """
    Add objectives and associated enemy spawns to TMX file.

    Args:
        tmx_path: Path to TMX file
        scrapyard_count: Number of AlienScrapyard objectives
        outpost_count: Number of CaptureOutpost objectives
        scrapyard_radius: Radius for scrapyard interaction
        outpost_radius: Radius for outpost zone
        enemies_per_outpost: Number of enemies to spawn per outpost
        min_distance: Minimum distance between objectives
        edge_margin: Tiles to avoid from edges
    """
    # Parse with xml.etree but preserve formatting better
    tree = ET.parse(tmx_path)
    root = tree.getroot()

    width = int(root.get("width"))
    height = int(root.get("height"))
    tile_width = int(root.get("tilewidth"))
    tile_height = int(root.get("tileheight"))

    print(f"Map dimensions: {width}x{height} tiles ({tile_width}x{tile_height}px)")

    # Remove existing Objectives layer if present (but keep other layers intact)
    objectives_layer = None
    for layer in root.findall("objectgroup"):
        if layer.get("name") == "Objectives":
            objectives_layer = layer
            break

    if objectives_layer is not None:
        root.remove(objectives_layer)
        print("Removed existing Objectives layer")

    # Generate objective positions
    total_objectives = scrapyard_count + outpost_count
    points = generate_objective_points(
        width,
        height,
        tile_width,
        tile_height,
        total_objectives,
        min_distance,
        edge_margin,
    )

    if len(points) < total_objectives:
        print(f"Warning: Only generated {len(points)} of {total_objectives} objectives")

    # Assign types to positions
    objectives = []
    for i, (x, y) in enumerate(points):
        if i < scrapyard_count:
            obj_type = "alien_scrapyard"
            radius = scrapyard_radius
            enemies = 0
            description = "Salvage alien scrap materials"
        else:
            obj_type = "capture_outpost"
            radius = outpost_radius
            enemies = enemies_per_outpost
            description = "Clear the enemy outpost"

        objectives.append(
            {
                "type": obj_type,
                "x": x,
                "y": y,
                "radius": radius,
                "enemies_required": enemies,
                "description": description,
            }
        )

    # Create Objectives object layer
    obj_layer = ET.SubElement(root, "objectgroup")
    next_layer_id = len(root.findall("layer")) + len(root.findall("objectgroup")) + 1
    obj_layer.set("id", str(next_layer_id))
    obj_layer.set("name", "Objectives")

    # Add objective objects
    all_outpost_enemies = []
    for idx, obj in enumerate(objectives):
        obj_elem = ET.SubElement(obj_layer, "object")
        obj_elem.set("id", str(idx + 1))

        if obj["type"] == "alien_scrapyard":
            obj_elem.set("name", f"Scrapyard_{idx + 1:02d}")
        else:
            obj_elem.set("name", f"Outpost_{idx + 1:02d}")

        obj_elem.set("x", str(obj["x"]))
        obj_elem.set("y", str(obj["y"]))

        # Add point marker
        ET.SubElement(obj_elem, "point")

        # Add properties
        properties = ET.SubElement(obj_elem, "properties")

        prop_type = ET.SubElement(properties, "property")
        prop_type.set("name", "objective_type")
        prop_type.set("type", "string")
        prop_type.set("value", obj["type"])

        prop_desc = ET.SubElement(properties, "property")
        prop_desc.set("name", "description")
        prop_desc.set("type", "string")
        prop_desc.set("value", obj["description"])

        prop_radius = ET.SubElement(properties, "property")
        prop_radius.set("name", "radius")
        prop_radius.set("type", "float")
        prop_radius.set("value", str(obj["radius"]))

        if obj["type"] == "alien_scrapyard":
            prop_time = ET.SubElement(properties, "property")
            prop_time.set("name", "interaction_time")
            prop_time.set("type", "float")
            prop_time.set("value", "3.0")
        else:
            prop_enemies = ET.SubElement(properties, "property")
            prop_enemies.set("name", "enemies_required")
            prop_enemies.set("type", "int")
            prop_enemies.set("value", str(obj["enemies_required"]))

            # Generate enemies for this outpost
            outpost_enemies = generate_enemies_in_zone(
                obj["x"],
                obj["y"],
                obj["radius"],
                obj["enemies_required"],
                width,
                height,
                tile_width,
                tile_height,
            )
            all_outpost_enemies.extend(outpost_enemies)

    # Add outpost enemies to EnemySpawns layer
    if all_outpost_enemies:
        # Find or create EnemySpawns layer
        enemy_layer = None
        for layer in root.findall("objectgroup"):
            if layer.get("name") in ("Spawns", "EnemySpawns"):
                enemy_layer = layer
                break

        if enemy_layer is None:
            enemy_layer = ET.SubElement(root, "objectgroup")
            enemy_layer.set("id", str(next_layer_id + 1))
            enemy_layer.set("name", "EnemySpawns")

        # Find the highest existing object ID
        max_id = 0
        for obj_elem in enemy_layer.findall("object"):
            obj_id = int(obj_elem.get("id", 0))
            if obj_id > max_id:
                max_id = obj_id

        # Add outpost enemies
        for idx, (x, y, enemy_type) in enumerate(all_outpost_enemies):
            obj_elem = ET.SubElement(enemy_layer, "object")
            obj_elem.set("id", str(max_id + idx + 1))
            obj_elem.set("name", f"Outpost_Spawn_{enemy_type}_{idx + 1:02d}")
            obj_elem.set("x", str(x))
            obj_elem.set("y", str(y))

            ET.SubElement(obj_elem, "point")

            properties = ET.SubElement(obj_elem, "properties")
            prop = ET.SubElement(properties, "property")
            prop.set("name", "enemy_type")
            prop.set("type", "string")
            prop.set("value", enemy_type)

        print(f"Added {len(all_outpost_enemies)} outpost enemy spawns")

    # Pretty print XML
    indent_xml(root)
    tree.write(tmx_path, encoding="utf-8", xml_declaration=True)

    print(f"\nAdded {len(objectives)} objectives to {tmx_path}:")
    scrapyards = sum(1 for o in objectives if o["type"] == "alien_scrapyard")
    outposts = sum(1 for o in objectives if o["type"] == "capture_outpost")
    print(f"  - {scrapyards} Alien Scrapyards")
    print(f"  - {outposts} Capture Outposts")


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
    parser = argparse.ArgumentParser(description="Add objectives to TMX map")
    parser.add_argument("--input", required=True, help="Input TMX file")
    parser.add_argument(
        "--scrapyards",
        type=int,
        default=2,
        help="Number of AlienScrapyard objectives (default: 2)",
    )
    parser.add_argument(
        "--outposts",
        type=int,
        default=1,
        help="Number of CaptureOutpost objectives (default: 1)",
    )
    parser.add_argument(
        "--scrapyard-radius",
        type=float,
        default=50.0,
        help="Interaction radius for scrapyards (default: 50)",
    )
    parser.add_argument(
        "--outpost-radius",
        type=float,
        default=100.0,
        help="Zone radius for outposts (default: 100)",
    )
    parser.add_argument(
        "--enemies-per-outpost",
        type=int,
        default=3,
        help="Enemies to spawn per outpost (default: 3)",
    )
    parser.add_argument(
        "--min-distance",
        type=float,
        default=200.0,
        help="Minimum distance between objectives (default: 200)",
    )
    parser.add_argument(
        "--edge-margin",
        type=int,
        default=4,
        help="Tiles to avoid from map edges (default: 4)",
    )
    parser.add_argument(
        "--seed", type=int, default=None, help="Random seed for reproducibility"
    )

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
        print(f"Using random seed: {args.seed}")

    add_objectives_to_tmx(
        args.input,
        args.scrapyards,
        args.outposts,
        args.scrapyard_radius,
        args.outpost_radius,
        args.enemies_per_outpost,
        args.min_distance,
        args.edge_margin,
    )


if __name__ == "__main__":
    main()
