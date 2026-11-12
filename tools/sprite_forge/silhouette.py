#!/usr/bin/env python3
"""The "who's that?" test from the art on-ramp, automated.

Every character and enemy must be recognisable from its blacked-out
silhouette alone. This renders each actor's idle pose as a black shape at
in-game size, in several directions, side by side, and scores how much
each pair overlaps when stood on the same spot. Pairs that overlap too much
are flagged: they will be hard to tell apart in a fight.

    python3 tools/sprite_forge/silhouette.py                 # forge actors
    python3 tools/sprite_forge/silhouette.py --legacy        # + old cut-outs
    python3 tools/sprite_forge/silhouette.py --out lineup.png Wade Presidente

Size is part of a silhouette, so shapes are compared at their real size,
feet aligned. Needs only Pillow and numpy.
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ACTORS = os.path.join(REPO, "assets", "sprites", "actors", "manifest.txt")
LEGACY = os.path.join(REPO, "assets", "sprites", "manifest.txt")

WORLD_ZOOM = 1.5        # ArenaView draws the world magnified by this
DIRECTIONS = {"S": 0, "SE": 1, "E": 2, "N": 4}
FLAG_OVERLAP = 0.72     # overlap at real size above this is a look-alike
FLAG_SHAPE = 0.80       # overlap with both scaled to the same height
SHAPE_HEIGHT = 100


def load_actors():
    actors = {}
    for line in open(ACTORS).read().splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == "actor":
            actors[p[1]] = {"png": p[2], "w": int(p[3]), "h": int(p[4]),
                            "px": int(p[5]), "py": int(p[6]),
                            "cols": int(p[7]),
                            "scale": float(p[8]) if len(p) > 8 else 1.0,
                            "anims": {}}
        elif p[0] == "anim" and p[1] in actors:
            actors[p[1]]["anims"][p[2]] = (int(p[3]), int(p[4]))
    return actors


def mask_from_rgba(img, scale):
    alpha = np.array(img.getchannel("A")) > 40
    m = Image.fromarray((alpha * 255).astype(np.uint8))
    if scale != 1.0:
        m = m.resize((max(1, round(m.width * scale)),
                      max(1, round(m.height * scale))), Image.NEAREST)
    return np.array(m) > 127


def cropped(mask, fx, fy):
    """Trim empty space, keeping the feet point in the same place."""
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return mask, fx, fy
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    return mask[y0:y1, x0:x1], fx - x0, fy - y0


def actor_masks(name, a, sheets):
    """direction -> (mask, feet_x, feet_y) at on-screen size."""
    if a["png"] not in sheets:
        sheets[a["png"]] = Image.open(os.path.join(REPO, a["png"])).convert("RGBA")
    sheet = sheets[a["png"]]
    start, frames = a["anims"].get("idle", (0, 1))
    scale = a["scale"] * WORLD_ZOOM
    out = {}
    for label, d in DIRECTIONS.items():
        cell = start + d * frames
        x, y = (cell % a["cols"]) * a["w"], (cell // a["cols"]) * a["h"]
        img = sheet.crop((x, y, x + a["w"], y + a["h"]))
        out[label] = cropped(mask_from_rgba(img, scale), a["px"] * scale,
                             a["py"] * scale)
    return out


def legacy_masks(names):
    out = {}
    if not os.path.exists(LEGACY):
        return out
    for line in open(LEGACY).read().splitlines():
        p = line.split()
        if len(p) < 6 or p[0] != "character" or (names and p[1] not in names):
            continue
        img = Image.open(os.path.join(REPO, p[2])).convert("RGBA")
        w, h = int(p[3]), int(p[4])
        img = img.crop((0, 0, w, h))
        m = mask_from_rgba(img, WORLD_ZOOM)
        # One pose for every direction; feet at the bottom centre.
        entry = cropped(m, m.shape[1] / 2.0, float(m.shape[0]))
        out[p[1] + " (old)"] = {k: entry for k in DIRECTIONS}
    return out


def overlap(a, b):
    """IoU of two masks stood on the same feet point."""
    (ma, ax, ay), (mb, bx, by) = a, b
    ax, ay, bx, by = round(ax), round(ay), round(bx), round(by)
    left = max(ax, bx)
    up = max(ay, by)
    W = left + max(ma.shape[1] - ax, mb.shape[1] - bx)
    H = up + max(ma.shape[0] - ay, mb.shape[0] - by)
    ca = np.zeros((H, W), bool)
    cb = np.zeros((H, W), bool)
    ca[up - ay:up - ay + ma.shape[0], left - ax:left - ax + ma.shape[1]] = ma
    cb[up - by:up - by + mb.shape[0], left - bx:left - bx + mb.shape[1]] = mb
    union = (ca | cb).sum()
    return (ca & cb).sum() / union if union else 0.0


def same_height(entry):
    """The shape alone: scaled so its feet-to-top height is SHAPE_HEIGHT."""
    m, fx, fy = entry
    k = SHAPE_HEIGHT / max(1.0, fy)
    img = Image.fromarray((m * 255).astype(np.uint8))
    img = img.resize((max(1, round(m.shape[1] * k)), max(1, round(m.shape[0] * k))),
                     Image.NEAREST)
    return np.array(img) > 127, fx * k, fy * k


def lineup(masks, path):
    names = list(masks)
    pad, label_h = 18, 16
    col_w = []
    for n in names:
        col_w.append(max(70, max(m[0].shape[1] for m in masks[n].values())))
    row_h = max(m[0].shape[0] for n in names for m in masks[n].values()) + 8
    W = sum(col_w) + pad * (len(names) + 1) + 40
    H = (row_h + 6) * len(DIRECTIONS) + label_h + 10
    img = Image.new("RGB", (W, H), (206, 201, 190))
    draw = ImageDraw.Draw(img)
    for r, d in enumerate(DIRECTIONS):
        y0 = r * (row_h + 6) + 4
        draw.text((6, y0 + row_h // 2), d, fill=(90, 90, 90))
        x = 40 + pad
        for n, w in zip(names, col_w):
            m, fx, fy = masks[n][d]
            ground = y0 + row_h
            ox, oy = int(x + w / 2 - fx), int(ground - fy)
            shape = Image.fromarray((m * 255).astype(np.uint8))
            img.paste((16, 16, 22), (ox, oy), shape)
            x += w + pad
    x = 40 + pad
    for n, w in zip(names, col_w):
        draw.text((x, H - label_h), n, fill=(30, 30, 30))
        x += w + pad
    img.save(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("names", nargs="*", help="actors to include (default all)")
    ap.add_argument("--legacy", action="store_true",
                    help="include the old single-pose character cut-outs")
    ap.add_argument("--out", default=os.path.join(REPO, "silhouettes.png"))
    args = ap.parse_args()

    actors = load_actors()
    sheets = {}
    masks = {}
    for name, a in actors.items():
        if args.names and name not in args.names:
            continue
        masks[name] = actor_masks(name, a, sheets)
    if args.legacy:
        masks.update(legacy_masks(set(args.names)))
    if not masks:
        sys.exit("no actors found")

    lineup(masks, args.out)
    print(f"wrote {args.out}")

    names = list(masks)
    flagged = 0
    print(f"\nmost similar direction per pair: overlap at real size "
          f"(flag > {FLAG_OVERLAP:.2f}) and shape alone, same height "
          f"(flag > {FLAG_SHAPE:.2f})")
    rows = []
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            a, b = masks[names[i]], masks[names[j]]
            size = max((overlap(a[d], b[d]), d) for d in DIRECTIONS)
            shape = max((overlap(same_height(a[d]), same_height(b[d])), d)
                        for d in DIRECTIONS)
            rows.append((max(size[0] / FLAG_OVERLAP, shape[0] / FLAG_SHAPE),
                         size, shape, names[i], names[j]))
    for worst, size, shape, a, b in sorted(rows, reverse=True):
        mark = "  <-- look-alike" if worst > 1.0 else ""
        flagged += worst > 1.0
        print(f"  size {size[0]:.2f} ({size[1]:>2})  shape {shape[0]:.2f} "
              f"({shape[1]:>2})  {a} / {b}{mark}")
    if flagged:
        print(f"\n{flagged} pair(s) too similar: change proportions, pose or "
              "a signature shape that breaks the outline.")
    return 1 if flagged else 0


if __name__ == "__main__":
    sys.exit(main())
