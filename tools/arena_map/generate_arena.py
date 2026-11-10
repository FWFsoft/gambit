#!/usr/bin/env python3
"""Generates the prototype arena: assets/maps/arena.tmx + arena_tiles.png.

Layout (world space is screen space, y down, ship at the origin):

                 north: Frigidariant (ice)
    west: Cerebrian (psychic)   SHIP   east: Magnarok (fire)
                 south: Venoxarid (poison)

The quadrants match Arena::genusForDirection in src/ArenaData.cpp, so the
ground under your feet tells you which genus spawns there. Tiles are flat,
deliberately low-contrast diamonds: characters, enemies and attack telegraphs
have to read clearly on top of them (design pillar: Clarity).

The map is a normal Tiled file. Open it in Tiled to move objectives around;
re-run this script only if you want a different size or layout.

Usage:
  python3 tools/arena_map/generate_arena.py [--size 120] [--seed 11]
"""

import argparse
import math
import random
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
MAPS = REPO / "assets" / "maps"

TILE = 64          # cell size in the tileset (diamond is 64x32, centred)
VARIANTS = 4       # per biome: base, base alt, detail A, detail B
CLEARING_RADIUS = 340.0

# name, base colour, alt colour, detail colour A, detail colour B
BIOMES = [
    ("clearing", (108, 97, 82), (101, 91, 78), (134, 122, 103), (86, 78, 67)),
    ("venoxarid", (50, 82, 58), (46, 75, 54), (188, 92, 148), (146, 186, 84)),
    ("cerebrian", (72, 61, 94), (66, 56, 88), (150, 84, 172), (196, 152, 218)),
    ("frigidariant", (124, 146, 166), (116, 138, 159), (186, 212, 232),
     (222, 240, 255)),
    ("magnarok", (64, 53, 52), (58, 48, 48), (214, 108, 42), (246, 178, 62)),
]


def diamond_mask() -> np.ndarray:
    ys, xs = np.mgrid[0:TILE, 0:TILE]
    cx = cy = (TILE - 1) / 2.0
    # Slightly oversized so neighbouring tiles overlap instead of seaming.
    return (np.abs(xs - cx) / (TILE / 2.0) +
            np.abs(ys - cy) / (TILE / 4.0)) <= 1.07


def draw_tile(rng: random.Random, biome, variant: int) -> Image.Image:
    _, base, alt, detail_a, detail_b = biome
    mask = diamond_mask()
    colour = np.array(base if variant % 2 == 0 else alt, dtype=np.float32)
    img = np.zeros((TILE, TILE, 4), dtype=np.float32)
    img[..., :3] = colour

    # Chunky 2x2 noise keeps it pixel-art, low amplitude keeps it quiet.
    noise = np.array([[rng.uniform(-5, 5) for _ in range(TILE // 2)]
                      for _ in range(TILE // 2)], dtype=np.float32)
    img[..., :3] += np.kron(noise, np.ones((2, 2)))[..., None]

    # A whisper of an edge on the lower sides so the grid reads as ground.
    ys, xs = np.mgrid[0:TILE, 0:TILE]
    cx = cy = (TILE - 1) / 2.0
    d = np.abs(xs - cx) / (TILE / 2.0) + np.abs(ys - cy) / (TILE / 4.0)
    img[(d > 0.93) & (ys > cy), :3] *= 0.93

    def speck(colour_rgb, count, size):
        for _ in range(count):
            # Keep details well inside the diamond.
            u = rng.uniform(-0.6, 0.6)
            v = rng.uniform(-0.6, 0.6) * (1 - abs(u))
            x = int(cx + u * TILE / 2)
            y = int(cy + v * TILE / 4)
            img[y:y + size, x:x + size, :3] = colour_rgb

    if variant == 2:
        speck(detail_a, rng.randint(2, 4), 2)
    elif variant == 3:
        speck(detail_b, rng.randint(1, 3), 2)
        speck(detail_a, 1, 2)

    img[..., 3] = np.where(mask, 255, 0)
    img[~mask, :3] = 0
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def build_tileset(seed: int) -> Image.Image:
    rng = random.Random(seed)
    sheet = Image.new("RGBA", (TILE * VARIANTS, TILE * len(BIOMES)), (0, 0, 0, 0))
    for row, biome in enumerate(BIOMES):
        for variant in range(VARIANTS):
            sheet.paste(draw_tile(rng, biome, variant), (variant * TILE, row * TILE))
    return sheet


def smooth_noise(rng: random.Random, size: int, cells: int) -> np.ndarray:
    """Bilinear value noise in [-1, 1] on a size x size grid."""
    grid = np.array([[rng.uniform(-1, 1) for _ in range(cells + 2)]
                     for _ in range(cells + 2)], dtype=np.float32)
    coords = np.linspace(0, cells, size, endpoint=False)
    i = coords.astype(int)
    f = coords - i
    top = grid[i][:, i] * (1 - f)[None, :] + grid[i][:, i + 1] * f[None, :]
    bottom = grid[i + 1][:, i] * (1 - f)[None, :] + grid[i + 1][:, i + 1] * f[None, :]
    return top * (1 - f)[:, None] + bottom * f[:, None]


def biome_at(wx: float, wy: float, warp_x: float, warp_y: float) -> int:
    if math.hypot(wx, wy) < CLEARING_RADIUS + warp_x * 40.0:
        return 0
    dx = wx + warp_x * 170.0
    dy = wy + warp_y * 170.0
    # Must stay in step with Arena::genusForDirection.
    if abs(dx) >= abs(dy):
        return 4 if dx >= 0 else 2   # east Magnarok, west Cerebrian
    return 1 if dy >= 0 else 3       # south Venoxarid, north Frigidariant


def point_object(obj_id, name, wx, wy, centre_y, props=None) -> str:
    out = [f'    <object id="{obj_id}" name="{name}" x="{wx:.1f}" '
           f'y="{wy + centre_y:.1f}">', "      <point />"]
    if props:
        out.append("      <properties>")
        for key, kind, value in props:
            out.append(f'        <property name="{key}" type="{kind}" '
                       f'value="{value}" />')
        out.append("      </properties>")
    out.append("    </object>")
    return "\n".join(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--size", type=int, default=120, help="map size in tiles")
    ap.add_argument("--seed", type=int, default=11)
    args = ap.parse_args()
    size = args.size
    rng = random.Random(args.seed)

    build_tileset(args.seed).save(MAPS / "arena_tiles.png")

    # TiledMap centres the map: world = ((tx - ty) * 32, (tx + ty) * 16) - centre
    centre_y = (size - 1) * (TILE / 4.0) * 2 / 2.0 * 1.0  # (cx + cy) * 16
    centre_y = ((size - 1) / 2.0 + (size - 1) / 2.0) * (TILE / 4.0)

    warp_a = smooth_noise(rng, size, 9)
    warp_b = smooth_noise(rng, size, 9)

    rows = []
    for ty in range(size):
        row = []
        for tx in range(size):
            wx = (tx - ty) * (TILE / 2.0)
            wy = (tx + ty) * (TILE / 4.0) - centre_y
            biome = biome_at(wx, wy, warp_a[ty][tx], warp_b[ty][tx])
            roll = rng.random()
            variant = 0 if roll < 0.52 else 1 if roll < 0.86 else 2 if roll < 0.96 else 3
            row.append(str(biome * VARIANTS + variant + 1))
        rows.append(",".join(row))
    csv = ",\n".join(rows)

    next_id = 1
    objects = {"Ship": [], "PlayerSpawns": [], "Objectives": []}

    objects["Ship"].append(point_object(next_id, "The Ship", 0, 0, centre_y))
    next_id += 1

    for i in range(4):
        angle = math.pi / 2 + i * math.pi / 2
        objects["PlayerSpawns"].append(point_object(
            next_id, f"Spawn_{i + 1}", math.cos(angle) * 150,
            math.sin(angle) * 110 + 40, centre_y))
        next_id += 1

    # Two scrapyards per genus: one reachable in round 1, one further out.
    half_w = (size - 1) * TILE / 2.0
    half_h = (size - 1) * TILE / 4.0
    genus_angles = {"Magnarok": 0.0, "Venoxarid": 90.0, "Cerebrian": 180.0,
                    "Frigidariant": 270.0}
    for genus, base_angle in genus_angles.items():
        for ring, (radius, spread) in enumerate([(760.0, -22.0), (1280.0, 20.0)]):
            angle = math.radians(base_angle + spread + rng.uniform(-6, 6))
            wx, wy = math.cos(angle) * radius, math.sin(angle) * radius
            # Stay inside the playable diamond.
            norm = abs(wx) / half_w + abs(wy) / half_h
            if norm > 0.9:
                wx, wy = wx * 0.9 / norm, wy * 0.9 / norm
            objects["Objectives"].append(point_object(
                next_id, f"Scrapyard_{genus}_{ring + 1}", wx, wy, centre_y, [
                    ("objective_type", "string", "alien_scrapyard"),
                    ("description", "string",
                     "Salvage scrap and haul it back to the ship"),
                    ("radius", "float", "70.0"),
                    ("interaction_time", "float", "3.0"),
                ]))
            next_id += 1

    # Two derelict turrets guarding the approaches to the ship.
    for i, angle_deg in enumerate((225.0, 45.0)):
        angle = math.radians(angle_deg)
        objects["Objectives"].append(point_object(
            next_id, f"Turret_{i + 1}", math.cos(angle) * 420,
            math.sin(angle) * 420, centre_y, [
                ("objective_type", "string", "derelict_turret"),
                ("description", "string", "Repair the derelict turret"),
                ("radius", "float", "70.0"),
                ("interaction_time", "float", "4.0"),
            ]))
        next_id += 1

    groups = []
    for layer_id, (name, items) in enumerate(objects.items(), start=2):
        groups.append(f'  <objectgroup id="{layer_id}" name="{name}">\n' +
                      "\n".join(items) + "\n  </objectgroup>")

    tmx = f'''<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.10.2" orientation="isometric" renderorder="right-down" width="{size}" height="{size}" tilewidth="{TILE}" tileheight="{TILE}" infinite="0" nextlayerid="{len(objects) + 2}" nextobjectid="{next_id}">
  <tileset firstgid="1" name="arena_tiles" tilewidth="{TILE}" tileheight="{TILE}" tilecount="{VARIANTS * len(BIOMES)}" columns="{VARIANTS}">
    <image source="arena_tiles.png" width="{TILE * VARIANTS}" height="{TILE * len(BIOMES)}" />
  </tileset>
  <layer id="1" name="Ground" width="{size}" height="{size}">
    <data encoding="csv">
{csv}
</data>
  </layer>
{chr(10).join(groups)}
</map>
'''
    (MAPS / "arena.tmx").write_text(tmx)
    print(f"wrote {MAPS / 'arena.tmx'} ({size}x{size} tiles, "
          f"{len(objects['Objectives'])} objectives) and arena_tiles.png")


if __name__ == "__main__":
    main()
