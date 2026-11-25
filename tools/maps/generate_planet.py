#!/usr/bin/env python3
"""Generate the planet map: assets/maps/planet.tmx and its tilesets.

The planet is a continent of earth-like biomes around the crashed ship.
Biomes are geography, not genus: the four genera live across them (each
region is dealt a genus per match by the server, as before). Read
docs/design/MAP_PIPELINE.md first; the rules here follow it.

What it builds, in order:
  1. Land: a continent filling the map's diamond, with bays and coves, in a
     dark sea.
  2. Biomes: anchor points laid out like a coast of the Americas (forest and
     snow in the north-west, a warm coast and desert in the south-west,
     plains in the middle, a city on the north-east shore, swamp around the
     east bay, jungle in the south-east), with organic borders. The
     Pacific Northwest and the Arctic are one forest: the colder, higher,
     more northern part of it is Arctic. Tidal and city hug the coast.
     Slivers are folded into their neighbours so every biome is a big
     place (see "Biomes" in MAP_PIPELINE.md for the sizing rule).
  3. Crash sites: candidate landing spots where two or three biomes meet.
     The ship sits on one (--site); all are written to the map.
  4. Height: each biome has its own relief (Cascades ridges, desert mesas,
     flat swamp, rolling plains), blended across borders. The crash basin
     around the ship is flat. Peaks are impassable and shape the routes.
  5. Water: lakes (frozen and walkable in the Arctic), marsh in the swamp,
     tide pools and beaches on the tidal coast, rivers off the mountains.
  6. Regions: genus territories, nested inside biomes (a territory never
     crosses a biome border), dealt a genus per match by the server.
  7. The city: streets and blocks of the city biome, at real scale.
  8. The crash: a scorched trench the ship ploughed on its way in.
  9. Landmarks, points of interest (three rings around the ship), trails
     from the ship to every one, lifeline rocks, and set dressing per
     biome (conifers, cacti, palms, jungle trees, cypress, grass...).

Everything is seeded: the same seed and site give the same planet. The
output is a normal Tiled map (big layers are zlib-compressed). Hand edits
in Tiled are fine, but re-running the generator overwrites them.

Usage:
  python3 tools/maps/generate_planet.py [--seed 3] [--size 800] [--site 0]
      [--preview docs/prototype/planet_overview.png]
"""

import argparse
import base64
import heapq
import math
import random
import re
import struct
import zlib
from collections import deque
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
TILE_STEP_PX = 45.0     # average world px per grid step, for grid distances
HERO_SPEED = 255.0      # Config::Player::SPEED, world px per second


def tiles(metres):
    """Whole tiles along an axis covering `metres`."""
    return max(1, round(metres / TILE_EDGE_M))


# Must match TerrainGrid (include/TerrainGrid.h).
BASE_LEVEL = 4
LEVELS = 16
LEVEL_PIXELS = 16.0

ZONE_SCALE = 1.75          # written to the map; scales the storm's radii
ZONE_RADII = [2000.0 * ZONE_SCALE, 2500.0 * ZONE_SCALE, 2900.0 * ZONE_SCALE]
BASIN_RADIUS = 950.0       # flat ground around the ship
# The crash site: scorched ground and wreckage around every landing, its own
# place on the planet (and the "landing" music's area: MusicDirector).
CRASH_SITE_RADIUS = 1300.0
SHIP_CLEAR = 420.0
COAST = 0.86               # land where the shape metric is below this
SEA_KEEP = 2600.0          # sea tiles drawn this far out; past it, empty
# The continent is laid out for a 720-tile grid. The map's grid is bigger
# (--size, 800 by default) so the continent sits in open sea and never runs
# into the grid's edge, which read as a hard line (issue #50). BORDER_SEA:
# whatever happens, no land within this many world px (square to the edge)
# of the grid's edge, with BORDER_WOBBLE of noise on it.
CONTINENT_TILES = 720
BORDER_SEA = 900.0
BORDER_WOBBLE = 250.0

# ---------------------------------------------------------------------------
# Biomes. Ids are the Biome layer's values (0 = sea / none). Earth-like
# places; the genera appear across them.
# ---------------------------------------------------------------------------
PLAINS, PNW, ARCTIC, DESERT, TIDAL, CITY, JUNGLE, SWAMP = range(1, 9)
BIOMES = {
    PLAINS: ("plains", "Plains", (196, 184, 96)),
    PNW: ("pnw", "Pacific Northwest", (64, 156, 104)),
    ARCTIC: ("arctic", "Arctic", (206, 226, 246)),
    DESERT: ("desert", "Desert", (228, 150, 72)),
    TIDAL: ("tidal", "Tidal coast", (92, 200, 214)),
    CITY: ("city", "City", (156, 132, 236)),
    JUNGLE: ("jungle", "Jungle", (52, 206, 78)),
    SWAMP: ("swamp", "Swamp", (150, 128, 66)),
}

# Anchors: (biome, x, y, bias) with x, y as fractions of the map's half
# extents (screen space: -y is north) and bias in world px (negative grows
# the biome). Several anchors may share a biome. The plains are the
# connective tissue between the rest, as on a real continent.
ANCHORS = [
    (PLAINS, 0.04, -0.04, 900.0),
    (PLAINS, 0.16, -0.50, 1400.0),
    (PLAINS, 0.04, 0.48, -300.0),
    (PNW, -0.42, -0.12, -700.0),
    (PNW, -0.30, -0.44, 0.0),
    (PNW, -0.68, -0.02, -400.0),       # its own coast: rocky, foggy
    (ARCTIC, -0.22, -0.68, 0.0),       # cascadia: split by cold, see below
    (DESERT, -0.22, 0.34, -300.0),
    (TIDAL, -0.56, 0.46, -4400.0),     # south of the range, like San Diego
    (CITY, 0.48, -0.34, -600.0),
    (JUNGLE, 0.34, 0.48, -1900.0),
    (SWAMP, 0.62, 0.12, -3200.0),
]
ARCTIC_SHARE = 0.44 
BOREAL_FRINGE = 1300.0     # forest kept between the snow and the dry side
# The spine: one mountain range down the continent, west of the plains,
# bending to the sea in the south-west (the Cascades and Sierra, then the
# Transverse Ranges). The wet side (west and north of it) is forest and
# snow; the dry side is everything else, so the desert sits in its rain
# shadow and the warm coast lies south of the bend. Points are fractions
# of the half extents, north to south. Passes are where it drops low.
SPINE = [(-0.02, -0.86), (-0.10, -0.50), (-0.20, -0.16), (-0.27, 0.06),
         (-0.40, 0.20), (-0.60, 0.22), (-0.82, 0.20)]
SPINE_WIDTH = 1300.0       # world px from crest to foot (roughly)
SPINE_PASSES = [0.16, 0.42, 0.66, 0.88]  # along the spine, 0 north .. 1 south       # of the cascadian forest (PNW + Arctic)
TIDAL_DEPTH = 7500.0       # world px inland the tidal coast reaches
CITY_DEPTH = 8000.0
MIN_FRAGMENT_M2 = 9000.0   # pieces of a biome smaller than this fold away

# Relief per biome: base level, hill amplitude, ridge (mountain) amplitude,
# mesa step, deep-lake threshold on the lake noise (None: no deep lakes).
BIOME_RELIEF = {
    PLAINS: (4.6, 1.5, 0.0, 0.0, 0.44),
    PNW: (5.6, 3.2, 5.0, 0.0, 0.36),
    ARCTIC: (6.4, 2.8, 6.5, 0.0, 0.33),
    DESERT: (5.0, 1.1, 0.0, 3.0, None),
    TIDAL: (3.4, 1.2, 0.0, 0.0, None),
    CITY: (5.0, 0.4, 0.0, 0.0, None),
    JUNGLE: (5.2, 3.6, 3.6, 0.0, 0.40),
    SWAMP: (2.8, 0.5, 0.0, 0.0, 0.22),
}

# Genus territories (the Region layer, 1..15) per biome, by area share.
REGION_SLOTS = 15

# ---------------------------------------------------------------------------
# Ground materials: one row of the tileset each, four variants per row.
# Painted dark and quiet (the look darkens them further); detail lives in
# seams and specks, never in bright texture (ART_DIRECTION.md, "World").
# Biomes differ by hue at the same low value, so they read at a glance
# without lighting the floor up. Snow is the one light ground, kept cool.
# ---------------------------------------------------------------------------
MATERIALS = [
    # name,     base,            alt,             seam,           speck
    ("dust",     (86, 80, 78),   (80, 75, 74),   (64, 60, 60),   (110, 102, 96)),
    ("scrub",    (52, 62, 60),   (48, 57, 56),   (36, 44, 44),   (78, 92, 84)),
    ("rock",     (70, 70, 82),   (64, 64, 76),   (44, 44, 54),   (96, 96, 110)),
    ("high",     (96, 98, 114),  (90, 92, 108),  (62, 64, 78),   (128, 130, 146)),
    # Damp ground by water: earthy, never blue, so it never reads as water.
    ("wet",      (46, 50, 42),   (43, 47, 39),   (32, 35, 29),   (70, 76, 60)),
    ("path",     (104, 96, 88),  (98, 91, 84),   (78, 72, 66),   (126, 116, 104)),
    ("pad",      (112, 104, 94), (106, 99, 90),  (84, 78, 70),   (138, 128, 112)),
    ("scar",     (36, 30, 30),   (32, 27, 28),   (22, 18, 20),   (150, 70, 40)),
    ("street",   (58, 58, 70),   (54, 54, 66),   (34, 34, 44),   (90, 90, 110)),
    ("river",    (40, 62, 78),   (38, 58, 74),   (30, 46, 62),   (80, 120, 140)),
    # Deep water: the bluest grounds, with glints that read as a surface.
    ("water",    (22, 34, 52),   (20, 31, 49),   (16, 25, 40),   (74, 110, 140)),
    ("sea",      (14, 20, 34),   (13, 19, 32),   (10, 15, 26),   (46, 70, 98)),
    # Biome grounds.
    ("grass",    (62, 70, 48),   (58, 66, 45),   (44, 50, 34),   (88, 98, 62)),
    ("meadow",   (72, 72, 50),   (67, 68, 47),   (52, 52, 36),   (98, 96, 64)),
    # Forest floor: a dark green, kept off the blue-teal the water uses.
    ("moss",     (38, 52, 38),   (35, 48, 35),   (25, 36, 26),   (62, 82, 56)),
    ("needles",  (50, 46, 44),   (46, 42, 40),   (34, 31, 30),   (74, 66, 58)),
    ("snow",     (110, 118, 134), (104, 112, 128), (82, 90, 108), (136, 144, 160)),
    ("ice",      (70, 92, 112),  (66, 87, 106),  (50, 70, 90),   (110, 136, 156)),
    ("sand",     (104, 88, 66),  (98, 83, 62),   (80, 66, 48),   (128, 108, 80)),
    ("redrock",  (96, 64, 54),   (90, 60, 51),   (66, 42, 36),   (122, 84, 68)),
    ("beach",    (108, 100, 84), (102, 95, 80),  (84, 76, 62),   (132, 124, 104)),
    ("tidepool", (46, 70, 78),   (43, 66, 74),   (32, 52, 60),   (82, 116, 122)),
    ("jungle",   (30, 60, 32),   (28, 55, 30),   (18, 40, 20),   (50, 92, 50)),
    ("mud",      (58, 50, 38),   (54, 46, 35),   (40, 34, 26),   (78, 68, 50)),
    ("marsh",    (50, 54, 38),   (46, 50, 35),   (34, 38, 26),   (78, 84, 58)),
    ("bog",      (34, 46, 42),   (32, 43, 40),   (24, 34, 32),   (70, 92, 80)),
    ("vine",     (40, 62, 30),   (37, 58, 28),   (26, 44, 18),   (64, 96, 44)),
    # Impassable high ground (peaks, sealed-off pockets): darker and cracked,
    # so a wall never looks like the walkable slope next to it. Appended last
    # so every other material keeps its tile ids.
    ("cliff",    (40, 38, 48),   (37, 35, 45),   (20, 18, 24),   (96, 92, 108)),
    # The crash site: ash and scorch from the landing, embers in the specks.
    ("ash",      (40, 33, 32),   (37, 31, 30),   (20, 16, 16),   (190, 96, 48)),
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
    "probe": ("recover_probe", "Dig out the probe and boot it: it maps the land around it",
              70.0, 4.0, (2, 0), 170.0),
    "frogs": ("save_the_frogs", "Carry the frogs to the pond", 80.0, 1.5,
              (0, 0), 190.0),
    "gas": ("noxious_gas", "Kick the spores in the toxic cloud", 110.0, 4.0,
            (0, 0), 190.0),
    "littlejohn": ("little_john", "Open the gate its guardian is watching",
                   110.0, 1.0, (0, 1), 190.0),
    # The city's own errands (issue #49): the same mechanics, dressed for
    # streets, and with pads that fit a crossroads rather than open ground.
    "depot": ("alien_scrapyard", "Strip the wrecked transit depot for scrap and haul it home",
              70.0, 3.0, (2, 0), 150.0),
    "clinic": ("salvage_medpacks", "Clear the squatters, then crack the clinic's supply cache",
               120.0, 1.0, (3, 0), 160.0),
    "plaza": ("capture_outpost", "Take back the plaza from the nest that holds it",
              130.0, 1.0, (3, 1), 170.0),
}

# What each ring asks for. Near: quick stops in round 1. Mid: a real trip.
# Far: rounds 2-3, bigger camps, two infusions; some sit past round 3's
# storm, worth a dash through it (Strategic Crunch).
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
# Coverage (issue #49): after the rings, extra objectives go into the
# biggest empty stretches until, for every landing site, no walkable spot is
# more than COVER_GAP world px (straight line, ~12 s at hero speed) from an
# objective in play. Fill objectives are in play for every landing site.
COVER_GAP = 3000.0
COVER_MAX = 40
# What a fill objective is, by biome: the first that fits wins, so each
# place keeps its own kind of errand.
FILL_KINDS = {
    CITY: ["plaza", "clinic", "depot"],
    SWAMP: ["frogs", "gas", "outpost_big"],
    JUNGLE: ["gas", "littlejohn", "outpost_big"],
    TIDAL: ["frogs", "scrapyard", "outpost"],
    DESERT: ["scrapyard", "probe", "outpost_big"],
    ARCTIC: ["probe", "outpost_big", "medpacks"],
    PNW: ["littlejohn", "outpost_big", "probe"],
    PLAINS: ["outpost_big", "medpacks", "scrapyard"],
}
# Objectives are fixed places on the planet. One is in play for every
# landing site within this reach (the far ring's edge), not just the site it
# was placed around, so sites close together share the ground between them.
POI_REACH = 9000.0
# Where an objective type feels at home, by biome (added to its score).
POI_BIOME = {
    "frogs": {SWAMP: 3.0, TIDAL: 2.0, JUNGLE: 2.0},
    "gas": {SWAMP: 3.0, JUNGLE: 2.0, DESERT: 1.0},
    "scrapyard": {CITY: 2.0, DESERT: 1.0, PLAINS: 0.5},
    "medpacks": {CITY: 2.5},
    "depot": {CITY: 4.0}, "clinic": {CITY: 4.0}, "plaza": {CITY: 4.0},
    "outpost": {PNW: 1.0, ARCTIC: 1.0, DESERT: 1.0},
    "probe": {ARCTIC: 1.5, DESERT: 1.0},
    "littlejohn": {JUNGLE: 1.0, PNW: 1.0},
}

LANDMARKS = ["monolith", "ribcage", "crater", "freighter", "spire", "monolith"]
LANDMARKS_PER_SITE = 5
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
        # Half extents of the continent (not of the grid): the land's shape,
        # biomes and sites are laid out in these.
        self.hx, self.hy = CONTINENT_TILES * HALF_W, CONTINENT_TILES * QUARTER_H
        ty, tx = np.mgrid[0:size, 0:size]
        self.tx, self.ty = tx, ty
        self.wx = ((tx - ty) * HALF_W).astype(np.float32)
        self.wy = ((tx + ty) * QUARTER_H - self.cy).astype(np.float32)

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


def shift(a, dy, dx, fill=False):
    """a moved by (dy, dx) cells, edges filled with `fill`."""
    out = np.full_like(a, fill)
    h, w = a.shape
    out[max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = \
        a[max(-dy, 0):h + min(-dy, 0), max(-dx, 0):w + min(-dx, 0)]
    return out


def dilate(mask, eight=True):
    out = mask.copy()
    steps = [(-1, 0), (1, 0), (0, -1), (0, 1)]
    if eight:
        steps += [(-1, -1), (-1, 1), (1, -1), (1, 1)]
    for dy, dx in steps:
        out |= shift(mask, dy, dx)
    return out


def grid_distance(mask, limit):
    """Approximate distance in world px from every cell to `mask`, capped at
    limit (alternating 4- and 8-neighbour growth: an octagonal metric)."""
    steps = int(limit / TILE_STEP_PX) + 1
    dist = np.full(mask.shape, limit, dtype=np.float32)
    dist[mask] = 0.0
    seen = mask.copy()
    front = mask.copy()
    for k in range(1, steps):
        grown = dilate(front, eight=(k % 2 == 0)) & ~seen
        if not grown.any():
            break
        dist[grown] = k * TILE_STEP_PX
        seen |= grown
        front = grown
    return dist


def blur(a, r, passes=2):
    """Box blur of radius r cells (edge-clamped), `passes` times."""
    out = a.astype(np.float32)
    k = 2 * r + 1
    for _ in range(passes):
        p = np.pad(out, ((r + 1, r), (0, 0)), mode="edge")
        c = np.cumsum(p, axis=0, dtype=np.float64)
        out = ((c[k:] - c[:-k]) / k).astype(np.float32)
        p = np.pad(out, ((0, 0), (r + 1, r)), mode="edge")
        c = np.cumsum(p, axis=1, dtype=np.float64)
        out = ((c[:, k:] - c[:, :-k]) / k).astype(np.float32)
    return out


def components(mask):
    """Connected pieces (8-neighbour) of mask: a label array and sizes."""
    size_y, size_x = mask.shape
    label = np.zeros(mask.shape, dtype=np.int32)
    sizes = [0]
    todo = np.argwhere(mask)
    current = 0
    for y0, x0 in todo:
        if label[y0, x0]:
            continue
        current += 1
        label[y0, x0] = current
        queue = deque([(y0, x0)])
        n = 0
        while queue:
            y, x = queue.popleft()
            n += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < size_y and 0 <= nx < size_x and mask[ny, nx] \
                            and not label[ny, nx]:
                        label[ny, nx] = current
                        queue.append((ny, nx))
        sizes.append(n)
    return label, np.array(sizes)


def fill_from_neighbours(labels, holes):
    """Give every hole cell the label of its nearest labelled neighbour."""
    labels = labels.copy()
    holes = holes.copy()
    labels[holes] = 0
    for _ in range(4000):
        if not holes.any():
            break
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1),
                       (-1, 1), (1, -1)):
            src = shift(labels, dy, dx, 0)
            take = holes & (src > 0)
            labels[take] = src[take]
            holes &= ~take
    return labels


# ---------------------------------------------------------------------------
# The planet
# ---------------------------------------------------------------------------
def edge_depth(g):
    """World px from each tile centre to the nearest edge of the grid's
    diamond, measured square to that edge (a grid step is ~28.6 px)."""
    step = 1.0 / math.hypot(1.0 / (2 * HALF_W), 1.0 / (2 * QUARTER_H))
    inner = np.minimum.reduce([g.tx, g.ty, g.size - 1 - g.tx, g.size - 1 - g.ty])
    return (inner + 0.5) * step


def border_sea(g, noise):
    """Tiles that must stay sea: a belt inside every edge of the grid."""
    shore = BORDER_SEA + BORDER_WOBBLE * noise.fbm(g.wx, g.wy, 1700, 3)
    return edge_depth(g) <= shore


def make_land(g, noise):
    wx, wy = g.wx, g.wy
    nx_, ny_ = wx / g.hx, wy / g.hy
    diamond = np.abs(nx_) + np.abs(ny_)
    ellipse = np.hypot(nx_, ny_) * 1.12
    e = 0.7 * diamond + 0.3 * ellipse
    e = e + 0.085 * noise.fbm(wx, wy, 5200, 4) + 0.03 * noise.fbm(wx + 77, wy, 1500, 3)
    # Bays and coves: the east bay (the swamp's), coves on the tidal coast,
    # an inlet under the city.
    for bx, by, rx, ry, depth in ((0.66, 0.16, 0.12, 0.11, 0.55),
                                  (-0.72, 0.16, 0.06, 0.05, 0.45),
                                  (-0.60, 0.40, 0.05, 0.05, 0.45),
                                  (-0.18, 0.74, 0.07, 0.05, 0.40)):
        d = np.hypot((nx_ - bx) / rx, (ny_ - by) / ry)
        e = e + depth * np.clip(1.0 - d, 0, 1)
    # A peninsula off the warm south-west coast (Baja, below San Diego):
    # room for the tidal coast to be a place, not a strip.
    d = np.hypot((nx_ + 0.50) / 0.20, (ny_ - 0.56) / 0.16)
    e = e - 0.30 * np.clip(1.0 - d, 0, 1) ** 0.7
    return (e < COAST) & ~border_sea(g, noise), e


def spine_field(g, noise):
    """Distance to the spine's crest (world px), which side of it a cell
    is on (True: the wet west/north side) and how far along it (0..1)."""
    wx = g.wx + 900 * noise.fbm(g.wx, g.wy, 3000, 3)
    wy = g.wy + 600 * noise.fbm(g.wx + 400, g.wy - 90, 3000, 3)
    pts = [(fx * g.hx, fy * g.hy) for fx, fy in SPINE]
    # Extend both ends well past the coast so "side" is defined everywhere.
    (x0, y0), (x1, y1) = pts[0], pts[1]
    pts.insert(0, (x0 + (x0 - x1) * 3, y0 + (y0 - y1) * 3))
    (x0, y0), (x1, y1) = pts[-1], pts[-2]
    pts.append((x0 + (x0 - x1) * 3, y0 + (y0 - y1) * 3))
    lengths = [math.hypot(bx - ax, by - ay) for (ax, ay), (bx, by) in zip(pts, pts[1:])]
    real0, real1 = lengths[0], sum(lengths[:-1])
    best = np.full(wx.shape, np.inf, dtype=np.float32)
    side = np.zeros(wx.shape, dtype=bool)
    along = np.zeros(wx.shape, dtype=np.float32)
    run = 0.0
    for ((ax, ay), (bx, by)), seg in zip(zip(pts, pts[1:]), lengths):
        dx, dy = bx - ax, by - ay
        t = np.clip(((wx - ax) * dx + (wy - ay) * dy) / (seg * seg), 0, 1)
        d = np.hypot(wx - (ax + t * dx), wy - (ay + t * dy))
        cross = dx * (wy - ay) - dy * (wx - ax)
        take = d < best
        best[take] = d[take]
        # Walking north to south, the west/north side is on the right in
        # screen space (y down): positive cross product.
        side[take] = cross[take] > 0
        along[take] = (run + t[take] * seg - real0) / (real1 - real0)
        run += seg
    return best, side, np.clip(along, 0, 1)


def lay_out_biomes(g, land, sea_dist, noises, rng, spine):
    """The biome of every land cell (0 for sea)."""
    n_warp, n_cold, n_ridge = noises
    wx, wy = g.wx, g.wy
    # Domain warp at two scales: big bends, then a ragged edge. Without it
    # the borders between anchors are ruled lines.
    warp_x = wx + 7000 * n_warp.fbm(wx, wy, 4200, 3) + 1800 * n_warp.fbm(wx, wy + 51, 1300, 2)
    warp_y = wy + 4500 * n_warp.fbm(wx + 999, wy - 333, 4200, 3) + \
        1200 * n_warp.fbm(wx - 71, wy, 1300, 2)
    _sd, wet_side, _along = spine
    dists = []
    for biome, fx, fy, bias in ANCHORS:
        ax, ay = fx * g.hx, fy * g.hy
        d = np.hypot(warp_x - ax, warp_y - ay) + bias
        # The range is the border: forest and snow on its wet side only.
        forest_anchor = biome in (PNW, ARCTIC)
        d = np.where(wet_side == forest_anchor, d, np.inf)
        if biome == TIDAL:
            d = np.where(sea_dist <= TIDAL_DEPTH, d, np.inf)
        if biome == CITY:
            d = np.where(sea_dist <= CITY_DEPTH, d, np.inf)
        dists.append(d.astype(np.float32))
    dists = np.stack(dists)
    owner = np.argmin(dists, axis=0)
    ids = np.array([a[0] for a in ANCHORS])
    biome = ids[owner]
    biome[~land] = 0

    # Cascadia: the PNW and Arctic anchors make one forest; its colder part
    # (north, high, a little noise) is Arctic, so snow starts deeper in.
    forest = (biome == PNW) | (biome == ARCTIC)
    if forest.any():
        ridge_hint = np.clip(1.0 - np.abs(n_ridge.fbm(wx, wy, 3600, 4)) * 3.2, 0, 1)
        cold = -wy / g.hy + 0.35 * ridge_hint + 0.22 * n_cold.fbm(wx, wy, 3000, 3)
        # Boreal fringe: never snow right against the dry side's land.
        dry = land & ~forest
        fringe = grid_distance(dry, BOREAL_FRINGE + 100) < BOREAL_FRINGE
        snowable = forest & ~fringe
        cut = np.quantile(cold[snowable], 1.0 - ARCTIC_SHARE / max(0.01, snowable.mean() / forest.mean()))
        biome[forest] = np.where((cold[forest] > cut) & snowable[forest], ARCTIC, PNW)

    # Fold slivers into their neighbours: every piece of a biome is a place.
    min_cells = MIN_FRAGMENT_M2 * METRE * METRE / (TILE * TILE / 2.0)
    holes = np.zeros_like(land)
    for b in BIOMES:
        label, sizes = components(biome == b)
        small = np.where(sizes < min_cells)[0]
        small = small[small > 0]
        if len(small):
            holes |= np.isin(label, small)
    biome = fill_from_neighbours(biome, holes & land)
    biome[~land] = 0
    return biome


def biome_mix(g, biome, x, y, radius, step=260.0):
    """Shares of each biome (0 = sea) inside a disc, by sampling."""
    counts = np.zeros(len(BIOMES) + 1)
    for sx in np.arange(-radius, radius + 1, step):
        for sy in np.arange(-radius, radius + 1, step):
            if sx * sx + sy * sy > radius * radius:
                continue
            tx, ty = g.to_tile(x + sx, y + sy)
            b = biome[ty, tx] if g.inside(tx, ty) else 0
            counts[b] += 1
    return counts / max(1.0, counts.sum())


def choose_sites(g, land, biome, sea_dist, rng, spine_dist, count=6):
    """Candidate crash sites: flat-able land where two or three biomes meet
    inside the round-3 storm, spread across the planet."""
    radius = ZONE_RADII[-1]
    cands = []
    for x in np.arange(-g.hx * 0.8, g.hx * 0.8, 900.0):
        for y in np.arange(-g.hy * 0.8, g.hy * 0.8, 900.0):
            tx, ty = g.to_tile(x, y)
            if not g.inside(tx, ty) or not land[ty, tx]:
                continue
            if sea_dist[ty, tx] < 2400 or biome[ty, tx] == CITY or spine_dist[ty, tx] < 3000:
                continue
            mix = biome_mix(g, biome, x, y, radius, step=500.0)
            sea = mix[0]
            shares = mix[1:]
            real = int((shares >= 0.12).sum())
            if sea > 0.4 or mix[CITY] > 0.35:
                continue
            score = {1: 0.0, 2: 2.0, 3: 3.0, 4: 2.2}.get(real, 1.0)
            score += 1.5 * (1.0 - sea) + rng.uniform(0, 0.3)
            # The default site favours the heart of the continent.
            score -= 0.6 * math.hypot(x / g.hx, y / g.hy)
            cands.append((score, x, y, mix))
    cands.sort(key=lambda c: -c[0])
    sites = []
    for score, x, y, mix in cands:
        if any(math.hypot(x - sx, y - sy) < 7500 for _s, sx, sy, _m in sites):
            continue
        sites.append((score, x, y, mix))
        if len(sites) >= count:
            break
    return [(x, y, mix) for _s, x, y, mix in sites]


def build(seed: int, size: int, site_index: int):
    rng = random.Random(seed)
    g = Grid(size)
    n1, n2, n3, n4, n5, n6, n7, n8 = (Noise(rng) for _ in range(8))
    wx, wy = g.wx, g.wy

    # ---- 1. Land -----------------------------------------------------------
    land, shape = make_land(g, n1)
    sea_dist = grid_distance(~land, 9000.0)
    land_dist = grid_distance(land, SEA_KEEP + 100.0)

    # ---- 2. Biomes -----------------------------------------------------------
    spine = spine_field(g, n4)
    spine_dist, _wet, spine_along = spine
    biome = lay_out_biomes(g, land, sea_dist, (n5, n6, n3), rng, spine)

    # ---- 3. Crash sites --------------------------------------------------
    sites = choose_sites(g, land, biome, sea_dist, random.Random(seed * 7 + 1), spine_dist)
    assert sites, "no crash site found"
    site_index = min(site_index, len(sites) - 1)
    # Every site is a landing (basin, trench, hull, camps, objectives); the
    # server picks one per match. `--site` only sets the map's default ship.
    site_xy = [(x, y) for x, y, _mix in sites]
    ship_x, ship_y = site_xy[site_index]
    ship_dist = np.min([np.hypot(wx - x, wy - y) for x, y in site_xy], axis=0)
    for x, y in site_xy:
        land[g.disc(x, y, BASIN_RADIUS + 400)] = True
    land &= ~border_sea(g, n1)  # a landing basin never reopens the edge

    # ---- 4. Height -----------------------------------------------------------
    params = np.zeros((5, size, size), dtype=np.float32)
    lake_thr = np.full((size, size), 9.0, dtype=np.float32)
    for b, (base, hill, ridge_amp, mesa, lake) in BIOME_RELIEF.items():
        m = biome == b
        params[0][m], params[1][m], params[2][m], params[3][m] = base, hill, ridge_amp, mesa
        lake_thr[m] = 9.0 if lake is None else lake
    params[0][~land] = 2.0
    blend = [blur(params[i], 22) for i in range(4)]
    base, hill_amp, ridge_amp = blend[0], blend[1], blend[2]
    mesa_amp = blur(params[3], 4)          # mesas keep steep edges
    hills = n2.fbm(wx, wy, 1500, 4)
    dunes = n2.fbm(wx * 0.45 + 300, wy * 1.6, 700, 2)
    hills = np.where(biome == DESERT, 0.5 * hills + 0.7 * dunes, hills)
    ridge_n = n3.fbm(wx, wy, 3600, 4)
    ridge = np.clip(1.0 - np.abs(ridge_n) * 3.2, 0, 1) ** 2.2
    ridge *= np.clip((ship_dist - 1600) / 1400, 0, 1)   # none near the ship
    ridge *= np.clip((sea_dist - 300) / 2500, 0, 1)      # none on the shore
    ridge *= ridge_amp / 10.0
    mesa_n = n4.fbm(wx, wy, 1900, 3)
    mesa = (mesa_n > 0.16).astype(np.float32) + (mesa_n > 0.34)
    coast = np.clip(sea_dist / 2200.0, 0, 1) ** 0.6
    # The spine: a crest with rugged peaks, dropping to passes.
    crest = np.exp(-(spine_dist / SPINE_WIDTH) ** 2)
    gap = np.ones_like(crest)
    for tp in SPINE_PASSES:
        gap -= 0.8 * np.exp(-((spine_along - tp) / 0.035) ** 2)
    rugged = 0.7 + 0.5 * np.clip(n3.fbm(wx + 300, wy, 900, 3) + 0.5, 0, 1)
    spine_h = 10.0 * crest * np.clip(gap, 0.1, 1) * rugged
    spine_h *= np.clip((ship_dist - 1800) / 1500, 0, 1)
    spine_h *= np.clip(sea_dist / 1200, 0.25, 1)
    ridge = np.maximum(ridge, np.clip(spine_h / 10.5, 0, 1) * (crest > 0.35))
    height = base + hill_amp * hills + 10.5 * ridge + mesa_amp * mesa
    height = np.maximum(height, base + spine_h)
    height = 2.0 + (height - 2.0) * (0.35 + 0.65 * coast)
    basin = np.clip((ship_dist - BASIN_RADIUS) / 900.0, 0, 1)
    height = BASE_LEVEL + (height - BASE_LEVEL) * basin
    height[~land] = 1.0

    # ---- 5. Water: lakes, marsh, tide pools --------------------------------
    lake_field = n7.fbm(wx, wy, 1500, 3)
    lakes_all = land & (lake_field > lake_thr) & (ship_dist > 1900) & (ridge < 0.2) \
        & (sea_dist > 500)
    frozen = lakes_all & (biome == ARCTIC)            # walkable ice
    lakes = lakes_all & ~frozen                       # deep, impassable
    height[lakes] = np.minimum(height[lakes], 2.4)
    height[frozen] = np.minimum(height[frozen], np.maximum(height[frozen] - 1.5, 3.0))
    marsh = land & (biome == SWAMP) & (lake_field > -0.06) & ~lakes & (ship_dist > 1300)
    height[marsh] = np.minimum(height[marsh], 2.6)
    tidepool = land & (biome == TIDAL) & (sea_dist < 650) & \
        (n8.fbm(wx, wy, 500, 2) > 0.05) & (ship_dist > 1300)
    beach = land & (biome == TIDAL) & (sea_dist < 1500)

    # Terraces: hills step in pairs of levels, so the land reads as plateaus
    # and ledges rather than a blur. Mountains keep every level: rugged.
    terraced = 2.0 * np.rint(height / 2.0)
    height = np.where((ridge < 0.25) & (basin > 0.5), terraced, height)
    level = np.clip(np.rint(height), 0, LEVELS - 1).astype(int)
    level[biome == DESERT] = np.minimum(level[biome == DESERT], 12)  # buttes, not peaks
    level[lakes] = 2
    level[~land] = 1

    # ---- 6. Regions: genus territories nested in biomes --------------------
    region = np.zeros((size, size), dtype=int)
    areas = {b: int((biome == b).sum()) for b in BIOMES}
    total = sum(areas.values())
    want = {b: max(1, round(REGION_SLOTS * a / total)) if a else 0
            for b, a in areas.items()}
    want[CITY] = 1 if areas[CITY] else 0
    while sum(want.values()) > REGION_SLOTS:
        big = max(want, key=lambda b: want[b] if b != CITY else -1)
        want[big] -= 1
    next_region = 1
    for b in BIOMES:
        cells = np.argwhere(biome == b)
        if not len(cells):
            continue
        k = want[b]
        # Farthest-point seeds inside the biome, then nearest-seed cells.
        pick = [cells[rng.randrange(len(cells))]]
        for _ in range(k - 1):
            d = np.min([np.hypot(*(cells - p).T) for p in pick], axis=0)
            pick.append(cells[int(np.argmax(d))])
        cwx = (cells[:, 1] - cells[:, 0]) * HALF_W
        cwy = (cells[:, 1] + cells[:, 0]) * QUARTER_H
        cwx = cwx + 600 * n5.fbm(cwx, cwy, 2000, 2)
        best = np.full(len(cells), np.inf)
        owner = np.zeros(len(cells), dtype=int)
        for i, (py, px) in enumerate(pick):
            pwx, pwy = (px - py) * HALF_W, (px + py) * QUARTER_H
            d = np.hypot(cwx - pwx, cwy - pwy)
            take = d < best
            best[take] = d[take]
            owner[take] = i
        region[cells[:, 0], cells[:, 1]] = next_region + owner
        next_region += k
    assert next_region - 1 <= REGION_SLOTS
    city_id = int(region[biome == CITY].max()) if areas[CITY] else -1

    # ---- 7. The city --------------------------------------------------------
    material = np.full((size, size), MAT["scrub"], dtype=int)
    blocked = np.zeros((size, size), dtype=bool)
    city = land & (biome == CITY) & (sea_dist > 150)
    city_level = 6
    level[city] = city_level
    ridge[city] = 0
    material[city] = MAT["street"]
    props = []
    city_blocks = []
    cx_all, cy_all = wx[city], wy[city]
    city_x = float(cx_all.mean()) if city.any() else 0.0
    city_y = float(cy_all.mean()) if city.any() else 0.0
    out_len = math.hypot(city_x, city_y) or 1.0
    out_x, out_y = city_x / out_len, city_y / out_len
    # A grid of 8 m streets (two lanes and kerbs) around 14 m blocks, at
    # real scale. Some blocks split into two narrower buildings across a 2 m
    # alley.
    street, block = tiles(8.0), tiles(14.0)
    pitch = street + block
    city_span = float(np.max(np.hypot(cx_all - city_x, cy_all - city_y))) if city.any() else 1.0
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
                # Taller toward the city's heart (out by the shore): two
                # storeys at the edge, up to five in the middle. Capped
                # there: buildings hide what walks behind them.
                heart = math.hypot(cwx - (city_x + out_x * city_span * 0.4),
                                   cwy - (city_y + out_y * city_span * 0.4))
                storeys = int(np.clip(round(5.0 - 3.0 * heart / city_span +
                                            rng.uniform(-1.0, 1.0)), 2, 5))
                tall = storeys * STOREY * METRE
                city_blocks.append((cwx, cwy, lx1 - lx0, ly1 - ly0, tall))
                props.append(("city_block", cwx, cwy, {"width": lx1 - lx0,
                                                      "depth": ly1 - ly0,
                                                      "tall": round(tall),
                                                      "variant": rng.randrange(4)}))

    # The skyline: the real city, out past the shore. Drawn, never reached.
    if city.any():
        placed = 0
        for _ in range(4000):
            if placed >= 34:
                break
            t = rng.uniform(0.9, 2.2)
            lat = rng.uniform(-0.9, 0.9) * city_span
            sx = city_x + out_x * city_span * t - out_y * lat
            sy = city_y + out_y * city_span * t + out_x * lat
            tx, ty = g.to_tile(sx, sy)
            if not g.inside(tx, ty) or land[ty, tx] or sea_dist[ty, tx] < 500:
                continue
            props.append(("skyline", sx, sy, {"tall": round(rng.uniform(500, 1500)),
                                              "width": round(rng.uniform(140, 380)),
                                              "variant": rng.randrange(4)}))
            placed += 1

    # ---- 8. The crash trenches: one per landing site ----------------------
    # Ploughed in from whichever side has the most open land. The sites the
    # ship did not land on this match read as old landing scars.
    trench = np.zeros((size, size), dtype=bool)
    crash_dirs = []
    for sx_, sy_ in site_xy:
        best_dir, best_open = 155.0, -1.0
        for deg in range(0, 360, 15):
            a = math.radians(deg)
            open_ = 0
            for k in range(1, 22):
                tx, ty = g.to_tile(sx_ + math.cos(a) * k * 100, sy_ + math.sin(a) * k * 100)
                if g.inside(tx, ty) and land[ty, tx] and biome[ty, tx] != CITY:
                    open_ += 1
            if open_ > best_open + 0.5 or (open_ == best_open and
                                           abs(deg - 155) < abs(best_dir - 155)):
                best_dir, best_open = float(deg), open_
        a = math.radians(best_dir)
        ux, uy = math.cos(a), math.sin(a)
        crash_dirs.append((ux, uy))
        along = (wx - sx_) * ux + (wy - sy_) * uy
        across = -(wx - sx_) * uy + (wy - sy_) * ux
        trench |= (along > 150) & (along < 2100) & (np.abs(across) < 70 + along * 0.05)
    trench &= land & ~city
    level[trench] = np.maximum(level[trench] - 1, 2)

    # ---- Blocked: sea, lakes, peaks ----------------------------------------
    peaks = land & (level >= 13) & ~city
    blocked |= ~land | lakes | peaks

    # ---- Rivers: from the mountain flanks to the water --------------------
    river = np.zeros((size, size), dtype=bool)
    springs = np.argwhere(land & (level >= 10) & (level <= 12) & ~blocked &
                          np.isin(biome, (PNW, ARCTIC, JUNGLE, PLAINS)))
    springs = [tuple(p) for p in springs]
    rng.shuffle(springs)
    made = 0
    for sy_, sx_ in springs:
        if made >= 16:
            break
        wsx, wsy = g.centre(sx_, sy_)
        if min(math.hypot(wsx - x, wsy - y) for x, y in site_xy) < 2200:
            continue
        if river[max(0, sy_ - 30):sy_ + 30, max(0, sx_ - 30):sx_ + 30].any():
            continue
        path, x, y = [], sx_, sy_
        seen = set()
        for _ in range(1600):
            path.append((x, y))
            seen.add((x, y))
            if not land[y, x] or lakes[y, x] or river[y, x]:
                break
            options = [(height[ny, nx] + rng.uniform(0, 0.35), nx, ny)
                       for nx, ny, _c in neighbours8(size, x, y)
                       if (nx, ny) not in seen]
            if not options:
                break
            _h, x, y = min(options)
        else:
            continue
        if len(path) < 60:
            continue
        for x, y in path:
            if land[y, x] and not city[y, x] and not lakes[y, x]:
                river[y, x] = True
                height[y, x] -= 0.6
        made += 1
    river &= ~blocked
    level[river] = np.maximum(level[river] - 1, 2)

    # ---- Connectivity: carve passes so all big land reaches the ship ------
    def reachable_from_ship():
        sx, sy = g.to_tile(ship_x, ship_y + 60)
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

    for _attempt in range(40):
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
                if not land[ny, nx] or lakes[ny, nx] or (city[ny, nx] and blocked[ny, nx]):
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
    for x, y in site_xy:
        tx, ty = g.to_tile(x, y + 60)
        assert reach[ty, tx], f"landing site ({x:.0f}, {y:.0f}) is cut off"


    # ---- Materials ---------------------------------------------------------
    slope = np.zeros_like(height)
    slope[1:-1, 1:-1] = (np.abs(height[1:-1, 2:] - height[1:-1, :-2]) +
                         np.abs(height[2:, 1:-1] - height[:-2, 1:-1]))
    # Ecotones: the ground's biome is read a few tiles off, by noise, so
    # borders are ragged and blended rather than ruled lines.
    jx = np.rint(4.0 * n8.fbm(wx, wy, 260, 2)).astype(int)
    jy = np.rint(4.0 * n8.fbm(wx + 500, wy - 200, 260, 2)).astype(int)
    gb = biome[np.clip(g.ty + jy, 0, size - 1), np.clip(g.tx + jx, 0, size - 1)]
    gb = np.where(gb == 0, biome, gb)
    patch = n1.fbm(wx + 500, wy - 700, 900, 3)
    wild = land & ~city
    pick = {
        PLAINS: np.where(patch > 0.05, MAT["meadow"], MAT["grass"]),
        PNW: np.where(patch > -0.1, MAT["moss"], MAT["needles"]),
        ARCTIC: np.full(patch.shape, MAT["snow"]),
        DESERT: np.where(patch > 0.25, MAT["dust"], MAT["sand"]),
        TIDAL: np.where(patch > 0.0, MAT["scrub"], MAT["sand"]),
        CITY: np.full(patch.shape, MAT["dust"]),
        JUNGLE: np.where(patch > 0.2, MAT["vine"], MAT["jungle"]),
        SWAMP: np.where(patch > 0.1, MAT["marsh"], MAT["mud"]),
    }
    for b, mats in pick.items():
        m = wild & (gb == b)
        material[m] = mats[m]
    rocky = wild & (slope > 1.4)
    material[rocky & (gb != ARCTIC) & (gb != DESERT)] = MAT["rock"]
    material[rocky & (gb == DESERT)] = MAT["redrock"]
    material[wild & (biome == DESERT) & (mesa_amp * mesa > 1.5)] = MAT["redrock"]
    material[wild & (level >= 11) & ~np.isin(gb, (ARCTIC, DESERT))] = MAT["high"]
    material[wild & (level >= 11) & (gb == DESERT)] = MAT["redrock"]
    material[wild & (level >= 11) & (gb == ARCTIC) & (slope > 1.4)] = MAT["high"]
    water = (~land) | lakes | river
    near_water = dilate(dilate(water))
    material[wild & near_water & (level <= 6) &
             np.isin(gb, (PLAINS, PNW, JUNGLE))] = MAT["wet"]
    material[beach & ~city] = MAT["beach"]
    material[tidepool & ~city] = MAT["tidepool"]
    material[marsh] = MAT["bog"]
    material[frozen] = MAT["ice"]
    material[river] = MAT["river"]
    material[lakes] = MAT["water"]
    material[~land] = MAT["sea"]
    material[trench] = MAT["scar"]
    # What nobody can climb or reach looks like a wall (MAP_PIPELINE.md).
    material[(peaks | pockets) & land & ~lakes & ~city & ~trench] = MAT["cliff"]
    crash_ground = np.zeros((size, size), dtype=bool)
    for x, y in site_xy:
        # Ash out to a ragged edge, the pad the ship sits on left as dust.
        d = np.hypot(wx - x, wy - y) + 220.0 * n1.fbm(wx * 1.7, wy * 1.7, 700, 2)
        site_ground = (d <= CRASH_SITE_RADIUS) & land & ~city & ~lakes & ~river
        crash_ground |= site_ground
        material[site_ground & ~trench] = MAT["ash"]
        basin_dust = g.disc(x, y, BASIN_RADIUS * 0.5) & land & ~trench & ~city
        material[basin_dust] = MAT["dust"]

    world = dict(g=g, rng=rng, land=land, level=level, region=region, biome=biome,
                 blocked=blocked, material=material, city=city, lakes=lakes,
                 river=river, trench=trench, peaks=peaks, ridge=ridge,
                 near_water=near_water | marsh | tidepool, props=props,
                 city_id=city_id, city_blocks=city_blocks, slope=slope,
                 crash=crash_dirs[site_index], ship=(ship_x, ship_y), sites=sites,
                 site_xy=site_xy, crash_dirs=crash_dirs,
                 site_index=site_index, sea_dist=sea_dist, marsh=marsh,
                 frozen=frozen, tidepool=tidepool, beach=beach,
                 land_dist=land_dist, crash_ground=crash_ground)
    # Each landing site gets its own rings of objectives, landmarks, trails
    # home and lifeline rocks. Rings are placed across all sites one ring at
    # a time, so every site's near objectives come before anyone's far ones.
    world["landmarks"], world["pois"], world["rocks"] = [], [], []
    world["trail"] = np.zeros((size, size), dtype=bool)

    def at_site(i):
        world["ship"], world["crash"], world["site_now"] = site_xy[i], crash_dirs[i], i

    for i in range(len(site_xy)):
        at_site(i)
        place_landmarks(world)
    for ring in RINGS:
        for i in range(len(site_xy)):
            at_site(i)
            place_pois(world, ring)
    fill_coverage(world)
    for i in range(len(site_xy)):
        at_site(i)
        cut_trails(world)
        place_rocks(world)
    at_site(site_index)
    dress(world)
    return world


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
    sx, sy = w["ship"]
    tries = 0
    want = list(LANDMARKS[:LANDMARKS_PER_SITE])
    while want and tries < 5000:
        tries += 1
        a = rng.uniform(-math.pi, math.pi)
        r = rng.uniform(1800, 5200)
        x, y = sx + math.cos(a) * r * 1.2, sy + math.sin(a) * r * 0.85
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


def place_pois(w, ring_spec):
    g, rng = w["g"], w["rng"]
    sx, sy = w["ship"]
    site = w["site_now"]
    pois = w["pois"]
    level, near_water = w["level"], w["near_water"]

    def context_score(kind, x, y):
        tx, ty = g.to_tile(x, y)
        lvl = level[ty, tx]
        s = rng.uniform(0, 1.0)
        b = int(w["biome"][ty, tx])
        in_city = b == CITY
        base_kind = kind.split("_")[0]
        s += POI_BIOME.get(base_kind, {}).get(b, 0.0)
        if kind in ("frogs", "gas"):
            s += 3.0 if near_water[ty, tx] else 0.0
            s += 1.0 if lvl <= 5 else 0.0
        if kind.startswith("outpost"):
            s += 0.35 * lvl
        if kind.startswith("scrapyard"):
            s += 3.0 if (w["trench"][ty, tx] or in_city) else 0.0
            ux, uy = w["crash"]
            if (x - sx) * ux + (y - sy) * uy > 0 and abs(-(x - sx) * uy + (y - sy) * ux) < 500:
                s += 2.0
        if kind == "probe":
            s += 0.25 * lvl + (1.5 if w["ridge"][ty, tx] > 0.1 else 0.0)
        if kind == "littlejohn":
            for _k, lx, ly in w["landmarks"]:
                if math.hypot(lx - x, ly - y) < 900:
                    s += 3.0
        return s

    others = [p for i, p in enumerate(w["site_xy"]) if i != site]

    def ring_full(ox, oy, d):
        for _name, q0, q1, quota in RINGS:
            if q0 <= d <= q1:
                return sum(1 for _k, px, py, _r, _s in pois
                           if q0 <= math.hypot(px - ox, py - oy) <= q1) >= len(quota) + 3
        return False
    for ring, r0, r1, kinds in [ring_spec]:
        # Shared ground: objectives already in this band (placed around a
        # neighbouring site) count toward this ring, so every site sees
        # about the same number in play.
        have = sum(1 for _k, px, py, _r, _s in pois
                   if r0 <= math.hypot(px - sx, py - sy) <= r1)
        for kind in kinds[have:]:
            pad = POI_KINDS[kind][5]
            best = None
            for _ in range(500):
                a = rng.uniform(-math.pi, math.pi)
                r = rng.uniform(r0, r1)
                x, y = sx + math.cos(a) * r * 1.25, sy + math.sin(a) * r * 0.9
                d = math.hypot(x - sx, y - sy)
                if d < r0 or d > r1:
                    continue
                # Never inside another site's near ring (that is its own), and
                # never where it would push any site's ring past its count.
                if any(math.hypot(x - ox, y - oy) < 2500.0 for ox, oy in others):
                    continue
                if any(ring_full(ox, oy, math.hypot(x - ox, y - oy)) for ox, oy in others):
                    continue
                if not walkable_spot(w, x, y, pad + 40):
                    continue
                if any(math.hypot(x - px, y - py) < POI_SPACING for _k, px, py, _r, _s in pois):
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
            pois.append((kind, x, y, ring, site))
            flatten(w, x, y, pad, material=MAT["pad"])


def poi_in_play(w, poi, site):
    """Whether an objective is in play when the ship lands on `site`."""
    kind, x, y, ring, home = poi
    if ring == "fill":
        return True
    px, py = w["site_xy"][site]
    return math.hypot(x - px, y - py) <= POI_REACH


def coverage_gaps(w, stride=8):
    """Sample walkable tiles; for each, the worst (over landing sites)
    straight-line distance to an objective in play. Returns (xs, ys, gap)."""
    g = w["g"]
    ty, tx = np.mgrid[0:g.size:stride, 0:g.size:stride]
    keep = w["land"][ty, tx] & ~w["blocked"][ty, tx]
    xs, ys = g.wx[ty, tx][keep], g.wy[ty, tx][keep]
    worst = np.zeros(xs.shape, dtype=np.float32)
    for site in range(len(w["site_xy"])):
        live = [(x, y) for (k, x, y, r, h) in w["pois"]
                if poi_in_play(w, (k, x, y, r, h), site)]
        if not live:
            worst[:] = np.inf
            continue
        best = np.full(xs.shape, np.inf, dtype=np.float32)
        for x, y in live:
            best = np.minimum(best, np.hypot(xs - x, ys - y))
        worst = np.maximum(worst, best)
    return xs, ys, worst


def fill_coverage(w):
    """Objectives in the empty stretches the rings left (issue #49)."""
    g, rng = w["g"], w["rng"]
    tried = set()
    placed = 0
    while placed < COVER_MAX:
        xs, ys, gap = coverage_gaps(w)
        order = np.argsort(-gap)
        target = None
        for i in order[:400]:
            if gap[i] <= COVER_GAP:
                break
            key = (int(xs[i]) // 600, int(ys[i]) // 600)
            if key not in tried:
                target = (float(xs[i]), float(ys[i]), key)
                break
        if target is None:
            break
        gx, gy, key = target
        tried.add(key)
        tx, ty = g.to_tile(gx, gy)
        kinds = FILL_KINDS.get(int(w["biome"][ty, tx]), ["outpost", "medpacks"])
        best = None
        for _ in range(700):
            a = rng.uniform(-math.pi, math.pi)
            r = 900.0 * math.sqrt(rng.uniform(0, 1))
            x, y = gx + math.cos(a) * r, gy + math.sin(a) * r * 0.75
            ctx, cty = g.to_tile(x, y)
            if not g.inside(ctx, cty):
                continue
            here = int(w["biome"][cty, ctx])
            kind = next((k for k in FILL_KINDS.get(here, kinds)
                         if not (w["city"][cty, ctx] and k in ("frogs", "gas", "probe"))),
                        None)
            if kind is None:
                continue
            pad = POI_KINDS[kind][5]
            if not walkable_spot(w, x, y, pad + 40):
                continue
            if any(math.hypot(x - px, y - py) < POI_SPACING for _k, px, py, _r, _s in w["pois"]):
                continue
            if any(math.hypot(x - lx, y - ly) < 380 for _k, lx, ly in w["landmarks"]):
                continue
            if any(math.hypot(x - sx, y - sy) < 1150.0 for sx, sy in w["site_xy"]):
                continue
            # Closest to the middle of the gap wins; biome fit breaks ties.
            score = -math.hypot(x - gx, y - gy) + 120.0 * POI_BIOME.get(
                kind.split("_")[0], {}).get(here, 0.0)
            if best is None or score > best[0]:
                best = (score, kind, x, y)
        if best is None:
            continue
        _score, kind, x, y = best
        home = min(range(len(w["site_xy"])),
                   key=lambda i: math.hypot(x - w["site_xy"][i][0], y - w["site_xy"][i][1]))
        w["pois"].append((kind, x, y, "fill", home))
        flatten(w, x, y, POI_KINDS[kind][5], material=MAT["pad"])
        placed += 1
    xs, ys, gap = coverage_gaps(w)
    w["coverage"] = (placed, float(np.median(gap)), float(np.percentile(gap, 95)),
                     float(gap.max()))


def cut_trails(w):
    """One shortest-path tree from the ship: every trail runs home."""
    g = w["g"]
    size = g.size
    level, blocked = w["level"], w["blocked"]
    sx, sy = g.to_tile(w["ship"][0], w["ship"][1] + 60)
    cost = {(sx, sy): 0.0}
    prev = {}
    heap = [(0.0, sx, sy)]
    limit = 11000.0 / TILE_STEP_PX * 6.0   # far enough for every ring
    while heap:
        c, x, y = heapq.heappop(heap)
        if c > cost.get((x, y), 1e18) or c > limit:
            continue
        for nx, ny, step in neighbours8(size, x, y):
            if blocked[ny, nx]:
                continue
            climb = abs(int(level[ny, nx]) - int(level[y, x]))
            # Trails prefer flat ground, reuse each other (cheap on path),
            # and avoid the rivers' and marshes' wet feet when they can.
            k = step * (1.0 + 1.6 * climb * climb)
            if w["material"][ny, nx] == MAT["path"]:
                k *= 0.6
            if w["river"][ny, nx] or w["marsh"][ny, nx]:
                k *= 1.6
            nc = c + k
            if nc < cost.get((nx, ny), 1e18):
                cost[(nx, ny)] = nc
                prev[(nx, ny)] = (x, y)
                heapq.heappush(heap, (nc, nx, ny))
    trail = np.zeros((size, size), dtype=bool)
    for _kind, x, y, _ring, site in w["pois"]:
        if site != w["site_now"]:
            continue
        node = g.to_tile(x, y)
        while node in prev:
            trail[node[1], node[0]] = True
            node = prev[node]
    for kind, x, y in w["landmarks"]:
        if math.hypot(x - w["ship"][0], y - w["ship"][1]) > 6000:
            continue
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
    w["trail"] = w["trail"] | wide


def place_rocks(w):
    """Lifeline rocks (Terrain layer, type "terrain"): the place_terrain.py
    rules around the ship. Centres about SPACING apart so Isaac can chain
    them; clear of the ship ring, the pads, trails and spawns."""
    g, rng = w["g"], w["rng"]
    sx, sy = w["ship"]
    spacing = 560.0
    zone = ZONE_RADII[-1] + 400
    keepout = [(x, y, SHIP_CLEAR) for x, y in w["site_xy"]]
    for kind, x, y, _ring, _site in w["pois"]:
        keepout.append((x, y, POI_KINDS[kind][5] + 120))
    for _k, x, y in w["landmarks"]:
        keepout.append((x, y, 360))
    rocks = w["rocks"]
    for _ in range(20000):
        x = sx + rng.uniform(-zone * 1.3, zone * 1.3)
        y = sy + rng.uniform(-zone, zone)
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


# Vegetation per biome: (kind, count per hectare of the biome, min gap m,
# cluster size, cluster spread m, which ground). Real sizes are in
# MAP_PIPELINE.md, "Biomes". Looks only: nothing here blocks movement.
VEGETATION = {
    PNW: [("conifer", 260, 4.5, 6, 9.0, "soft"), ("boulder", 14, 6.0, 2, 3.0, "rock"),
          ("flora", 14, 3.0, 3, 3.0, "soft")],
    ARCTIC: [("snow_conifer", 110, 5.0, 4, 8.0, "soft"), ("boulder", 18, 6.0, 2, 3.0, "rock"),
             ("flora", 10, 3.0, 3, 3.0, "soft")],
    DESERT: [("cactus", 45, 6.0, 2, 6.0, "soft"), ("boulder", 22, 6.0, 2, 3.0, "rock"),
             ("bones", 4, 25.0, 1, 0.0, "soft"), ("flora", 8, 3.0, 2, 3.0, "soft")],
    TIDAL: [("palm", 60, 5.0, 3, 6.0, "soft"), ("reeds", 90, 2.5, 4, 3.0, "wet"),
            ("boulder", 16, 6.0, 2, 3.0, "rock"), ("flora", 10, 3.0, 3, 3.0, "soft")],
    JUNGLE: [("jungle_tree", 230, 5.5, 5, 9.0, "soft"), ("flora", 30, 3.0, 4, 3.0, "soft"),
             ("boulder", 10, 6.0, 2, 3.0, "rock")],
    SWAMP: [("cypress", 130, 4.5, 4, 6.0, "any"), ("reeds", 220, 2.0, 6, 3.0, "wet"),
            ("flora", 22, 3.0, 4, 3.0, "soft")],
    PLAINS: [("grass", 200, 2.5, 6, 4.0, "soft"), ("boulder", 8, 8.0, 2, 3.0, "rock"),
             ("flora", 12, 3.0, 3, 3.0, "soft")],
}


def dress(w):
    """Set dressing: props that make each biome a place. Looks only; kept
    off pads and trails so fights stay readable (Clarity)."""
    g, rng = w["g"], w["rng"]
    props = w["props"]
    sx, sy = w["ship"]
    keepout = [(x, y, SHIP_CLEAR + 80) for x, y in w["site_xy"]]
    for kind, x, y, _ring, _site in w["pois"]:
        keepout.append((x, y, POI_KINDS[kind][5] + 60))
    for k, x, y in w["landmarks"]:
        keepout.append((x, y, LANDMARK_PAD[k] + 60))
    rocks = w["rocks"]
    # Spatial hash of keepouts, rocks and placed props: the planet is big.
    bucket = 600.0
    keep_hash = {}
    for kx, ky, kr in keepout:
        for bx in range(int((kx - kr) // bucket), int((kx + kr) // bucket) + 1):
            for by in range(int((ky - kr) // bucket), int((ky + kr) // bucket) + 1):
                keep_hash.setdefault((bx, by), []).append((kx, ky, kr))
    rock_hash = {}
    for rx, ry, rw, rh in rocks:
        rock_hash.setdefault((int(rx // bucket), int(ry // bucket)), []).append((rx, ry, rw, rh))

    def free(x, y):
        key = (int(x // bucket), int(y // bucket))
        if any(math.hypot(x - kx, y - ky) < kr for kx, ky, kr in keep_hash.get(key, ())):
            return False
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for rx, ry, rw, rh in rock_hash.get((key[0] + dx, key[1] + dy), ()):
                    if abs(x - rx) < rw / 2 + 30 and abs(y - ry) < rh / 2 + 30:
                        return False
        tx, ty = g.to_tile(x, y)
        if not g.inside(tx, ty) or w["blocked"][ty, tx] or w["trail"][ty, tx]:
            return False
        return True

    def scatter(kind, count, where, min_gap, cluster=1, spread=0.0):
        placed = {}
        cells = np.argwhere(where)
        if len(cells) == 0 or count <= 0:
            return
        cell = max(min_gap, 1.0)
        n = 0
        for _ in range(count * 4):
            if n >= count:
                break
            cy_, cx_ = cells[rng.randrange(len(cells))]
            bx, by = g.centre(cx_, cy_)
            for _c in range(cluster):
                x = bx + rng.uniform(-spread, spread)
                y = by + rng.uniform(-spread, spread) * 0.6
                tx, ty = g.to_tile(x, y)
                if not g.inside(tx, ty) or not where[ty, tx] or not free(x, y):
                    continue
                kx, ky = int(x // cell), int(y // cell)
                if any(math.hypot(x - px, y - py) < min_gap
                       for ddx in (-1, 0, 1) for ddy in (-1, 0, 1)
                       for px, py in placed.get((kx + ddx, ky + ddy), ())):
                    continue
                placed.setdefault((kx, ky), []).append((x, y))
                n += 1
                props.append((kind, x, y, {"variant": rng.randrange(4),
                                           "scale": round(rng.uniform(0.75, 1.3), 2)}))

    land, mat = w["land"], w["material"]
    biome = w["biome"]
    # The crash flattened everything that grew there: wreckage only.
    wild = land & ~w["city"] & ~w["blocked"] & ~w["crash_ground"]
    soft = ~np.isin(mat, (MAT["rock"], MAT["high"], MAT["redrock"], MAT["path"],
                          MAT["pad"], MAT["scar"], MAT["river"], MAT["ice"],
                          MAT["tidepool"], MAT["bog"], MAT["beach"]))
    rock = np.isin(mat, (MAT["rock"], MAT["high"], MAT["redrock"]))
    wet = np.isin(mat, (MAT["bog"], MAT["wet"], MAT["tidepool"], MAT["mud"]))
    tile_ha = (TILE * TILE / 2.0) / (METRE * METRE) / 1e4
    for b, kinds in VEGETATION.items():
        inside = wild & (biome == b)
        ha = inside.sum() * tile_ha
        for kind, per_ha, gap, cluster, spread, ground in kinds:
            where = inside & {"soft": soft, "rock": rock, "wet": wet,
                              "any": soft | wet}[ground]
            scatter(kind, int(per_ha * ha), where, gap * METRE, cluster=cluster,
                    spread=spread * METRE)
    # Sea stacks off the tidal coast: rock pillars standing in the surf.
    surf = ~land & (w["sea_dist"] < 600)
    tidal_near = dilate(dilate(biome == TIDAL))
    for _ in range(3):
        tidal_near = dilate(tidal_near)
    near = surf & blur(tidal_near.astype(np.float32), 18, 1).astype(bool)
    cells = np.argwhere(near)
    for _ in range(min(60, len(cells))):
        cy_, cx_ = cells[rng.randrange(len(cells))]
        x, y = g.centre(cx_, cy_)
        props.append(("sea_stack", x, y, {"variant": rng.randrange(4),
                                          "scale": round(rng.uniform(0.7, 1.4), 2)}))
    # Crash wreckage: pieces ~2 m across, with room to walk between them.
    scatter("debris", 70, w["trench"] & ~w["blocked"], 4.0 * METRE, cluster=3,
            spread=6.0 * METRE)
    # ... and thrown wide across the crash site, so it reads as its own place.
    scatter("debris", 90, w["crash_ground"] & ~w["trench"] & ~w["blocked"],
            9.0 * METRE, cluster=2, spread=5.0 * METRE)
    scatter("ruin", 260, wild & (w["level"] >= 7) & ~np.isin(biome, (SWAMP, TIDAL)),
            300, cluster=3, spread=160)
    # City dressing: a lamp every ~15 m of street, and the odd neon sign.
    streets = w["city"] & ~w["blocked"]
    street_ha = streets.sum() * tile_ha
    scatter("lamp", int(24 * street_ha), streets, 15.0 * METRE)
    scatter("sign", int(10 * street_ha), streets, 22.0 * METRE)


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
    elif name == "cliff":
        # Heavy vertical cracks: a rock face, not a slope.
        for _ in range(2 + variant % 2):
            x0, y0 = rng.uniform(16, 48), rng.uniform(20, 30)
            line(x0, y0, x0 + rng.uniform(-3, 3), y0 + rng.uniform(10, 16), seam, 2)
    elif name in ("rock", "high", "redrock", "ice"):
        for _ in range(1 + variant % 3):
            x0, y0 = rng.uniform(14, 50), rng.uniform(22, 42)
            line(x0, y0, x0 + rng.uniform(-12, 12), y0 + rng.uniform(-5, 5), seam)
    elif name in ("sand", "beach", "snow"):
        # Ripples and drifts: soft lines along one axis.
        for _ in range(1 + variant % 2):
            x0, y0 = rng.uniform(12, 40), rng.uniform(24, 40)
            line(x0, y0, x0 + rng.uniform(10, 18), y0 + rng.uniform(-2, 2), seam)
    elif name == "ash":
        # Scorch streaks, all one way: thrown out by the landing.
        for _ in range(1 + variant % 2):
            x0, y0 = rng.uniform(14, 40), rng.uniform(24, 40)
            line(x0, y0, x0 + rng.uniform(12, 20), y0 + rng.uniform(-3, 1), seam, 2)
    elif name in ("dust", "pad", "path", "scrub", "wet", "grass", "meadow", "moss",
                  "needles", "jungle", "mud", "vine"):
        if variant >= 2:
            x0, y0 = rng.uniform(16, 48), rng.uniform(24, 40)
            line(x0, y0, x0 + rng.uniform(-10, 10), y0 + rng.uniform(-4, 4), seam)
    elif name in ("water", "sea", "river", "tidepool", "bog"):
        for _ in range(2):
            x0, y0 = rng.uniform(16, 44), rng.uniform(26, 38)
            line(x0, y0, x0 + rng.uniform(6, 14), y0, speck)
    busy = 2 if name in ("scrub", "scar", "grass", "meadow", "needles", "jungle", "vine") else 1
    for _ in range((variant % 2) + busy):
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


DATA_ROWS = 4   # height, region, blocked, biome


def build_data_tileset():
    """terrain_data.png: what the hidden layers paint with in Tiled.
    Row 0: height 0..15 (dark to light). Row 1: region 0..15. Row 2:
    blocked. Row 3: biome 0..15 (in the biome's legend colour)."""
    sheet = Image.new("RGBA", (TILE * 16, TILE * DATA_ROWS), (0, 0, 0, 0))
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
        poly3 = [(px, py + 3 * TILE) for px, py in poly]
        colour = BIOMES[i][2] if i in BIOMES else (60, 60, 70)
        d.polygon(poly3, fill=colour + (170,))
        d.text((x + 26, 3 * TILE + 26), str(i), fill=(0, 0, 0, 255), font=font)
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


def encode_layer(values):
    """Tiled's base64 + zlib layer data: little-endian uint32 gids."""
    raw = np.ascontiguousarray(values.astype("<u4")).tobytes()
    return base64.b64encode(zlib.compress(raw, 9)).decode("ascii")


def write_tmx(w, path, seed, size_arg):
    g = w["g"]
    size, cy = g.size, g.cy
    n_ground = len(MATERIALS) * VARIANTS
    data_gid = n_ground + 1
    rng = np.random.default_rng(seed * 31 + 5)
    ship_x, ship_y = w["ship"]

    roll = rng.random((size, size))
    variant = np.where(roll < 0.5, 0, np.where(roll < 0.85, 1, np.where(roll < 0.95, 2, 3)))
    ground = w["material"] * VARIANTS + variant + 1
    far_sea = ~w["land"] & (w["land_dist"] >= SEA_KEEP)
    ground[far_sea] = 0                       # the void past the sea: not drawn
    height = data_gid + w["level"]
    region = np.where(w["region"] > 0, data_gid + 16 + w["region"], 0)
    blocked = np.where(w["blocked"], data_gid + 32, 0)
    biome = np.where(w["biome"] > 0, data_gid + 48 + w["biome"], 0)

    next_id = 1
    groups = {}

    def add(layer, xml):
        groups.setdefault(layer, []).append(xml)

    # The default ship (one-site tools and the editor); the server moves it
    # to a landing site every match.
    add("Ship", point_object(next_id, "The Ship", ship_x, ship_y, cy)); next_id += 1
    hull = ship_collision_from_arena(cy)
    for site, (sx_, sy_) in enumerate(w["site_xy"]):
        tag = ("site", "int", str(site))
        for i in range(4):
            a = math.pi / 2 + i * math.pi / 2
            add("PlayerSpawns", point_object(next_id, f"S{site}_Spawn_{i + 1}",
                                             sx_ + math.cos(a) * 150,
                                             sy_ + math.sin(a) * 110 + 40 + 110, cy, [tag]))
            next_id += 1
        # The ship's own objectives come with it: only at its own site.
        for i, angle_deg in enumerate((225.0, 45.0)):
            a = math.radians(angle_deg)
            add("Objectives", point_object(next_id, f"S{site}_Turret_{i + 1}",
                                           sx_ + math.cos(a) * 420,
                                           sy_ + math.sin(a) * 420, cy, [
                ("objective_type", "string", "derelict_turret"),
                ("description", "string",
                 "Repair the derelict turret with scrap: the noise draws a pack"),
                # Quick: the price is scrap, not time (Tuning::TURRET_REPAIR_*,
                # which the server applies whatever this says). The pack the
                # noise draws arrives after, to fight off on the way home.
                ("radius", "float", "110.0"), ("interaction_time", "float", "1.0"),
                ("sites", "string", str(site))]))
            next_id += 1
        # The hull: solid only at the site the ship stands on ("hull@<site>").
        for name, typ, x, y, wdt, hgt in hull:
            typ = f"{typ}@{site}" if typ.startswith("hull") else typ
            add("Collision", f'    <object id="{next_id}" name="S{site}_{name}" type="{typ}" '
                             f'x="{x + sx_:.1f}" y="{y + sy_ + cy:.1f}" '
                             f'width="{wdt:g}" height="{hgt:g}" />')
            next_id += 1

    counts = {}
    for kind, x, y, ring, site in w["pois"]:
        # Fixed on the planet: in play for every landing site in reach. The
        # camp's size was set by the site it was placed around (its ring).
        sx_, sy_ = w["site_xy"][site]
        reach = [i for i in range(len(w["site_xy"]))
                 if poi_in_play(w, (kind, x, y, ring, site), i)]
        assert site in reach
        tag = ("sites", "string", ",".join(str(i) for i in reach))
        otype, desc, radius, itime, (grunts, elites), _pad = POI_KINDS[kind]
        counts[kind] = counts.get(kind, 0) + 1
        name = f"{kind.title().replace('_', '')}_{counts[kind]}"
        props = [("objective_type", "string", otype), ("description", "string", desc),
                 ("radius", "float", f"{radius:.1f}"),
                 ("interaction_time", "float", f"{itime:.1f}"), tag]
        if ring == "far" and grunts:
            grunts += 1
        camp = grunts + elites
        if otype in ("capture_outpost", "salvage_medpacks"):
            props.append(("enemies_required", "int", str(camp)))
        if otype == "save_the_frogs":
            dx, dy = x - sx_, y - sy_
            d = math.hypot(dx, dy)
            px, py = x - dx / d * 260.0, y - dy / d * 260.0
            props += [("enemies_required", "int", "3"),
                      ("deposit_x", "float", f"{px:.1f}"),
                      ("deposit_y", "float", f"{py + cy:.1f}")]
        add("Objectives", point_object(next_id, name, x, y, cy, props)); next_id += 1
        for i in range(camp):
            elite = i >= grunts
            if elite:
                gx, gy = x, y + (0.0 if grunts == 0 else 12.0)
            else:
                a = 2 * math.pi * i / max(1, grunts) + random.Random(next_id).uniform(-0.3, 0.3)
                ring_r = (radius * 0.6 if otype in ("capture_outpost", "salvage_medpacks")
                          else radius + 40.0)
                gx, gy = x + math.cos(a) * ring_r, y + math.sin(a) * ring_r * 0.8
            camp_props = [("enemy_type", "string", "venoxarid_grunt"), tag]
            if elite:
                camp_props.append(("elite", "bool", "true"))
            add("EnemySpawns", point_object(next_id, f"{name}_guard_{i + 1}", gx, gy, cy,
                                            camp_props))
            next_id += 1

    for i, (x, y, rw, rh) in enumerate(sorted(w["rocks"], key=lambda r: (r[1], r[0]))):
        add("Terrain", f'    <object id="{next_id}" name="Rock_{i + 1}" type="terrain" '
                       f'x="{x - rw / 2:.1f}" y="{y - rh / 2 + cy:.1f}" width="{rw}" height="{rh}" />')
        next_id += 1
    for i, (kind, x, y, props) in enumerate(w["props"]):
        add("Props", prop_xml(next_id, f"{kind}_{i + 1}", kind, x, y, cy, props))
        next_id += 1
    # The landing sites: the server picks one per match (never the last).
    for i, (x, y, mix) in enumerate(w["sites"]):
        names = ",".join(f"{BIOMES[b][0]}:{mix[b]:.2f}" for b in BIOMES if mix[b] >= 0.05)
        add("CrashSites", point_object(next_id, f"Site_{i}", x, y, cy, [
            ("biomes", "string", names)]))
        next_id += 1

    order = ["Ship", "PlayerSpawns", "Objectives", "EnemySpawns", "Collision", "Terrain",
             "Props", "CrashSites"]
    xml_groups = []
    layer_id = 6
    for name in order:
        items = groups.get(name, [])
        xml_groups.append(f'  <objectgroup id="{layer_id}" name="{name}">\n' +
                          "\n".join(items) + "\n  </objectgroup>")
        layer_id += 1

    def layer(lid, name, values, visible=True):
        vis = "" if visible else ' visible="0"'
        return (f'  <layer id="{lid}" name="{name}" width="{size}" height="{size}"{vis}>\n'
                f'    <data encoding="base64" compression="zlib">\n'
                f'{encode_layer(values)}\n    </data>\n  </layer>')

    biome_names = ";".join(f"{b}={BIOMES[b][0]}" for b in BIOMES)
    tmx = f'''<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.10.2" orientation="isometric" renderorder="right-down" width="{size}" height="{size}" tilewidth="{TILE}" tileheight="{TILE}" infinite="0" nextlayerid="{layer_id}" nextobjectid="{next_id}">
  <properties>
    <property name="generator" value="tools/maps/generate_planet.py --seed {seed} --size {size_arg} --site {w['site_index']}" />
    <property name="zone_scale" type="float" value="{ZONE_SCALE}" />
    <property name="biomes" value="{biome_names}" />
  </properties>
  <tileset firstgid="1" name="planet_tiles" tilewidth="{TILE}" tileheight="{TILE}" tilecount="{n_ground}" columns="{VARIANTS}">
    <image source="planet_tiles.png" width="{TILE * VARIANTS}" height="{TILE * len(MATERIALS)}" />
  </tileset>
  <tileset firstgid="{data_gid}" name="terrain_data" tilewidth="{TILE}" tileheight="{TILE}" tilecount="{16 * DATA_ROWS}" columns="16">
    <image source="terrain_data.png" width="{TILE * 16}" height="{TILE * DATA_ROWS}" />
  </tileset>
{layer(1, "Ground", ground)}
{layer(2, "Height", height, False)}
{layer(3, "Region", region, False)}
{layer(4, "Blocked", blocked, False)}
{layer(5, "Biome", biome, False)}
{chr(10).join(xml_groups)}
</map>
'''
    path.write_text(tmx)
    return next_id


# ---------------------------------------------------------------------------
# Stats: is every biome big enough? (the music test)
# ---------------------------------------------------------------------------
def crossing_stats(w, samples=3000, seed=11):
    """Straight runs through each biome at hero speed. From a random walkable
    point, run both ways along a random heading until the biome changes (or
    the ground stops): the chord's length is one run through the biome.
    Returns {biome: (median s, 25th percentile s, share of land)}."""
    g = w["g"]
    rng = random.Random(seed)
    biome, blocked, land = w["biome"], w["blocked"], w["land"]
    cells = np.argwhere(land & ~blocked & (biome > 0))
    runs = {b: [] for b in BIOMES}
    step = 60.0
    for _ in range(samples):
        cy_, cx_ = cells[rng.randrange(len(cells))]
        b = int(biome[cy_, cx_])
        x0, y0 = g.centre(cx_, cy_)
        a = rng.uniform(0, math.pi)
        ux, uy = math.cos(a), math.sin(a)
        total = 0.0
        for sgn in (1, -1):
            t = 0.0
            while t < 60000:
                t += step
                tx, ty = g.to_tile(x0 + sgn * ux * t, y0 + sgn * uy * t)
                if not g.inside(tx, ty) or biome[ty, tx] != b or not land[ty, tx]:
                    break
            total += t
        runs[b].append(total / HERO_SPEED)
    land_cells = max(1, int((biome > 0).sum()))
    out = {}
    for b, r in runs.items():
        share = float((biome == b).sum()) / land_cells
        if r:
            out[b] = (float(np.median(r)), float(np.percentile(r, 25)), share)
        else:
            out[b] = (0.0, 0.0, share)
    return out


# ---------------------------------------------------------------------------
# Preview: the whole planet from above, for review (not used by the game)
# ---------------------------------------------------------------------------
# Preview colours are the real tile colours, brightened evenly (the preview
# is small and has no lighting), so it never flatters the ground.
MAT_PREVIEW = {m[0]: tuple(min(255, int(c * 1.7)) for c in m[1]) for m in MATERIALS}


def _project(w, scale, top):
    g = w["g"]
    lift = (w["level"] - BASE_LEVEL) * LEVEL_PIXELS
    px = ((g.wx + g.hx) * scale).astype(int)
    py = ((g.wy - lift + g.hy) * scale + top).astype(int)
    return px, py


def _paint(img_arr, px, py, colours, order):
    h, w_ = img_arr.shape[:2]
    for dy in (0, 1):
        for dx in (-1, 0, 1):
            x = np.clip(px[order] + dx, 0, w_ - 1)
            y = np.clip(py[order] + dy, 0, h - 1)
            img_arr[y, x] = colours[order]


def preview(w, path):
    g = w["g"]
    scale = 1800.0 / (2 * g.hx)
    top = 40
    W, H = int(2 * g.hx * scale), int(2 * g.hy * scale) + top + 30
    px, py = _project(w, scale, top)
    order = np.argsort((g.tx + g.ty).ravel(), kind="stable")
    px, py = px.ravel(), py.ravel()
    lvl = w["level"].ravel()
    shade = (0.72 + 0.035 * lvl)[:, None]

    def to_px(x, y):
        tx, ty = g.to_tile(x, y)
        lift = (w["level"][ty, tx] - BASE_LEVEL) * LEVEL_PIXELS if g.inside(tx, ty) else 0
        return (x + g.hx) * scale, (y - lift + g.hy) * scale + top

    stats = crossing_stats(w)

    # --- Terrain view -------------------------------------------------------
    palette = np.array([MAT_PREVIEW[m[0]] for m in MATERIALS], dtype=np.float32)
    col = palette[w["material"].ravel()]
    rock_blocked = (w["blocked"] & w["land"] & ~w["lakes"]).ravel()
    col[rock_blocked] = (40, 38, 46)
    col = np.clip(col * shade, 0, 255).astype(np.uint8)
    arr = np.zeros((H, W, 3), dtype=np.uint8)
    arr[:] = (6, 8, 14)
    _paint(arr, px, py, col, order)
    img = Image.fromarray(arr)
    d = ImageDraw.Draw(img)
    _overlay(w, d, to_px, scale, terrain=True)
    d.text((8, 6), "planet terrain: pads (near black / mid yellow / far orange), landmarks "
                   "pink, round-3 storm around every landing site blue", fill=(220, 220, 220))
    img.save(path)

    # --- Biome view -------------------------------------------------------
    bpal = np.zeros((16, 3), dtype=np.float32)
    bpal[0] = (14, 22, 38)
    for b, (_k, _n, c) in BIOMES.items():
        bpal[b] = c
    col = bpal[w["biome"].ravel()] * 0.62
    col[rock_blocked] *= 0.5
    waterish = (w["lakes"] | w["river"]).ravel()
    col[waterish] = (30, 60, 96)
    col[(w["marsh"] | w["tidepool"] | w["frozen"]).ravel()] *= 0.8
    col = np.clip(col * shade, 0, 255).astype(np.uint8)
    arr = np.zeros((H, W, 3), dtype=np.uint8)
    arr[:] = (6, 8, 14)
    _paint(arr, px, py, col, order)
    # Biome borders, bright.
    biome = w["biome"]
    edge = (biome > 0) & ((biome != shift(biome, 1, 0, 0)) | (biome != shift(biome, 0, 1, 0))) \
        & (shift(biome, 1, 0, 0) > 0) & (shift(biome, 0, 1, 0) > 0)
    ey, ex = np.nonzero(edge)
    arr[np.clip(py.reshape(biome.shape)[ey, ex], 0, H - 1),
        np.clip(px.reshape(biome.shape)[ey, ex], 0, W - 1)] = (235, 235, 245)
    img = Image.fromarray(arr)
    d = ImageDraw.Draw(img)
    _overlay(w, d, to_px, scale, terrain=False)
    for b, (_k, name, c) in BIOMES.items():
        m = biome == b
        if not m.any():
            continue
        cx, cy = float(g.wx[m].mean()), float(g.wy[m].mean())
        # Nudge the label onto the biome itself.
        tx, ty = g.to_tile(cx, cy)
        if not (g.inside(tx, ty) and biome[ty, tx] == b):
            yy, xx = np.argwhere(m)[len(np.argwhere(m)) // 2]
            cx, cy = g.centre(xx, yy)
        lx, ly = to_px(cx, cy)
        d.text((lx - 30, ly - 6), name.upper(), fill=(255, 255, 255))
    # Legend with the music test.
    lx, ly = 10, H - 150
    d.rectangle([lx - 4, ly - 4, lx + 560, H - 6], fill=(10, 12, 20))
    d.text((lx, ly), "biome            land   straight run through (median / quarter shortest)",
           fill=(220, 220, 220))
    for i, (b, (_k, name, c)) in enumerate(BIOMES.items()):
        med, q1, share = stats[b]
        y = ly + 14 + i * 14
        d.rectangle([lx, y + 2, lx + 10, y + 11], fill=c)
        d.text((lx + 16, y), f"{name:<18} {share * 100:4.1f}%   {med:5.0f} s / {q1:4.0f} s",
               fill=(220, 220, 220))
    d.text((8, 6), "planet biomes: borders white, round-3 storm around each landing site "
                   "blue, sites numbered. Straight runs at hero speed (255 px/s).",
           fill=(220, 220, 220))
    biome_path = Path(path).with_name(Path(path).stem + "_biomes.png")
    img.save(biome_path)
    return stats, biome_path


def _overlay(w, d, to_px, scale, terrain):
    # Round 3's storm around every landing site (the default one brighter).
    for i, (sx, sy) in enumerate(w["site_xy"]):
        cx, cy_ = to_px(sx, sy)
        rings = ZONE_RADII if i == w["site_index"] else ZONE_RADII[-1:]
        for r in rings:
            d.ellipse([cx - r * scale, cy_ - r * scale, cx + r * scale, cy_ + r * scale],
                      outline=(70, 130, 255) if i == w["site_index"] else (50, 80, 160))
    if terrain:
        for kind, x, y, _p in w["props"]:
            if kind == "city_block":
                px, py = to_px(x, y)
                d.rectangle([px - 1, py - 1 - _p["tall"] * scale * 0.5, px + 1, py + 1],
                            fill=(36, 40, 70), outline=(90, 200, 255))
            elif kind == "skyline":
                px, py = to_px(x, y)
                d.rectangle([px - _p["width"] * scale / 2, py - _p["tall"] * scale,
                             px + _p["width"] * scale / 2, py],
                            fill=(20, 24, 44), outline=(60, 120, 200))
        for kind, x, y in w["landmarks"]:
            px, py = to_px(x, y)
            d.polygon([(px, py - 10), (px + 5, py), (px - 5, py)], fill=(255, 120, 220))
        for kind, x, y, ring, _site in w["pois"]:
            px, py = to_px(x, y)
            col = (20, 20, 20) if ring == "near" else (240, 150, 40) if ring == "far" \
                else (240, 220, 90)
            d.ellipse([px - 3, py - 3, px + 3, py + 3], fill=col, outline=(255, 255, 255))
    for i, (x, y, _mix) in enumerate(w["sites"]):
        px, py = to_px(x, y)
        active = True
        d.ellipse([px - 6, py - 6, px + 6, py + 6], outline=(255, 255, 255),
                  fill=(120, 255, 170) if active else None, width=2)
        d.text((px + 8, py - 6), str(i), fill=(255, 255, 255))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--seed", type=int, default=3)
    ap.add_argument("--size", type=int, default=800)
    ap.add_argument("--site", type=int, default=0,
                    help="which candidate crash site the ship sits on")
    ap.add_argument("--out", default=str(MAPS / "planet.tmx"))
    ap.add_argument("--preview", default="")
    args = ap.parse_args()
    w = build(args.seed, args.size, args.site)
    out = Path(args.out)
    build_ground_tileset(args.seed).save(out.parent / "planet_tiles.png")
    build_data_tileset().save(out.parent / "terrain_data.png")
    write_tmx(w, out, args.seed, args.size)
    land = int(w["land"].sum())
    drawn = int((~(~w["land"] & (w["land_dist"] >= SEA_KEEP))).sum())
    print(f"wrote {args.out}: {args.size}x{args.size} tiles ({drawn} drawn), {land} land "
          f"tiles, {len(w['pois'])} points of interest, {len(w['landmarks'])} landmarks, "
          f"{len(w['rocks'])} rocks, {len(w['props'])} props")
    sx, sy = w["ship"]
    print(f"{len(w['sites'])} landing sites (the server picks one per match); default ship "
          f"on site {w['site_index']} at ({sx:.0f}, {sy:.0f})")
    for i in range(len(w["sites"])):
        n = sum(1 for p in w["pois"] if poi_in_play(w, p, i))
        print(f"  site {i}: {n} objectives in play")
    placed, med, p95, worst = w["coverage"]
    print(f"coverage: {placed} fill objectives; nearest objective in play from any "
          f"walkable spot, worst site: median {med:.0f} px, 95% {p95:.0f} px, "
          f"max {worst:.0f} px (target {COVER_GAP:.0f})")
    for i, (x, y, mix) in enumerate(w["sites"]):
        names = ", ".join(f"{BIOMES[b][1]} {mix[b] * 100:.0f}%" for b in BIOMES
                          if mix[b] >= 0.05)
        print(f"  site {i}: {names}, sea {mix[0] * 100:.0f}%")
    if args.preview:
        stats, biome_path = preview(w, args.preview)
        print(f"preview: {args.preview}, {biome_path}")
        for b, (med, q1, share) in stats.items():
            print(f"  {BIOMES[b][1]:<18} {share * 100:5.1f}% of land, straight run "
                  f"{med:4.0f} s median, {q1:4.0f} s quarter-shortest")


if __name__ == "__main__":
    main()
