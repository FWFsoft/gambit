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
    cell = getattr(mod, "CELL", (128, 112, 98))
    r = forge.Renderer(*cell)
    clip = next(c for c in mod.clips() if c.name == clip_name)
    frames = frames or list(range(clip.frames))
    scale = 3
    sheet = Image.new("RGBA", (cell[0] * 8 * scale, cell[1] * len(frames) * scale),
                      (78, 96, 66, 255))
    for fi, f in enumerate(frames):
        denom = clip.frames if clip.loop else max(1, clip.frames - 1)
        pose, off, rot = forge.sample(clip, f / denom, base)
        for d in range(8):
            model.pose(pose, off, rot, d * 45.0)
            img = r.frame(model).resize((cell[0] * scale, cell[1] * scale),
                                        Image.NEAREST)
            sheet.alpha_composite(img, (d * cell[0] * scale,
                                        fi * cell[1] * scale))
    sheet.save(out)
    print("wrote", out, sheet.size)


if __name__ == "__main__":
    main()
