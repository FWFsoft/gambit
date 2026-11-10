#!/usr/bin/env python3
"""Sprite pipeline: turns existing art into game-ready sprites.

Nothing here is drawn by hand. It converts art the team already has:

  characters  Preferred source: assets/turnarounds/<Name>.png, an 8-pose
              turnaround drawn at the game's isometric camera angle. Eight
              equal cells in one row, in this order:
                  S  SE  E  NE  N  NW  W  SW
              (S = facing the camera, E = facing screen-right), transparent
              background, feet on the same baseline in every cell.
              -> assets/sprites/characters/<Name>.png, an 8-frame strip. The
              client picks the frame from the direction the character faces.

              Fallback when no turnaround exists yet: assets/previews/<Name>.png
              (1024x1536 concept illustration) is cropped, de-shadowed and
              downscaled into a single placeholder frame that the client
              mirrors for left/right. It is splash art, not a game sprite, so
              it never shows the character's back or true facing.

              Either way the client animates frames procedurally
              (bob / lean / lunge / flash) until real animation frames exist.

  enemies     <alien-arena>/Assets/Creatures/impl/NonPlayable/<Genus>/<Name>/
              {Idle,Run,Die}/Art/*.png (Unity sheets, sliced via .meta)
              -> assets/sprites/enemies/<Name>.png
              One sheet per enemy: row 0 idle, row 1 run, row 2 die, uniform
              cells, feet on the bottom-centre of each cell.

  fx          a few effect sheets from alien-arena -> assets/sprites/fx/

Also writes assets/sprites/manifest.txt, a whitespace-separated table the
client parses (no JSON dependency in the engine):

  character <Name> <file> <cellW> <cellH> <directions>   (directions: 8 or 1)
  enemy <Name> <Genus> <tier> <file> <cellW> <cellH> <idleN> <runN> <dieN>
  fx <name> <file> <cellW> <cellH> <frames>

Usage:
  python3 tools/sprite_pipeline/build_sprites.py [--alien-arena ../alien-arena]
"""

import argparse
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageEnhance

REPO = Path(__file__).resolve().parents[2]
PREVIEWS = REPO / "assets" / "previews"
TURNAROUNDS = REPO / "assets" / "turnarounds"
OUT = REPO / "assets" / "sprites"
TURNAROUND_DIRECTIONS = 8

# A full-height character occupies roughly 1420px of the 1536px canvas.
# Everything is scaled by the same factor so relative sizes survive
# (Suds stays a tank, Namora stays crouched).
CHARACTER_SOURCE_HEIGHT = 1420.0
CHARACTER_TARGET_HEIGHT = 76.0
CHARACTER_COLORS = 28
OUTLINE = (18, 14, 24, 255)

GENERA = {
    "Venoxarid": ["Toxlith", "Venomire", "Serpentide"],
    "Cerebrian": ["Psymite", "Cerebrax", "Synaptogon"],
    "Frigidariant": ["Glacili", "Cryothor", "Frosthulm"],
    "Magnarok": ["Emberclad", "Pyroclast", "Lavaforge"],
}
ANIMS = ["Idle", "Run", "Die"]


def remove_ground_shadow(im: Image.Image) -> Image.Image:
    """Concept art has a painted grey shadow/splatter under the feet."""
    a = np.array(im).astype(np.float32)
    h = a.shape[0]
    rgb = a[..., :3]
    mx = rgb.max(-1)
    mn = rgb.min(-1)
    sat = (mx - mn) / np.maximum(mx, 1)
    ys = np.arange(h)[:, None] / h
    grey = (sat < 0.12) & (mx > 95) & (mx < 235) & (ys > 0.80)
    # the brightest splatter only hugs the very bottom; keep white clothing
    white = (sat < 0.10) & (mx >= 235) & (ys > 0.93)
    a[..., 3] = np.where(grey | white, 0, a[..., 3])
    return Image.fromarray(a.astype(np.uint8))


def add_outline(rgba: np.ndarray) -> np.ndarray:
    opaque = rgba[..., 3] > 0
    pad = np.pad(opaque, 1)
    neighbour = pad[:-2, 1:-1] | pad[2:, 1:-1] | pad[1:-1, :-2] | pad[1:-1, 2:]
    out = np.pad(rgba, ((0, 0), (0, 0), (0, 0)))
    out[neighbour & ~opaque] = OUTLINE
    return out


def pixelize(im: Image.Image, scale: float, colors: int) -> Image.Image:
    """Downscale RGBA art into an outlined, palette-limited pixel sprite."""
    w = max(1, round(im.width * scale))
    h = max(1, round(im.height * scale))

    # Downscale in premultiplied alpha so edges do not pick up black fringes.
    a = np.array(im).astype(np.float32) / 255.0
    a[..., :3] *= a[..., 3:4]
    small = Image.fromarray((a * 255).astype(np.uint8)).resize((w, h), Image.BOX)
    s = np.array(small).astype(np.float32) / 255.0
    alpha = s[..., 3]
    rgb = np.where(alpha[..., None] > 0.01,
                   s[..., :3] / np.maximum(alpha[..., None], 0.01), 0)
    opaque = alpha > 0.45

    rgb_im = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8))
    rgb_im = ImageEnhance.Color(rgb_im).enhance(1.25)
    rgb_im = ImageEnhance.Contrast(rgb_im).enhance(1.12)
    quant = rgb_im.quantize(colors=colors, method=Image.MEDIANCUT,
                            dither=Image.NONE).convert("RGB")

    rgba = np.dstack([np.array(quant), (opaque * 255).astype(np.uint8)])
    rgba[~opaque] = 0
    return Image.fromarray(rgba.astype(np.uint8))


def outlined(im: Image.Image) -> Image.Image:
    # 1px transparent border so the outline has room.
    rgba = np.pad(np.array(im), ((1, 1), (1, 1), (0, 0)))
    return Image.fromarray(add_outline(rgba).astype(np.uint8))


def build_character_from_preview(name: str) -> Image.Image:
    """Placeholder: one frame distilled from the concept illustration."""
    im = Image.open(PREVIEWS / f"{name}.png").convert("RGBA")
    im = im.crop(im.getbbox())
    im = remove_ground_shadow(im)
    im = im.crop(im.getbbox())
    scale = CHARACTER_TARGET_HEIGHT / CHARACTER_SOURCE_HEIGHT
    return outlined(pixelize(im, scale, CHARACTER_COLORS))


def feet_centre(frame: Image.Image) -> float:
    """x of the middle of the feet: the pivot that must not move between poses."""
    alpha = np.array(frame)[..., 3] > 40
    rows = np.where(alpha.any(1))[0]
    if len(rows) == 0:
        return frame.width / 2.0
    top, bottom = rows.min(), rows.max()
    band = alpha[int(bottom - (bottom - top) * 0.15):bottom + 1]
    cols = np.where(band.any(0))[0]
    return (cols.min() + cols.max() + 1) / 2.0


def build_character_from_turnaround(path: Path):
    """8-direction strip from a turnaround sheet. Returns (image, cellW, cellH)."""
    sheet = Image.open(path).convert("RGBA")
    cell = sheet.width // TURNAROUND_DIRECTIONS
    frames = [sheet.crop((i * cell, 0, (i + 1) * cell, sheet.height))
              for i in range(TURNAROUND_DIRECTIONS)]

    # One scale for every pose so the character does not pulse while turning.
    tallest = max(f.getbbox()[3] - f.getbbox()[1] for f in frames)
    scale = min(1.0, CHARACTER_TARGET_HEIGHT / tallest)

    # Quantize the whole strip together so all eight poses share one palette.
    small = pixelize(sheet, scale, CHARACTER_COLORS + 4)
    small_cell = small.width / TURNAROUND_DIRECTIONS
    out_frames = []
    for i in range(TURNAROUND_DIRECTIONS):
        frame = small.crop((round(i * small_cell), 0,
                            round((i + 1) * small_cell), small.height))
        box = frame.getbbox()
        pivot = feet_centre(frame) - box[0]
        out_frames.append((outlined(frame.crop(box)), pivot + 1))

    half = max(max(p, f.width - p) for f, p in out_frames)
    cell_w = int(np.ceil(half)) * 2
    cell_h = max(f.height for f, _ in out_frames)
    strip = Image.new("RGBA", (cell_w * TURNAROUND_DIRECTIONS, cell_h),
                      (0, 0, 0, 0))
    for i, (frame, pivot) in enumerate(out_frames):
        x = i * cell_w + cell_w // 2 - round(pivot)
        strip.alpha_composite(frame, (x, cell_h - frame.height))
    return strip, cell_w, cell_h


def unity_frame_count(png: Path) -> int:
    meta = Path(str(png) + ".meta").read_text()
    return max(1, len(re.findall(r"^\s+rect:\s*$", meta, flags=re.M)))


def load_frames(png: Path):
    sheet = Image.open(png).convert("RGBA")
    n = unity_frame_count(png)
    fw = sheet.width // n
    return [sheet.crop((i * fw, 0, (i + 1) * fw, sheet.height))
            for i in range(n)]


def build_enemy(root: Path, genus: str, name: str):
    base = root / "Assets/Creatures/impl/NonPlayable" / genus / name
    rows = []
    for anim in ANIMS:
        pngs = sorted((base / anim / "Art").glob("*.png"))
        if not pngs:
            raise FileNotFoundError(f"missing {anim} art for {name}")
        rows.append(load_frames(pngs[0]))

    cell_w = max(f.width for row in rows for f in row)
    cell_h = max(f.height for row in rows for f in row)
    cols = max(len(row) for row in rows)
    sheet = Image.new("RGBA", (cell_w * cols, cell_h * len(rows)), (0, 0, 0, 0))
    for r, row in enumerate(rows):
        for c, frame in enumerate(row):
            x = c * cell_w + (cell_w - frame.width) // 2
            y = r * cell_h + (cell_h - frame.height)
            sheet.alpha_composite(frame, (x, y))
    return sheet, cell_w, cell_h, [len(row) for row in rows]


FX_SOURCES = [
    # (name, relative path in alien-arena, frames)
    ("brew_can", "Assets/Suds/Suds_LMB_Anim (1).png", 8),
    ("explosion", "Assets/Suds/Esplosion (1).png", 4),
]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--alien-arena", default=str(REPO.parent / "alien-arena"),
                    help="path to a checkout of the alien-arena Unity project")
    args = ap.parse_args()
    arena = Path(args.alien_arena)

    manifest = []

    (OUT / "characters").mkdir(parents=True, exist_ok=True)
    names = sorted({p.stem for p in PREVIEWS.glob("*.png")} |
                   {p.stem for p in TURNAROUNDS.glob("*.png")})
    for name in names:
        rel = f"assets/sprites/characters/{name}.png"
        turnaround = TURNAROUNDS / f"{name}.png"
        if turnaround.exists():
            sprite, cw, ch = build_character_from_turnaround(turnaround)
            dirs = TURNAROUND_DIRECTIONS
            source = "turnaround"
        else:
            sprite = build_character_from_preview(name)
            cw, ch, dirs = sprite.width, sprite.height, 1
            source = "concept art placeholder"
        sprite.save(REPO / rel)
        manifest.append(f"character {name} {rel} {cw} {ch} {dirs}")
        print(f"character {name:<12} {cw}x{ch} x{dirs}  ({source})")

    if arena.is_dir():
        (OUT / "enemies").mkdir(parents=True, exist_ok=True)
        for genus, names in GENERA.items():
            for tier, name in enumerate(names):
                sheet, cw, ch, counts = build_enemy(arena, genus, name)
                rel = f"assets/sprites/enemies/{name}.png"
                sheet.save(REPO / rel)
                manifest.append(
                    f"enemy {name} {genus} {tier} {rel} {cw} {ch} "
                    f"{counts[0]} {counts[1]} {counts[2]}")
                print(f"enemy {name:<12} {genus:<13} cell {cw}x{ch} "
                      f"idle/run/die {counts}")

        (OUT / "fx").mkdir(parents=True, exist_ok=True)
        for name, rel_src, frames in FX_SOURCES:
            im = Image.open(arena / rel_src).convert("RGBA")
            rel = f"assets/sprites/fx/{name}.png"
            im.save(REPO / rel)
            manifest.append(
                f"fx {name} {rel} {im.width // frames} {im.height} {frames}")
            print(f"fx {name:<12} {im.width // frames}x{im.height} x{frames}")
    else:
        print(f"warning: {arena} not found - skipping enemies/fx "
              "(existing files are left untouched)", file=sys.stderr)
        existing = OUT / "manifest.txt"
        if existing.exists():
            manifest += [line for line in existing.read_text().splitlines()
                         if line.startswith(("enemy ", "fx "))]

    (OUT / "manifest.txt").write_text("\n".join(manifest) + "\n")
    print(f"wrote {OUT / 'manifest.txt'} ({len(manifest)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
