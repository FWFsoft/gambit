#!/usr/bin/env python3
"""Generates the prototype arena: assets/maps/arena.tmx + arena_tiles.png.

Layout (world space is screen space, y down, ship at the origin):

                 north: Frigidariant (ice)
    west: Cerebrian (psychic)   SHIP   east: Magnarok (fire)
                 south: Venoxarid (poison)

The quadrants match Arena::genusForDirection in src/ArenaData.cpp, so the
ground under your feet tells you which genus's infusions drop there.

Points of interest (POIs) are scattered in three rings around the ship:
each is an objective on a pale clearing that stands out of the dark ground,
most guarded by a camp of enemies (the EnemySpawns layer). The near ring is
quick and pays little; the far ring needs a later round's zone, bigger
camps and elites, and pays two infusions (Tuning::FAR_OBJECTIVE_DISTANCE).
Enemies only come from these camps and from raids on the ship, so where
the crew goes decides what it fights. Tiles are flat,
deliberately low-contrast diamonds: characters, enemies and attack telegraphs
have to read clearly on top of them (design pillar: Clarity).

The map is a normal Tiled file. Open it in Tiled to move objectives around;
re-run this script only if you want a different size or layout.

Usage:
  python3 tools/arena_map/generate_arena.py [--size 176] [--seed 11]
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


# kind, ring radius, angle (degrees, 0 = east, 90 = south, screen space).
# Spread so every quadrant has something near, something mid and something
# far, and no two neighbours are the same kind.
POIS = [
    # Near ring: round 1's first stops.
    ("scrapyard", 820, 20), ("medpacks", 900, 115), ("scrapyard", 860, 200),
    ("outpost", 950, 290),
    # Mid ring: still inside round 1's zone, a real trip out.
    ("littlejohn", 1500, 345), ("probe", 1550, 60), ("scrapyard", 1450, 140),
    ("frogs", 1600, 175), ("gas", 1500, 240), ("outpost_big", 1650, 320),
    ("medpacks", 1500, 95),
    # Far ring: rounds 2 and 3, bigger camps and double rewards.
    ("outpost_big", 2300, 10), ("gas", 2350, 75), ("outpost_big", 2400, 120),
    ("littlejohn", 2450, 160), ("probe", 2350, 215), ("medpacks_big", 2300, 265),
    ("scrapyard", 2500, 300), ("littlejohn", 2250, 40),
]

# Per kind: objective type, description, objective radius, interaction time,
# camp (grunts, elites), clearing radius.
POI_KINDS = {
    "scrapyard": ("alien_scrapyard", "Salvage scrap and haul it back to the ship",
                  70.0, 3.0, (2, 0), 150.0),
    "medpacks": ("salvage_medpacks", "Clear the camp, then open the med pod",
                 130.0, 1.0, (3, 0), 170.0),
    "medpacks_big": ("salvage_medpacks", "Clear the camp, then open the med pod",
                     150.0, 1.0, (4, 1), 190.0),
    "outpost": ("capture_outpost", "Wipe out the camp", 140.0, 1.0, (3, 0),
                180.0),
    "outpost_big": ("capture_outpost", "Wipe out the camp and its enforcer",
                    160.0, 1.0, (4, 1), 210.0),
    "probe": ("recover_probe", "Dig out the probe and carry it to the ship",
              70.0, 4.0, (2, 0), 150.0),
    "frogs": ("save_the_frogs", "Carry the frogs to the pond", 80.0, 1.5,
              (0, 0), 170.0),
    "gas": ("noxious_gas", "Kick the spores in the toxic cloud", 110.0, 4.0,
            (0, 0), 170.0),
    "littlejohn": ("little_john", "Open the gate its guardian is watching",
                   110.0, 1.0, (0, 1), 170.0),
}


def biome_at(wx: float, wy: float, warp_x: float, warp_y: float,
             clearings=()) -> int:
    if math.hypot(wx, wy) < CLEARING_RADIUS + warp_x * 40.0:
        return 0
    # A point of interest sits on a pale clearing that reads from afar.
    for cx, cy, radius in clearings:
        if math.hypot(wx - cx, wy - cy) < radius + warp_y * 30.0:
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
    ap.add_argument("--size", type=int, default=176, help="map size in tiles")
    ap.add_argument("--seed", type=int, default=11)
    args = ap.parse_args()
    size = args.size
    rng = random.Random(args.seed)

    build_tileset(args.seed).save(MAPS / "arena_tiles.png")

    # TiledMap centres the map: world = ((tx - ty) * 32, (tx + ty) * 16) - centre
    centre_y = (size - 1) * (TILE / 4.0) * 2 / 2.0 * 1.0  # (cx + cy) * 16
    centre_y = ((size - 1) / 2.0 + (size - 1) / 2.0) * (TILE / 4.0)

    half_w = (size - 1) * TILE / 2.0
    half_h = (size - 1) * TILE / 4.0

    def inside(wx, wy, margin=0.88):
        """Pull a point back inside the playable diamond."""
        norm = abs(wx) / half_w + abs(wy) / half_h
        if norm > margin:
            return wx * margin / norm, wy * margin / norm
        return wx, wy

    pois = []
    for kind, radius, angle_deg in POIS:
        angle = math.radians(angle_deg + rng.uniform(-5, 5))
        r = radius * rng.uniform(0.95, 1.05)
        wx, wy = inside(math.cos(angle) * r, math.sin(angle) * r)
        pois.append((kind, wx, wy))
    clearings = [(wx, wy, POI_KINDS[kind][5]) for kind, wx, wy in pois]

    warp_a = smooth_noise(rng, size, 9)
    warp_b = smooth_noise(rng, size, 9)

    rows = []
    for ty in range(size):
        row = []
        for tx in range(size):
            wx = (tx - ty) * (TILE / 2.0)
            wy = (tx + ty) * (TILE / 4.0) - centre_y
            biome = biome_at(wx, wy, warp_a[ty][tx], warp_b[ty][tx], clearings)
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

    objects["EnemySpawns"] = []
    counts = {}
    for kind, wx, wy in pois:
        (otype, desc, radius, itime, (grunts, elites),
         _clearing) = POI_KINDS[kind]
        counts[kind] = counts.get(kind, 0) + 1
        name = f"{kind.title().replace('_', '')}_{counts[kind]}"
        props = [("objective_type", "string", otype),
                 ("description", "string", desc),
                 ("radius", "float", f"{radius:.1f}"),
                 ("interaction_time", "float", f"{itime:.1f}")]
        camp = grunts + elites
        if otype in ("capture_outpost", "salvage_medpacks"):
            props.append(("enemies_required", "int", str(camp)))
        if otype == "save_the_frogs":
            # The pond is a short walk away, back toward the ship.
            d = math.hypot(wx, wy)
            px, py = wx - wx / d * 260.0, wy - wy / d * 260.0
            props += [("enemies_required", "int", "3"),
                      ("deposit_x", "float", f"{px:.1f}"),
                      ("deposit_y", "float", f"{py + centre_y:.1f}")]
        objects["Objectives"].append(point_object(
            next_id, name, wx, wy, centre_y, props))
        next_id += 1

        # The camp: guards stand inside the objective's radius (their kills
        # count for it), the elite in the middle.
        for i in range(camp):
            elite = i >= grunts
            if elite:
                gx, gy = wx, wy + (0.0 if grunts == 0 else 12.0)
            else:
                a = 2 * math.pi * i / max(1, grunts) + rng.uniform(-0.3, 0.3)
                ring = (radius * 0.6 if otype in ("capture_outpost",
                                                    "salvage_medpacks")
                        else radius + 40.0)
                gx, gy = wx + math.cos(a) * ring, wy + math.sin(a) * ring * 0.8
            camp_props = [("enemy_type", "string", "venoxarid_grunt")]
            if elite:
                camp_props.append(("elite", "bool", "true"))
            objects["EnemySpawns"].append(point_object(
                next_id, f"{name}_guard_{i + 1}", gx, gy, centre_y, camp_props))
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

    # The wreck is solid: one collision rectangle under the hull.
    hull_w, hull_h = 132.0, 46.0
    objects["Collision"] = [
        f'    <object id="{next_id}" name="ShipHull" type="wall" '
        f'x="{-hull_w / 2:.1f}" y="{-hull_h / 2 - 4 + centre_y:.1f}" '
        f'width="{hull_w:.1f}" height="{hull_h:.1f}" />'
    ]
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
          f"{len(objects['Objectives'])} objectives, "
          f"{len(objects['EnemySpawns'])} camp guards) and arena_tiles.png")


if __name__ == "__main__":
    main()
