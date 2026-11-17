#!/usr/bin/env python3
"""Scatter terrain (rock outcrops) over assets/maps/arena.tmx.

Writes the "Terrain" object layer: rectangles of Tiled type "terrain", which
the game loads as collision (players and enemies go around them) and which
Isaac's Lifeline anchors to. Re-running replaces the layer; the seed keeps
the layout stable.

Rules (see docs/design/DECISIONS.md, "Terrain"):
- rock centres about SPACING apart, so a Lifeline (520 from Isaac to the
  rock's face) can chain rock to rock across open ground;
- inside the round's largest zone, and inside the map's diamond;
- clear of the ship's safe ring, the objectives' circles and every spawn.

    python3 tools/maps/place_terrain.py [--seed N]
"""

import argparse
import math
import random
import re
import sys

MAP = "assets/maps/arena.tmx"
Y_SHIFT = 2800.0          # tmx y = world y + 2800 (the map's centring)
HALF_W, HALF_H = 5632.0, 2800.0
ZONE = 2900.0             # largest zone radius (round 3)
SHIP_CLEAR = 400.0        # the ship's safe ring is 300
SPACING = 560.0
SPAWN_CLEAR = 110.0
OBJECTIVE_MARGIN = 110.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    tmx = open(MAP).read()
    tmx = re.sub(r'\s*<objectgroup id="\d+" name="Terrain">.*?</objectgroup>', "", tmx, flags=re.S)

    blocked = []          # (x, y, clearance) in world space
    for g in re.finditer(r'<objectgroup[^>]*name="([^"]+)"(.*?)</objectgroup>', tmx, re.S):
        layer, body = g.group(1), g.group(2)
        for o in re.finditer(r'<object id="\d+" name="[^"]*"[^>]*x="([-\d.]+)" y="([-\d.]+)"(.*?)(?:/>|</object>)', body, re.S):
            x, y = float(o.group(1)), float(o.group(2)) - Y_SHIFT
            if layer == "Objectives":
                r = re.search(r'name="radius" type="float" value="([\d.]+)"', o.group(3))
                blocked.append((x, y, (float(r.group(1)) if r else 100.0) + OBJECTIVE_MARGIN))
            elif layer in ("PlayerSpawns", "EnemySpawns"):
                blocked.append((x, y, SPAWN_CLEAR))
    blocked.append((0.0, 0.0, SHIP_CLEAR))

    def ok(x, y, w, h, rocks):
        if math.hypot(x, y) > ZONE - 100:
            return False
        if abs(x) / HALF_W + abs(y) / HALF_H > 0.92:
            return False
        for bx, by, r in blocked:
            cx = min(max(bx, x - w / 2), x + w / 2)
            cy = min(max(by, y - h / 2), y + h / 2)
            if math.hypot(cx - bx, cy - by) < r:
                return False
        return all(math.hypot(x - rx, y - ry) >= SPACING for rx, ry, _, _ in rocks)

    rocks = []
    for _ in range(40000):  # dart throwing: fill until nothing fits
        x, y = rng.uniform(-ZONE, ZONE), rng.uniform(-HALF_H, HALF_H)
        w, h = round(rng.uniform(70, 150)), round(rng.uniform(45, 95))
        if ok(x, y, w, h, rocks):
            rocks.append((x, y, w, h))
    rocks.sort(key=lambda r: (r[1], r[0]))

    next_id = int(re.search(r'nextobjectid="(\d+)"', tmx).group(1))
    next_layer = int(re.search(r'nextlayerid="(\d+)"', tmx).group(1))
    lines = [f'  <objectgroup id="{next_layer}" name="Terrain">']
    for i, (x, y, w, h) in enumerate(rocks):
        lines.append(f'    <object id="{next_id + i}" name="Rock_{i + 1}" type="terrain" '
                     f'x="{x - w / 2:.1f}" y="{y - h / 2 + Y_SHIFT:.1f}" width="{w}" height="{h}" />')
    lines.append("  </objectgroup>")
    tmx = tmx.replace("</map>", "\n".join(lines) + "\n</map>")
    tmx = re.sub(r'nextobjectid="\d+"', f'nextobjectid="{next_id + len(rocks)}"', tmx)
    tmx = re.sub(r'nextlayerid="\d+"', f'nextlayerid="{next_layer + 1}"', tmx)
    open(MAP, "w").write(tmx)
    inner = sum(1 for x, y, _, _ in rocks if math.hypot(x, y) < 950)
    print(f"{len(rocks)} rocks ({inner} inside the boss ring) -> {MAP}", file=sys.stderr)


if __name__ == "__main__":
    main()
