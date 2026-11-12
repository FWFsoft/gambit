"""Quick look while modelling: one clip, every direction, scaled up.

  /opt/forge/bin/python tools/sprite_forge/preview.py wade guard out.png [frame]
"""

import importlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "actors"))

import bpy  # noqa: E402
from PIL import Image  # noqa: E402

import forge  # noqa: E402


def main():
    actor, clip_name, out = sys.argv[1], sys.argv[2], sys.argv[3]
    frames = [int(f) for f in sys.argv[4].split(",")] if len(sys.argv) > 4 else None
    mod = importlib.import_module(actor)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    model, base = mod.build()
    k = getattr(mod, "PIXEL_SCALE", 1.0)
    if len(sys.argv) > 5:
        k = float(sys.argv[5])
    cell = [round(v / k) for v in getattr(mod, "CELL", (128, 112, 98))]
    style = getattr(mod, "STYLE", "pixel")
    r = forge.Renderer(*cell, forge.PIXEL_SIZE * k,
                       supersample=1 if style == "pixel" else 2)
    clip = next(c for c in mod.clips() if c.name == clip_name)
    frames = frames or list(range(clip.frames))
    scale = max(1, round(3 * k))
    sheet = Image.new("RGBA", (cell[0] * 8 * scale, cell[1] * len(frames) * scale),
                      (78, 96, 66, 255))
    poses = forge.clip_frames(clip, base, model, getattr(mod, "FOLLOW", None))
    for fi, f in enumerate(frames):
        pose, off, rot = poses[f]
        for d in range(8):
            model.pose(pose, off, rot, d * 45.0)
            img = r.frame(model, style, getattr(mod, "RIM", None)).resize((cell[0] * scale, cell[1] * scale),
                                        Image.NEAREST)
            sheet.alpha_composite(img, (d * cell[0] * scale,
                                        fi * cell[1] * scale))
    sheet.save(out)
    print("wrote", out, sheet.size)


if __name__ == "__main__":
    main()
