#!/usr/bin/env python3
"""Bake forge actors into game sprite sheets.

Needs Blender's Python module (bpy). One-time setup:

    python3 -m venv /opt/forge
    /opt/forge/bin/pip install bpy numpy pillow

Then, from the repo root:

    /opt/forge/bin/python tools/sprite_forge/build.py            # every actor
    /opt/forge/bin/python tools/sprite_forge/build.py wade       # just Wade

Writes assets/sprites/actors/<Name>.png and assets/sprites/actors/manifest.txt.
An actor is a module in tools/sprite_forge/actors/ with build() -> (Model,
base pose), clips() -> [Clip], and optionally CELL = (w, h, feet_y).
"""

import importlib
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "actors"))

import forge  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
OUT_DIR = os.path.join(REPO, "assets", "sprites", "actors")
MANIFEST = os.path.join(OUT_DIR, "manifest.txt")


def main():
    available = sorted(f[:-3] for f in os.listdir(os.path.join(HERE, "actors"))
                       if f.endswith(".py") and not f.startswith("_"))
    wanted = sys.argv[1:] or available
    os.makedirs(OUT_DIR, exist_ok=True)

    # Keep entries for actors we are not rebuilding.
    kept = {}
    if os.path.exists(MANIFEST):
        for line in open(MANIFEST).read().splitlines():
            parts = line.split()
            if len(parts) > 1:
                kept.setdefault(parts[1], []).append(line)

    for key in wanted:
        mod = importlib.import_module(key)
        w, h, feet = getattr(mod, "CELL", (128, 112, 98))
        k = getattr(mod, "PIXEL_SCALE", 1.0)
        # CELL is in standard pixels; chunkier pixels need fewer of them.
        w, h, feet = round(w / k), round(h / k), round(feet / k)
        name = mod.__name__.capitalize()
        name = getattr(mod, "NAME", name)
        spec = forge.ActorSpec(name, mod.build, mod.clips(), w, h, feet,
                               pixel_scale=k,
                               socket=getattr(mod, "SOCKET", None),
                               style=getattr(mod, "STYLE", "pixel"),
                               rim_rgb=getattr(mod, "RIM", None),
                               follow=getattr(mod, "FOLLOW", None))
        rel = f"assets/sprites/actors/{name}.png"
        start = time.time()
        print(f"{name}:")
        kept[name] = forge.render_actor(spec, os.path.join(REPO, rel), rel)
        print(f"  done in {time.time() - start:.0f}s -> {rel}")

    with open(MANIFEST, "w") as out:
        for name in sorted(kept):
            out.write("\n".join(kept[name]) + "\n")
    print(f"wrote {MANIFEST}")


if __name__ == "__main__":
    main()
