#!/usr/bin/env python3
"""Generate the planet map: assets/maps/planet.tmx and its tilesets.

The planet replaces the flat, round prototype arena with a place: an island
of hills, mountain ranges, lakes and rivers around the crashed ship, with
the outskirts of a coalition city on one side and the city's skyline beyond
the shore. Read docs/design/MAP_PIPELINE.md first; the rules here follow it.

What it builds, in order:
  1. Land: an irregular island (fingers, a bay) in a dark sea.
  2. Height: rolling hills plus ridged mountain ranges; the crash basin
     around the ship is flat. Peaks are impassable and shape the routes.
  3. Water: lakes in the low ground (impassable), rivers running from the
     mountains to the sea (walkable shallows).
  4. Regions: the fixed zone template. A central basin, a ring of wild
     regions and the city. Each region is dealt a genus per match by the
     server, so where each genus lives is a discovery every run.
  5. The city: streets and blocks on the north-east shore.
  6. The crash: a scorched trench the ship ploughed on its way in.
  7. Landmarks: a few big, unique set pieces on sight lines.
  8. Points of interest: each objective kind goes where it makes sense
     (frogs by water, outposts on high ground, scrapyards in the crash
     field and the city), in three rings: near (quick), mid, far.
  9. Trails from the ship to every point of interest (one shortest-path
     tree, so trails merge into roads toward home).
 10. Lifeline rocks (Terrain layer) and set dressing (Props layer).

Everything is seeded: the same seed gives the same planet. The output is a
normal Tiled map. Hand edits in Tiled are fine, but re-running the generator
overwrites them (a "locked" override layer is planned, not built).

Usage:
  python3 tools/maps/generate_planet.py [--seed 3] [--size 360]
      [--preview docs/prototype/planet_overview.png]
"""

import argparse
import heapq
import math
import random
import re
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parents[2]
MAPS = REPO / "assets" / "maps"

TILE = 64
HALF_W, QUARTER_H = 32.0, 16.0

# Real-world scale. Must match Config::World (include/config/WorldScale.h):
# a hero is ~100 world px and ~1.8 m tall, so a metre is 55 world px, and a
# tile is ~0.65 m along an axis. Size anything a hero stands next to in
# metres (docs/design/MAP_PIPELINE.md, "Scale").
METRE = 55.0
STOREY = 3.5                                  # metres per building storey
TILE_EDGE_M = math.hypot(HALF_W, QUARTER_H) / METRE


def tiles(metres):
    """Whole tiles along an axis covering `metres`."""
    return max(1, round(metres / TILE_EDGE_M))


# Must match TerrainGrid (include/TerrainGrid.h).
BASE_LEVEL = 4
LEVELS = 16
LEVEL_PIXELS = 16.0

# Island radii in world (screen) pixels. The zone of round 3 (MAP_ZONE_SCALE
# times the arena's) reaches most of it; the far fingers stay past it.
ISLAND_RX = 7200.0
ISLAND_RY = 5300.0
ZONE_SCALE = 1.75          # written to the map; scales the storm's radii
ZONE_RADII = [2000.0 * ZONE_SCALE, 2500.0 * ZONE_SCALE, 2900.0 * ZONE_SCALE]
BASIN_RADIUS = 950.0       # flat ground around the ship
SHIP_CLEAR = 420.0
CITY_ANGLE = -32.0         # degrees, screen space (0 = east, -90 = north)

# ---------------------------------------------------------------------------
# Ground materials: one row of the tileset each, four variants per row.
# Painted dark and quiet (the look darkens them further); detail lives in
# seams and specks, never in bright texture (ART_DIRECTION.md, "World").
# ---------------------------------------------------------------------------
MATERIALS = [
    # name,   base,            alt,             seam,           speck
    ("dust",   (86, 80, 78),   (80, 75, 74),   (64, 60, 60),   (110, 102, 96)),
    ("scrub",  (52, 62, 60),   (48, 57, 56),   (36, 44, 44),   (78, 92, 84)),
    ("rock",   (70, 70, 82),   (64, 64, 76),   (44, 44, 54),   (96, 96, 110)),
    ("high",   (96, 98, 114),  (90, 92, 108),  (62, 64, 78),   (128, 130, 146)),
    ("wet",    (44, 54, 62),   (40, 50, 58),   (30, 38, 46),   (64, 84, 96)),
    ("path",   (104, 96, 88),  (98, 91, 84),   (78, 72, 66),   (126, 116, 104)),
    ("pad",    (112, 104, 94), (106, 99, 90),  (84, 78, 70),   (138, 128, 112)),
    ("scar",   (36, 30, 30),   (32, 27, 28),   (22, 18, 20),   (150, 70, 40)),
    ("street", (58, 58, 70),   (54, 54, 66),   (34, 34, 44),   (90, 90, 110)),
    ("river",  (40, 62, 78),   (38, 58, 74),   (30, 46, 62),   (80, 120, 140)),
    ("water",  (22, 34, 48),   (20, 31, 45),   (16, 25, 38),   (48, 72, 92)),
    ("sea",    (14, 20, 32),   (13, 19, 30),   (10, 15, 25),   (30, 44, 62)),
]
MAT = {name: i for i, (name, *_rest) in enumerate(MATERIALS)}
VARIANTS = 4

# Objective kinds (as in tools/arena_map/generate_arena.py): objective type,
# description, radius, interaction time, camp (grunts, elites), pad radius.
POI_KINDS = {
    "scrapyard": ("alien_scrapyard", "Salvage scrap and haul it back to the ship",
                  70.0, 3.0, (2, 0), 170.0),
    "medpacks": ("salvage_medpacks", "Clear the camp, then open the med pod",
                 130.0, 1.0, (3, 0), 190.0),
    "medpacks_big": ("salvage_medpacks", "Clear the camp, then open the med pod",
                     150.0, 1.0, (4, 1), 210.0),
    "outpost": ("capture_outpost", "Wipe out the camp", 140.0, 1.0, (3, 0),
                200.0),
    "outpost_big": ("capture_outpost", "Wipe out the camp and its enforcer",
                    160.0, 1.0, (4, 1), 230.0),
    "probe": ("recover_probe", "Dig out the probe and carry it to the ship",
              70.0, 4.0, (2, 0), 170.0),
    "frogs": ("save_the_frogs", "Carry the frogs to the pond", 80.0, 1.5,
              (0, 0), 190.0),
    "gas": ("noxious_gas", "Kick the spores in the toxic cloud", 110.0, 4.0,
            (0, 0), 190.0),
    "littlejohn": ("little_john", "Open the gate its guardian is watching",
                   110.0, 1.0, (0, 1), 190.0),
}

# What each ring asks for. Near: quick stops in round 1. Mid: a real trip.
# Far: rounds 2-3, bigger camps, two infusions; some sit out in the fingers
# past round 3's storm, worth a dash through it (Strategic Crunch).
RINGS = [
    ("near", 1150.0, 2500.0, ["scrapyard", "scrapyard", "medpacks", "outpost",
                              "outpost", "probe", "gas", "frogs"]),
    ("mid", 2500.0, 4000.0, ["outpost_big", "outpost_big", "littlejohn",
                             "littlejohn", "frogs", "gas", "gas", "probe",
                             "probe", "scrapyard", "scrapyard", "medpacks"]),
    ("far", 4000.0, 9000.0, ["outpost_big", "outpost_big", "outpost_big",
                             "littlejohn", "littlejohn", "probe",
                             "medpacks_big", "medpacks_big", "gas", "frogs"]),
]
POI_SPACING = 850.0

LANDMARKS = ["monolith", "ribcage", "crater", "freighter", "spire", "monolith"]
# Flat ground cleared around each landmark, and its solid core, in world px.
# Sized to the landmark's real footprint (ArenaView::drawLandmark).
LANDMARK_PAD = {"monolith": 260, "spire": 380, "ribcage": 720, "crater": 330,
                "freighter": 700}
LANDMARK_CORE = {"monolith": 120, "spire": 200, "ribcage": 70, "crater": 90,
                 "freighter": 420}


# ---------------------------------------------------------------------------
# Noise
# ---------------------------------------------------------------------------
class Noise:
    """Value noise on a random lattice, sampled at world coordinates."""

    def __init__(self, rng: random.Random, cells: int = 64):
        self.cells = cells
        self.grid = np.array([[rng.uniform(-1, 1) for _ in range(cells)]
                              for _ in range(cells)], dtype=np.float32)
        self.ox, self.oy = rng.uniform(0, cells), rng.uniform(0, cells)

    def __call__(self, x, y, scale):
        u = x / scale + self.ox
        v = y / scale + self.oy
        i0 = np.floor(u).astype(int)
        j0 = np.floor(v).astype(int)
        fu = u - i0
        fv = v - j0
        fu = fu * fu * (3 - 2 * fu)
        fv = fv * fv * (3 - 2 * fv)
        n = self.cells
        g = self.grid
        a = g[j0 % n, i0 % n]
        b = g[j0 % n, (i0 + 1) % n]
        c = g[(j0 + 1) % n, i0 % n]
        d = g[(j0 + 1) % n, (i0 + 1) % n]
        return (a * (1 - fu) + b * fu) * (1 - fv) + (c * (1 - fu) + d * fu) * fv

    def fbm(self, x, y, scale, octaves=4):
        total, amp, norm = 0.0, 1.0, 0.0
        for o in range(octaves):
            total = total + amp * self(x * (2 ** o) + o * 37.1,
                                       y * (2 ** o) - o * 11.7, scale)
            norm += amp
            amp *= 0.5
        return total / norm


# ---------------------------------------------------------------------------
# Grid helpers
# ---------------------------------------------------------------------------
class Grid:
    def __init__(self, size):
        self.size = size
        self.cy = (size - 1) * QUARTER_H  # world y of the map centre
        ty, tx = np.mgrid[0:size, 0:size]
        self.tx, self.ty = tx, ty
        self.wx = (tx - ty) * HALF_W
        self.wy = (tx + ty) * QUARTER_H - self.cy

    def to_tile(self, wx, wy):
        diff = wx / HALF_W
        total = (wy + self.cy + QUARTER_H) / QUARTER_H
        return int(math.floor((total + diff) / 2)), int(math.floor((total - diff) / 2))

    def centre(self, tx, ty):
        return (tx - ty) * HALF_W, (tx + ty) * QUARTER_H - self.cy

    def inside(self, tx, ty):
        return 0 <= tx < self.size and 0 <= ty < self.size

    def disc(self, wx, wy, radius):
        """Mask of tiles whose centre is within radius of (wx, wy)."""
        return np.hypot(self.wx - wx, self.wy - wy) <= radius


def neighbours8(size, tx, ty):
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dx or dy:
                nx, ny = tx + dx, ty + dy
                if 0 <= nx < size and 0 <= ny < size:
                    yield nx, ny, (1.4142 if dx and dy else 1.0)


# ---------------------------------------------------------------------------
# The planet
# ---------------------------------------------------------------------------
def build(seed: int, size: int):
    rng = random.Random(seed)
    g = Grid(size)
    n1, n2, n3, n4, n5 = (Noise(rng) for _ in range(5))
    wx, wy = g.wx, g.wy
    dist = np.hypot(wx, wy)
    ang = np.degrees(np.arctan2(wy, wx))

    # ---- 1. Land ---------------------------------------------------------
    e = np.hypot(wx / ISLAND_RX, wy / ISLAND_RY)
    e = e + 0.20 * n1.fbm(wx, wy, 2600, 4)
    # Fingers: lobes of land reaching out at a few angles.
    for a, reach, width in ((150, 0.22, 22), (60, 0.16, 18), (-120, 0.18, 20),
                            (-15, 0.10, 16)):
        d = (ang - a + 180) % 360 - 180
        e = e - reach * np.exp(-(d / width) ** 2) * np.clip(dist / 4000 - 0.3, 0, 1)
    # A bay biting in from the south-east, like the concept's inner sea.
    bay = np.hypot((wx - 3300) / 1500, (wy - 1500) / 900)
    e = e + 0.55 * np.clip(1.0 - bay, 0, 1)
    land = e < 1.0
    land &= dist > -1  # (keep shape)
    land[g.disc(0, 0, BASIN_RADIUS + 400)] = True

    # ---- 2. Height ---------------------------------------------------------
    inland = np.clip(1.0 - e, 0, 1)
    hills = n2.fbm(wx, wy, 1500, 4)
    ridge_n = n3.fbm(wx, wy, 2400, 4)
    ridge = np.clip(1.0 - np.abs(ridge_n) * 3.2, 0, 1) ** 2.2
    ridge *= np.clip((dist - 1600) / 1400, 0, 1)        # none near the ship
    ridge *= np.clip(inland * 3.0, 0, 1)                # none on the shore
    height = 3.0 + 4.0 * inland + 3.4 * hills + 10.5 * ridge
    # The crash basin: flat at the base level, easing into the hills.
    basin = np.clip((dist - BASIN_RADIUS) / 900.0, 0, 1)
    height = BASE_LEVEL + (height - BASE_LEVEL) * basin
    height[~land] = 1.0

    # ---- 3. Water: lakes in low inland ground ------------------------------
    lake_field = n4.fbm(wx, wy, 1300, 3)
    lakes = land & (lake_field > 0.34) & (dist > 1900) & (ridge < 0.2) & (inland > 0.12)
    height[lakes] = np.minimum(height[lakes], 2.4)

    # Terraces: the hills step in pairs of levels, so the land reads as
    # plateaus and ledges (a cliff face per step) rather than a smooth
    # blur. The mountains keep every level: rugged, not stepped.
    terraced = 2.0 * np.rint(height / 2.0)
    height = np.where((ridge < 0.25) & (basin > 0.5), terraced, height)
    level = np.clip(np.rint(height), 0, LEVELS - 1).astype(int)
    level[lakes] = 2
    level[~land] = 1

    # ---- 4. Regions (the zone template) -----------------------------------
    region = np.zeros((size, size), dtype=int)
    seeds = [(1, 0.0, 0.0, 600.0)]  # id, x, y, weight (bigger = smaller cell)
    ring = 7
    for i in range(ring):
        a = math.radians(CITY_ANGLE + 360.0 * (i + 1) / (ring + 1) + rng.uniform(-12, 12))
        seeds.append((i + 2, math.cos(a) * ISLAND_RX * 0.62,
                      math.sin(a) * ISLAND_RY * 0.62, 0.0))
    city_id = ring + 2
    ca = math.radians(CITY_ANGLE)
    city_x, city_y = math.cos(ca) * ISLAND_RX * 0.70, math.sin(ca) * ISLAND_RY * 0.70
    seeds.append((city_id, city_x, city_y, 0.0))
    warp_x = wx + 700 * n5.fbm(wx, wy, 1800, 3)
    warp_y = wy + 700 * n5.fbm(wx + 999, wy - 333, 1800, 3)
    best = np.full((size, size), np.inf)
    for rid, sx, sy, w in seeds:
        d = np.hypot(warp_x - sx, (warp_y - sy) * 1.2) + w
        if rid == 1:
            d = d + 1200.0  # the basin is a small hub, not a big region
        take = d < best
        best[take] = d[take]
        region[take] = rid
    region[~land] = 0

    # ---- 5. The city --------------------------------------------------------
    material = np.full((size, size), MAT["scrub"], dtype=int)
    blocked = np.zeros((size, size), dtype=bool)
    city = land & (region == city_id) & (inland > 0.04)
    city_level = 6
    level[city] = city_level
    ridge[city] = 0
    material[city] = MAT["street"]
    props = []
    city_blocks = []
    # A grid of 8 m streets (two lanes and kerbs) around 14 m blocks, at
    # real scale. Some blocks split into two narrower buildings across a 2 m
    # alley.
    street, block = tiles(8.0), tiles(14.0)
    pitch = street + block
    for by in range(0, size, pitch):
        for bx in range(0, size, pitch):
            x0, y0 = bx + street, by + street
            x1, y1 = bx + pitch, by + pitch
            if x1 > size or y1 > size:
                continue
            cell = city[y0:y1, x0:x1]
            if cell.size == 0 or not cell.all():
                continue
            if rng.random() < 0.18:
                continue  # an empty lot / plaza
            lots = [(x0, y0, x1, y1)]
            if rng.random() < 0.5:
                alley = tiles(2.0)
                if rng.random() < 0.5:
                    mid = (x0 + x1 - alley) // 2
                    lots = [(x0, y0, mid, y1), (mid + alley, y0, x1, y1)]
                else:
                    mid = (y0 + y1 - alley) // 2
                    lots = [(x0, y0, x1, mid), (x0, mid + alley, x1, y1)]
            for lx0, ly0, lx1, ly1 in lots:
                blocked[ly0:ly1, lx0:lx1] = True
                ccx, ccy = (lx0 + lx1) / 2.0, (ly0 + ly1) / 2.0
                cwx = (ccx - ccy) * HALF_W
                cwy = (ccx + ccy) * QUARTER_H - QUARTER_H - g.cy
                # Taller toward the city's heart (away from the island):
                # two storeys at the edge, up to five in the middle. Capped
                # there: buildings hide what walks behind them (the client
                # thins one with a hero behind it, but lower is clearer).
                heart = math.hypot(cwx - city_x * 1.25, cwy - city_y * 1.25)
                storeys = int(np.clip(round(5.0 - heart / 900.0 + rng.uniform(-1.0, 1.0)), 2, 5))
                tall = storeys * STOREY * METRE
                city_blocks.append((cwx, cwy, lx1 - lx0, ly1 - ly0, tall))
                props.append(("city_block", cwx, cwy, {"width": lx1 - lx0,
                                                      "depth": ly1 - ly0,
                                                      "tall": round(tall),
                                                      "variant": rng.randrange(4)}))

    # The skyline: the real city, out past the shore. Drawn, never reached.
    for i in range(26):
        a = math.radians(CITY_ANGLE + rng.uniform(-28, 28))
        r = rng.uniform(1.08, 1.45)
        sx, sy = math.cos(a) * ISLAND_RX * r, math.sin(a) * ISLAND_RY * r
        props.append(("skyline", sx, sy, {"tall": round(rng.uniform(500, 1500)),
                                          "width": round(rng.uniform(140, 380)),
                                          "variant": rng.randrange(4)}))

    # ---- 6. The crash trench ----------------------------------------------
    crash_angle = math.radians(155.0)
    ux, uy = math.cos(crash_angle), math.sin(crash_angle)
    along = wx * ux + wy * uy
    across = -wx * uy + wy * ux
    trench = (along > 150) & (along < 2100) & (np.abs(across) < 70 + along * 0.05)
    trench &= land
    material[trench] = MAT["scar"]
    level[trench] = np.maximum(level[trench] - 1, 2)

    # ---- Blocked: sea, lakes, peaks ----------------------------------------
    peaks = land & (level >= 13) & ~city
    blocked |= ~land | lakes | peaks

    # ---- Rivers: from the mountain flanks to the water --------------------
    river = np.zeros((size, size), dtype=bool)
    springs = np.argwhere(land & (level >= 10) & (level <= 12) & ~blocked)
    rng.shuffle(springs := [tuple(p) for p in springs])
    made = 0
    for sy_, sx_ in springs:
        if made >= 7:
            break
        wsx, wsy = g.centre(sx_, sy_)
        if math.hypot(wsx, wsy) < 2200:
            continue
        path, x, y = [], sx_, sy_
        seen = set()
        for _ in range(900):
            path.append((x, y))
            seen.add((x, y))
            if not land[y, x] or lakes[y, x]:
                break
            options = [(height[ny, nx] + rng.uniform(0, 0.35), nx, ny)
                       for nx, ny, _c in neighbours8(size, x, y)
                       if (nx, ny) not in seen]
            if not options:
                break
            _h, x, y = min(options)
        else:
            continue
        if len(path) < 40:
            continue
        for x, y in path:
            if land[y, x] and not city[y, x] and not lakes[y, x]:
                river[y, x] = True
                height[y, x] -= 0.6
        made += 1
    river &= ~blocked
    level[river] = np.maximum(level[river] - 1, 2)

    # ---- 7. Connectivity: carve passes so all big land reaches the ship ---
    def reachable_from_ship():
        sx, sy = g.to_tile(0, 60)
        seen = np.zeros((size, size), dtype=bool)
        stack = [(sx, sy)]
        seen[sy, sx] = True
        while stack:
            x, y = stack.pop()
            for nx, ny, _c in neighbours8(size, x, y):
                if not seen[ny, nx] and not blocked[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((nx, ny))
        return seen

    for _attempt in range(12):
        reach = reachable_from_ship()
        stranded = land & ~blocked & ~reach
        if stranded.sum() < 60:
            break
        # Walk from the reachable set through blocked land to the nearest
        # stranded tile, then open that walk as a pass.
        start = [tuple(p) for p in np.argwhere(reach)]
        prev = {}
        frontier = [(0.0, x, y) for y, x in start]
        heapq.heapify(frontier)
        best_cost = {(x, y): 0.0 for y, x in start}
        goal = None
        while frontier:
            c, x, y = heapq.heappop(frontier)
            if stranded[y, x]:
                goal = (x, y)
                break
            if c > best_cost.get((x, y), 1e18):
                continue
            for nx, ny, step in neighbours8(size, x, y):
                if not land[ny, nx] or lakes[ny, nx] or city_blocked(blocked, city, nx, ny):
                    continue
                nc = c + step
                if nc < best_cost.get((nx, ny), 1e18):
                    best_cost[(nx, ny)] = nc
                    prev[(nx, ny)] = (x, y)
                    heapq.heappush(frontier, (nc, nx, ny))
        if goal is None:
            break
        node = goal
        while node in prev:
            x, y = node
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    px, py = x + dx, y + dy
                    if g.inside(px, py) and land[py, px] and not lakes[py, px] \
                            and not city[py, px]:
                        blocked[py, px] = False
                        level[py, px] = min(level[py, px], 11)
                        peaks[py, px] = False
            node = prev[node]
    reach = reachable_from_ship()
    # Whatever still cannot be reached is filled in: nobody gets stuck there.
    pockets = land & ~blocked & ~reach
    blocked |= pockets

    # ---- Materials ---------------------------------------------------------
    slope = np.zeros_like(height)
    slope[1:-1, 1:-1] = (np.abs(height[1:-1, 2:] - height[1:-1, :-2]) +
                         np.abs(height[2:, 1:-1] - height[:-2, 1:-1]))
    scrubby = n1.fbm(wx + 500, wy - 700, 900, 3) > -0.05
    wild = land & ~city & ~trench
    material[wild] = np.where(scrubby[wild], MAT["scrub"], MAT["dust"])
    material[wild & (slope > 1.4)] = MAT["rock"]
    material[wild & (level >= 11)] = MAT["high"]
    near_water = np.zeros_like(land)
    water = (~land) | lakes | river
    for dy in (-2, -1, 0, 1, 2):
        for dx in (-2, -1, 0, 1, 2):
            near_water |= np.roll(np.roll(water, dy, 0), dx, 1)
    material[wild & near_water & (level <= 6)] = MAT["wet"]
    material[river] = MAT["river"]
    material[lakes] = MAT["water"]
    material[~land] = MAT["sea"]
    basin_dust = g.disc(0, 0, BASIN_RADIUS * 0.8) & land & ~trench
    material[basin_dust] = MAT["dust"]

    world = dict(g=g, rng=rng, land=land, level=level, region=region,
                 blocked=blocked, material=material, city=city, lakes=lakes,
                 river=river, trench=trench, peaks=peaks, ridge=ridge,
                 near_water=near_water, props=props, city_id=city_id,
                 city_blocks=city_blocks, slope=slope, crash=(ux, uy))
    place_landmarks(world)
    place_pois(world)
    cut_trails(world)
    place_rocks(world)
    dress(world)
    return world


def city_blocked(blocked, city, x, y):
    return city[y, x] and blocked[y, x]


def walkable_spot(w, x, y, radius):
    g = w["g"]
    tx, ty = g.to_tile(x, y)
    if not g.inside(tx, ty):
        return False
    r_tiles = int(radius / 28) + 1
    for dy in range(-r_tiles, r_tiles + 1):
        for dx in range(-r_tiles, r_tiles + 1):
            px, py = tx + dx, ty + dy
            if not g.inside(px, py):
                return False
            cx, cy = g.centre(px, py)
            if math.hypot(cx - x, cy - y) > radius:
                continue
            if w["blocked"][py, px]:
                return False
    return True


def flatten(w, x, y, radius, material=None, keep_level=None):
    """Level the ground around (x, y): a pad for a set piece or a camp."""
    g = w["g"]
    mask = g.disc(x, y, radius) & w["land"] & ~w["blocked"]
    if not mask.any():
        return
    tx, ty = g.to_tile(x, y)
    lvl = keep_level if keep_level is not None else int(w["level"][ty, tx])
    edge = g.disc(x, y, radius + 90) & w["land"] & ~w["blocked"] & ~mask
    w["level"][mask] = lvl
    # Soften the rim halfway so the pad does not stand on a plinth.
    w["level"][edge] = np.rint((w["level"][edge] + lvl) / 2).astype(int)
    if material is not None:
        w["material"][mask & ~w["river"]] = material


def place_landmarks(w):
    g, rng = w["g"], w["rng"]
    w["landmarks"] = []
    tries = 0
    want = list(LANDMARKS)
    while want and tries < 5000:
        tries += 1
        a = rng.uniform(-math.pi, math.pi)
        r = rng.uniform(1800, 5200)
        x, y = math.cos(a) * r * 1.2, math.sin(a) * r * 0.85
        tx, ty = g.to_tile(x, y)
        if not g.inside(tx, ty) or w["city"][ty, tx] or w["trench"][ty, tx]:
            continue
        kind = want[0]
        if not walkable_spot(w, x, y, LANDMARK_PAD[kind] + 20):
            continue
        if any(math.hypot(x - lx, y - ly) < 2200 for _k, lx, ly in w["landmarks"]):
            continue
        lvl = int(w["level"][ty, tx])
        if kind in ("monolith", "spire") and lvl < 7:
            if tries < 3000:
                continue
        if kind == "crater" and lvl > 7:
            if tries < 3000:
                continue
        want.pop(0)
        w["landmarks"].append((kind, x, y))
        flatten(w, x, y, LANDMARK_PAD[kind], material=MAT["dust"])
        if kind == "crater":
            ring = g.disc(x, y, 330) & ~g.disc(x, y, 230) & ~w["blocked"]
            w["level"][ring] = np.minimum(w["level"][ring] + 2, 13)
            w["material"][g.disc(x, y, 230) & ~w["blocked"]] = MAT["scar"]
        # A small solid core: you walk around a landmark, not through it.
        core = g.disc(x, y, LANDMARK_CORE[kind])
        w["blocked"] |= core & w["land"]
        w["props"].append((kind, x, y, {"variant": rng.randrange(4)}))


def place_pois(w):
    g, rng = w["g"], w["rng"]
    pois = []
    level, near_water = w["level"], w["near_water"]

    def context_score(kind, x, y):
        tx, ty = g.to_tile(x, y)
        lvl = level[ty, tx]
        s = rng.uniform(0, 1.0)
        in_city = w["region"][ty, tx] == w["city_id"]
        if kind in ("frogs", "gas"):
            s += 3.0 if near_water[ty, tx] else 0.0
            s += 1.0 if lvl <= 5 else 0.0
        if kind.startswith("outpost"):
            s += 0.35 * lvl
        if kind.startswith("scrapyard"):
            s += 3.0 if (w["trench"][ty, tx] or in_city) else 0.0
            ux, uy = w["crash"]
            if x * ux + y * uy > 0 and abs(-x * uy + y * ux) < 500:
                s += 2.0
        if kind.startswith("medpacks"):
            s += 2.5 if in_city else 0.0
        if kind == "probe":
            s += 0.25 * lvl + (1.5 if w["ridge"][ty, tx] > 0.1 else 0.0)
        if kind == "littlejohn":
            for _k, lx, ly in w["landmarks"]:
                if math.hypot(lx - x, ly - y) < 900:
                    s += 3.0
        return s

    for ring, r0, r1, kinds in RINGS:
        for kind in kinds:
            pad = POI_KINDS[kind][5]
            best = None
            for _ in range(500):
                a = rng.uniform(-math.pi, math.pi)
                r = rng.uniform(r0, r1)
                x, y = math.cos(a) * r * 1.25, math.sin(a) * r * 0.9
                d = math.hypot(x, y)
                if d < r0 or d > r1:
                    continue
                if not walkable_spot(w, x, y, pad + 40):
                    continue
                if any(math.hypot(x - px, y - py) < POI_SPACING for _k, px, py, _r in pois):
                    continue
                if any(math.hypot(x - lx, y - ly) < 380 for _k, lx, ly in w["landmarks"]):
                    continue
                tx, ty = g.to_tile(x, y)
                if w["city"][ty, tx] and kind in ("frogs", "gas", "probe"):
                    continue
                s = context_score(kind, x, y)
                if best is None or s > best[0]:
                    best = (s, x, y)
            if best is None:
                continue
            _s, x, y = best
            pois.append((kind, x, y, ring))
            flatten(w, x, y, pad, material=MAT["pad"])
    w["pois"] = pois


def cut_trails(w):
    """One shortest-path tree from the ship: every trail runs home."""
    g = w["g"]
    size = g.size
    level, blocked = w["level"], w["blocked"]
    sx, sy = g.to_tile(0, 60)
    cost = {(sx, sy): 0.0}
    prev = {}
    heap = [(0.0, sx, sy)]
    while heap:
        c, x, y = heapq.heappop(heap)
        if c > cost.get((x, y), 1e18):
            continue
        for nx, ny, step in neighbours8(size, x, y):
            if blocked[ny, nx]:
                continue
            climb = abs(int(level[ny, nx]) - int(level[y, x]))
            # Trails prefer flat ground, reuse each other (cheap on path),
            # and avoid the rivers' wet feet when they can.
            k = step * (1.0 + 1.6 * climb * climb)
            if w["material"][ny, nx] == MAT["path"]:
                k *= 0.6
            if w["river"][ny, nx]:
                k *= 1.6
            nc = c + k
            if nc < cost.get((nx, ny), 1e18):
                cost[(nx, ny)] = nc
                prev[(nx, ny)] = (x, y)
                heapq.heappush(heap, (nc, nx, ny))
    trail = np.zeros((size, size), dtype=bool)
    for _kind, x, y, _ring in w["pois"]:
        node = g.to_tile(x, y)
        while node in prev:
            trail[node[1], node[0]] = True
            node = prev[node]
    for kind, x, y in w["landmarks"]:
        node = g.to_tile(x + 300, y + 120)
        while node in prev:
            trail[node[1], node[0]] = True
            node = prev[node]
    # Widen by one tile, leave pads, city streets and water alone.
    wide = trail.copy()
    wide[:, 1:] |= trail[:, :-1]
    wide[1:, :] |= trail[:-1, :]
    keep = (w["material"] == MAT["pad"]) | w["city"] | w["river"] | \
        (w["material"] == MAT["scar"]) | blocked
    w["material"][wide & ~keep] = MAT["path"]
    w["trail"] = wide


def place_rocks(w):
    """Lifeline rocks (Terrain layer, type "terrain"): the place_terrain.py
    rules on a bigger island. Centres about SPACING apart so Isaac can chain
    them; clear of the ship ring, the pads, trails and spawns."""
    g, rng = w["g"], w["rng"]
    spacing = 560.0
    zone = ZONE_RADII[-1] + 400
    keepout = [(0.0, 0.0, SHIP_CLEAR)]
    for kind, x, y, _ring in w["pois"]:
        keepout.append((x, y, POI_KINDS[kind][5] + 120))
    for _k, x, y in w["landmarks"]:
        keepout.append((x, y, 360))
    rocks = []
    for _ in range(60000):
        x = rng.uniform(-zone * 1.3, zone * 1.3)
        y = rng.uniform(-zone, zone)
        rw, rh = round(rng.uniform(70, 150)), round(rng.uniform(45, 95))
        tx, ty = g.to_tile(x, y)
        if not g.inside(tx, ty) or w["city"][ty, tx] or w["trail"][ty, tx]:
            continue
        if not walkable_spot(w, x, y, 110):
            continue
        bad = False
        for bx, by, r in keepout:
            cx = min(max(bx, x - rw / 2), x + rw / 2)
            cy = min(max(by, y - rh / 2), y + rh / 2)
            if math.hypot(cx - bx, cy - by) < r:
                bad = True
                break
        if bad or any(math.hypot(x - rx, y - ry) < spacing for rx, ry, _w, _h in rocks):
            continue
        rocks.append((x, y, rw, rh))
    w["rocks"] = rocks


def dress(w):
    """Set dressing: props that make the planet a place. Looks only; kept
    off pads and trails so fights stay readable (Clarity)."""
    g, rng = w["g"], w["rng"]
    props = w["props"]
    keepout = [(0.0, 0.0, SHIP_CLEAR + 80)]
    for kind, x, y, _ring in w["pois"]:
        keepout.append((x, y, POI_KINDS[kind][5] + 60))
    for k, x, y in w["landmarks"]:
        keepout.append((x, y, LANDMARK_PAD[k] + 60))
    rocks = w["rocks"]

    def free(x, y, r=40):
        if any(math.hypot(x - kx, y - ky) < kr for kx, ky, kr in keepout):
            return False
        if any(abs(x - rx) < rw / 2 + 30 and abs(y - ry) < rh / 2 + 30
               for rx, ry, rw, rh in rocks):
            return False
        tx, ty = g.to_tile(x, y)
        if not g.inside(tx, ty) or w["blocked"][ty, tx] or w["trail"][ty, tx]:
            return False
        return True

    def scatter(kind, count, where, min_gap, cluster=1, spread=0.0):
        placed = []
        cells = np.argwhere(where)
        if len(cells) == 0:
            return
        for _ in range(count * 6):
            if len(placed) >= count:
                break
            cy_, cx_ = cells[rng.randrange(len(cells))]
            bx, by = g.centre(cx_, cy_)
            for _c in range(cluster):
                x = bx + rng.uniform(-spread, spread)
                y = by + rng.uniform(-spread, spread) * 0.6
                if not free(x, y):
                    continue
                if any(math.hypot(x - px, y - py) < min_gap for px, py in placed):
                    continue
                placed.append((x, y))
                props.append((kind, x, y, {"variant": rng.randrange(4),
                                           "scale": round(rng.uniform(0.75, 1.3), 2)}))

    land, mat = w["land"], w["material"]
    wild = land & ~w["city"] & ~w["blocked"]
    # Flora: dressed per genus by the client (the region decides the look).
    scatter("flora", 900, wild & ((mat == MAT["scrub"]) | (mat == MAT["wet"])),
            90, cluster=4, spread=110)
    scatter("boulder", 380, wild & ((mat == MAT["rock"]) | (mat == MAT["high"])), 120,
            cluster=2, spread=80)
    # Crash wreckage: pieces ~2 m across, with room to walk between them.
    scatter("debris", 70, w["trench"] & ~w["blocked"], 4.0 * METRE, cluster=3,
            spread=6.0 * METRE)
    scatter("bones", 60, wild & (mat == MAT["dust"]), 400)
    scatter("ruin", 90, wild & (w["level"] >= 7), 300, cluster=3, spread=160)
    # City dressing: a lamp every ~15 m of street, and the odd neon sign.
    streets = w["city"] & ~w["blocked"]
    scatter("lamp", 70, streets, 15.0 * METRE)
    scatter("sign", 30, streets, 22.0 * METRE)


# ---------------------------------------------------------------------------
# Tilesets
# ---------------------------------------------------------------------------
def diamond_mask():
    ys, xs = np.mgrid[0:TILE, 0:TILE]
    cx = cy = (TILE - 1) / 2.0
    return (np.abs(xs - cx) / (TILE / 2.0) + np.abs(ys - cy) / (TILE / 4.0)) <= 1.06


def draw_tile(rng, mat, variant):
    name, base, alt, seam, speck = mat
    mask = diamond_mask()
    img = np.zeros((TILE, TILE, 4), dtype=np.float32)
    img[..., :3] = np.array(base if variant % 2 == 0 else alt, dtype=np.float32)
    noise = np.array([[rng.uniform(-4, 4) for _ in range(TILE // 2)]
                      for _ in range(TILE // 2)], dtype=np.float32)
    img[..., :3] += np.kron(noise, np.ones((2, 2)))[..., None]
    ys, xs = np.mgrid[0:TILE, 0:TILE]
    cx = cy = (TILE - 1) / 2.0

    def line(x0, y0, x1, y1, colour, width=1):
        steps = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(steps):
            t = i / max(1, steps - 1)
            x, y = int(x0 + (x1 - x0) * t), int(y0 + (y1 - y0) * t)
            img[y:y + width, x:x + width, :3] = colour

    # Seams and cracks carry the detail.
    if name == "street":
        line(2, cy, cx, 17, seam)            # paving seams along the grid
        line(cx, 47, TILE - 3, cy, seam)
    elif name in ("rock", "high"):
        for _ in range(1 + variant % 3):
            x0, y0 = rng.uniform(14, 50), rng.uniform(22, 42)
            line(x0, y0, x0 + rng.uniform(-12, 12), y0 + rng.uniform(-5, 5), seam)
    elif name in ("dust", "pad", "path", "scrub", "wet"):
        if variant >= 2:
            x0, y0 = rng.uniform(16, 48), rng.uniform(24, 40)
            line(x0, y0, x0 + rng.uniform(-10, 10), y0 + rng.uniform(-4, 4), seam)
    elif name in ("water", "sea", "river"):
        for _ in range(2):
            x0, y0 = rng.uniform(16, 44), rng.uniform(26, 38)
            line(x0, y0, x0 + rng.uniform(6, 14), y0, speck)
    for _ in range((variant % 2) + (2 if name in ("scrub", "scar") else 1)):
        u, v = rng.uniform(-0.6, 0.6), rng.uniform(-0.55, 0.55)
        v *= 1 - abs(u)
        x, y = int(cx + u * TILE / 2), int(cy + v * TILE / 4)
        img[y:y + 2, x:x + 2, :3] = speck
    img[..., 3] = np.where(mask, 255, 0)
    img[~mask, :3] = 0
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def build_ground_tileset(seed):
    rng = random.Random(seed)
    sheet = Image.new("RGBA", (TILE * VARIANTS, TILE * len(MATERIALS)), (0, 0, 0, 0))
    for row, mat in enumerate(MATERIALS):
        for v in range(VARIANTS):
            sheet.paste(draw_tile(rng, mat, v), (v * TILE, row * TILE))
    return sheet


def build_data_tileset():
    """terrain_data.png: what the hidden layers paint with in Tiled.
    Row 0: height 0..15 (dark to light). Row 1: region 0..15. Row 2: blocked."""
    sheet = Image.new("RGBA", (TILE * 16, TILE * 3), (0, 0, 0, 0))
    d = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    palette = [(80, 80, 80), (230, 120, 80), (110, 200, 120), (120, 150, 240),
               (220, 200, 90), (200, 110, 200), (90, 210, 210), (240, 160, 60),
               (160, 230, 90), (240, 90, 140), (150, 120, 240), (90, 160, 120),
               (200, 200, 200), (140, 90, 60), (60, 120, 180), (250, 250, 120)]
    for i in range(16):
        x = i * TILE
        poly = [(x + 32, 16), (x + 63, 32), (x + 32, 47), (x, 32)]
        v = 30 + i * 14
        d.polygon(poly, fill=(v, v, v, 200))
        d.text((x + 26, 26), str(i), fill=(255, 60, 60, 255), font=font)
        poly1 = [(px, py + TILE) for px, py in poly]
        d.polygon(poly1, fill=palette[i] + (170,))
        d.text((x + 26, TILE + 26), str(i), fill=(0, 0, 0, 255), font=font)
    poly2 = [(32, 16 + 2 * TILE), (63, 32 + 2 * TILE), (32, 47 + 2 * TILE), (0, 32 + 2 * TILE)]
    d.polygon(poly2, fill=(230, 40, 40, 170))
    d.text((20, 2 * TILE + 26), "X", fill=(255, 255, 255, 255), font=font)
    return sheet


# ---------------------------------------------------------------------------
# TMX
# ---------------------------------------------------------------------------
def prop_xml(obj_id, name, typ, x, y, cy, props):
    out = [f'    <object id="{obj_id}" name="{name}" type="{typ}" x="{x:.1f}" y="{y + cy:.1f}">']
    if props:
        out.append("      <properties>")
        for key, value in props.items():
            kind = "int" if isinstance(value, int) else "float"
            out.append(f'        <property name="{key}" type="{kind}" value="{value}" />')
        out.append("      </properties>")
    out.append("      <point />")
    out.append("    </object>")
    return "\n".join(out)


def point_object(obj_id, name, wx, wy, cy, props=None):
    out = [f'    <object id="{obj_id}" name="{name}" x="{wx:.1f}" y="{wy + cy:.1f}">',
           "      <point />"]
    if props:
        out.append("      <properties>")
        for key, kind, value in props:
            out.append(f'        <property name="{key}" type="{kind}" value="{value}" />')
        out.append("      </properties>")
    out.append("    </object>")
    return "\n".join(out)


def ship_collision_from_arena(cy):
    """The ship's hull tiers were fitted by hand on arena.tmx: copy them."""
    arena = (MAPS / "arena.tmx").read_text()
    arena_cy = float(re.search(r'name="The Ship" x="[-\d.]+" y="([-\d.]+)"', arena).group(1))
    group = re.search(r'<objectgroup id="\d+" name="Collision">(.*?)</objectgroup>',
                      arena, re.S).group(1)
    out = []
    for m in re.finditer(r'<object id="\d+" name="([^"]+)" type="([^"]+)" x="([-\d.]+)" '
                         r'y="([-\d.]+)" width="([\d.]+)" height="([\d.]+)"', group):
        name, typ, x, y, wdt, hgt = m.groups()
        out.append((name, typ, float(x), float(y) - arena_cy, float(wdt), float(hgt)))
    return out


def write_tmx(w, path, seed):
    g = w["g"]
    size, cy = g.size, g.cy
    n_ground = len(MATERIALS) * VARIANTS
    data_gid = n_ground + 1
    rng = random.Random(seed * 31 + 5)

    ground_rows, height_rows, region_rows, blocked_rows = [], [], [], []
    for ty in range(size):
        gr, hr, rr, br = [], [], [], []
        for tx in range(size):
            m = int(w["material"][ty, tx])
            roll = rng.random()
            variant = 0 if roll < 0.5 else 1 if roll < 0.85 else 2 if roll < 0.95 else 3
            gr.append(str(m * VARIANTS + variant + 1))
            hr.append(str(data_gid + int(w["level"][ty, tx])))
            reg = int(w["region"][ty, tx])
            rr.append(str(data_gid + 16 + reg) if reg else "0")
            br.append(str(data_gid + 32) if w["blocked"][ty, tx] else "0")
        ground_rows.append(",".join(gr))
        height_rows.append(",".join(hr))
        region_rows.append(",".join(rr))
        blocked_rows.append(",".join(br))

    next_id = 1
    groups = {}

    def add(layer, xml):
        groups.setdefault(layer, []).append(xml)

    add("Ship", point_object(next_id, "The Ship", 0, 0, cy)); next_id += 1
    for i in range(4):
        a = math.pi / 2 + i * math.pi / 2
        add("PlayerSpawns", point_object(next_id, f"Spawn_{i + 1}", math.cos(a) * 150,
                                         math.sin(a) * 110 + 40 + 110, cy))
        next_id += 1

    counts = {}
    for kind, x, y, ring in w["pois"]:
        otype, desc, radius, itime, (grunts, elites), _pad = POI_KINDS[kind]
        counts[kind] = counts.get(kind, 0) + 1
        name = f"{kind.title().replace('_', '')}_{counts[kind]}"
        props = [("objective_type", "string", otype), ("description", "string", desc),
                 ("radius", "float", f"{radius:.1f}"),
                 ("interaction_time", "float", f"{itime:.1f}")]
        if ring == "far" and grunts:
            grunts += 1
        camp = grunts + elites
        if otype in ("capture_outpost", "salvage_medpacks"):
            props.append(("enemies_required", "int", str(camp)))
        if otype == "save_the_frogs":
            d = math.hypot(x, y)
            px, py = x - x / d * 260.0, y - y / d * 260.0
            props += [("enemies_required", "int", "3"),
                      ("deposit_x", "float", f"{px:.1f}"),
                      ("deposit_y", "float", f"{py + cy:.1f}")]
        add("Objectives", point_object(next_id, name, x, y, cy, props)); next_id += 1
        for i in range(camp):
            elite = i >= grunts
            if elite:
                gx, gy = x, y + (0.0 if grunts == 0 else 12.0)
            else:
                a = 2 * math.pi * i / max(1, grunts) + rng.uniform(-0.3, 0.3)
                ring_r = (radius * 0.6 if otype in ("capture_outpost", "salvage_medpacks")
                          else radius + 40.0)
                gx, gy = x + math.cos(a) * ring_r, y + math.sin(a) * ring_r * 0.8
            camp_props = [("enemy_type", "string", "venoxarid_grunt")]
            if elite:
                camp_props.append(("elite", "bool", "true"))
            add("EnemySpawns", point_object(next_id, f"{name}_guard_{i + 1}", gx, gy, cy,
                                            camp_props))
            next_id += 1

    for i, angle_deg in enumerate((225.0, 45.0)):
        a = math.radians(angle_deg)
        add("Objectives", point_object(next_id, f"Turret_{i + 1}", math.cos(a) * 420,
                                       math.sin(a) * 420, cy, [
            ("objective_type", "string", "derelict_turret"),
            ("description", "string", "Repair the derelict turret"),
            ("radius", "float", "70.0"), ("interaction_time", "float", "4.0")]))
        next_id += 1

    for name, typ, x, y, wdt, hgt in ship_collision_from_arena(cy):
        add("Collision", f'    <object id="{next_id}" name="{name}" type="{typ}" '
                         f'x="{x:.1f}" y="{y + cy:.1f}" width="{wdt:g}" height="{hgt:g}" />')
        next_id += 1
    for i, (x, y, rw, rh) in enumerate(sorted(w["rocks"], key=lambda r: (r[1], r[0]))):
        add("Terrain", f'    <object id="{next_id}" name="Rock_{i + 1}" type="terrain" '
                       f'x="{x - rw / 2:.1f}" y="{y - rh / 2 + cy:.1f}" width="{rw}" height="{rh}" />')
        next_id += 1
    for i, (kind, x, y, props) in enumerate(w["props"]):
        add("Props", prop_xml(next_id, f"{kind}_{i + 1}", kind, x, y, cy, props))
        next_id += 1

    order = ["Ship", "PlayerSpawns", "Objectives", "EnemySpawns", "Collision", "Terrain", "Props"]
    xml_groups = []
    layer_id = 5
    for name in order:
        items = groups.get(name, [])
        xml_groups.append(f'  <objectgroup id="{layer_id}" name="{name}">\n' +
                          "\n".join(items) + "\n  </objectgroup>")
        layer_id += 1

    def layer(lid, name, rows, visible=True):
        vis = "" if visible else ' visible="0"'
        return (f'  <layer id="{lid}" name="{name}" width="{size}" height="{size}"{vis}>\n'
                f'    <data encoding="csv">\n' + ",\n".join(rows) + "\n</data>\n  </layer>")

    tmx = f'''<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.10.2" orientation="isometric" renderorder="right-down" width="{size}" height="{size}" tilewidth="{TILE}" tileheight="{TILE}" infinite="0" nextlayerid="{layer_id}" nextobjectid="{next_id}">
  <properties>
    <property name="generator" value="tools/maps/generate_planet.py --seed {seed} --size {size}" />
    <property name="zone_scale" type="float" value="{ZONE_SCALE}" />
  </properties>
  <tileset firstgid="1" name="planet_tiles" tilewidth="{TILE}" tileheight="{TILE}" tilecount="{n_ground}" columns="{VARIANTS}">
    <image source="planet_tiles.png" width="{TILE * VARIANTS}" height="{TILE * len(MATERIALS)}" />
  </tileset>
  <tileset firstgid="{data_gid}" name="terrain_data" tilewidth="{TILE}" tileheight="{TILE}" tilecount="48" columns="16">
    <image source="terrain_data.png" width="{TILE * 16}" height="{TILE * 3}" />
  </tileset>
{layer(1, "Ground", ground_rows)}
{layer(2, "Height", height_rows, False)}
{layer(3, "Region", region_rows, False)}
{layer(4, "Blocked", blocked_rows, False)}
{chr(10).join(xml_groups)}
</map>
'''
    path.write_text(tmx)
    return next_id


# ---------------------------------------------------------------------------
# Preview: the whole planet from above, for review (not used by the game)
# ---------------------------------------------------------------------------
def preview(w, path):
    g = w["g"]
    size = g.size
    scale = 0.06  # world px -> preview px
    half_w = size * HALF_W
    half_h = size * QUARTER_H
    W, H = int(half_w * 2 * scale), int(half_h * 2 * scale) + 40
    img = Image.new("RGB", (W, H), (6, 8, 14))
    d = ImageDraw.Draw(img)
    colours = {MAT[k]: c for k, c in {
        "dust": (120, 110, 100), "scrub": (70, 92, 80), "rock": (96, 96, 112),
        "high": (150, 150, 170), "wet": (60, 80, 92), "path": (190, 170, 130),
        "pad": (230, 210, 150), "scar": (60, 30, 26), "street": (80, 80, 100),
        "river": (70, 120, 160), "water": (34, 60, 90), "sea": (14, 22, 38)}.items()}
    lvl = w["level"]
    for ty in range(size):
        for tx in range(size):
            wx_, wy_ = g.centre(tx, ty)
            lift = (lvl[ty, tx] - BASE_LEVEL) * LEVEL_PIXELS
            px = (wx_ + half_w) * scale
            py = (wy_ - lift + half_h) * scale + 20
            c = colours[int(w["material"][ty, tx])]
            shade = 0.75 + 0.03 * lvl[ty, tx]
            if w["blocked"][ty, tx] and w["land"][ty, tx] and not w["lakes"][ty, tx]:
                c = (40, 38, 46) if not w["city"][ty, tx] else (30, 30, 44)
            c = tuple(min(255, int(v * shade)) for v in c)
            d.rectangle([px - 1.2, py - 0.7, px + 1.2, py + 0.7], fill=c)
    # Region borders
    reg = w["region"]
    for ty in range(1, size):
        for tx in range(1, size):
            if reg[ty, tx] and (reg[ty, tx] != reg[ty - 1, tx] or reg[ty, tx] != reg[ty, tx - 1]) \
                    and reg[ty - 1, tx] and reg[ty, tx - 1]:
                wx_, wy_ = g.centre(tx, ty)
                lift = (lvl[ty, tx] - BASE_LEVEL) * LEVEL_PIXELS
                px = (wx_ + half_w) * scale
                py = (wy_ - lift + half_h) * scale + 20
                d.point((px, py), fill=(200, 200, 255))

    def to_px(x, y):
        tx, ty = g.to_tile(x, y)
        lift = (lvl[ty, tx] - BASE_LEVEL) * LEVEL_PIXELS if g.inside(tx, ty) else 0
        return (x + half_w) * scale, (y - lift + half_h) * scale + 20

    for r in ZONE_RADII:
        cx, cy_ = to_px(0, 0)
        d.ellipse([cx - r * scale, cy_ - r * scale, cx + r * scale, cy_ + r * scale],
                  outline=(70, 130, 255))
    for x, y, rw, rh in w["rocks"]:
        px, py = to_px(x, y)
        d.rectangle([px - 1, py - 1, px + 1, py + 1], fill=(170, 170, 190))
    for kind, x, y, _p in w["props"]:
        if kind in ("city_block",):
            px, py = to_px(x, y)
            d.rectangle([px - 3, py - 2 - _p["tall"] * scale * 0.5, px + 3, py + 2],
                        fill=(36, 40, 70), outline=(90, 200, 255))
        if kind == "skyline":
            px, py = to_px(x, y)
            d.rectangle([px - _p["width"] * scale / 2, py - _p["tall"] * scale, px + _p["width"] * scale / 2, py],
                        fill=(20, 24, 44), outline=(60, 120, 200))
    for kind, x, y in w["landmarks"]:
        px, py = to_px(x, y)
        d.polygon([(px, py - 14), (px + 7, py), (px - 7, py)], fill=(255, 120, 220))
        d.text((px + 8, py - 10), kind, fill=(255, 160, 230))
    for kind, x, y, ring in w["pois"]:
        px, py = to_px(x, y)
        col = (20, 20, 20) if ring == "near" else (240, 150, 40) if ring == "far" else (240, 220, 90)
        d.ellipse([px - 5, py - 5, px + 5, py + 5], fill=col, outline=(255, 255, 255))
    cx, cy_ = to_px(0, 0)
    d.rectangle([cx - 6, cy_ - 4, cx + 6, cy_ + 4], fill=(120, 255, 170))
    d.text((8, 4), "planet overview: pads (near black / mid yellow / far orange), landmarks pink, "
                   "storm rings blue, city blocks cyan", fill=(220, 220, 220))
    img.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--seed", type=int, default=3)
    ap.add_argument("--size", type=int, default=380)
    ap.add_argument("--out", default=str(MAPS / "planet.tmx"))
    ap.add_argument("--preview", default="")
    args = ap.parse_args()
    w = build(args.seed, args.size)
    build_ground_tileset(args.seed).save(MAPS / "planet_tiles.png")
    build_data_tileset().save(MAPS / "terrain_data.png")
    write_tmx(w, Path(args.out), args.seed)
    land = int(w["land"].sum())
    print(f"wrote {args.out}: {args.size}x{args.size} tiles, {land} land tiles, "
          f"{len(w['pois'])} points of interest, {len(w['landmarks'])} landmarks, "
          f"{len(w['rocks'])} rocks, {len(w['props'])} props")
    if args.preview:
        preview(w, args.preview)
        print(f"preview: {args.preview}")


if __name__ == "__main__":
    main()
