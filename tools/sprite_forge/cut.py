#!/usr/bin/env python3
"""Cut a loose part out of a generated model, for free.

    <forge python> tools/sprite_forge/cut.py in.glb out.glb --height 2.0 \
        --box 0.29 0.375 -0.2 0.2 0.93 1.17 --mirror

Image-to-3D models fuse dangling things to the body: a chain hanging from a
wrist, a strap, a tassel. In the A-pose they hang straight down; once the
forge swings the arm down to the side, the skin carries them with the
forearm and they poke through the leg. If a part cannot be posed, cut it
and, if it matters, add it back in code on its own joint.

Boxes are x0 x1 y0 y1 z0 z1 in the forge's normalised frame (the one
fit.py prints joints in: feet on z = 0, `--height` tall, centred, facing
-Y, the model's left is +X). `--mirror` also cuts each box mirrored in X.
Every face with a vertex in a box goes. Look at the result with fit.py.
"""

import argparse
import os
import sys

import bpy  # noqa: I001  (bmesh needs bpy loaded first)
import bmesh
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import skin  # noqa: E402


def cut(src, dst, height, boxes, mirror):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    obj, v = skin.load_mesh(src, height)
    raw = skin._coords(obj.data)
    if mirror:
        boxes = boxes + [(-b[1], -b[0], b[2], b[3], b[4], b[5]) for b in boxes]
    inside = np.zeros(len(v), bool)
    for x0, x1, y0, y1, z0, z1 in boxes:
        assert x0 < x1 and y0 < y1 and z0 < z1, f"empty box {x0, x1, y0, y1, z0, z1}"
        inside |= ((v[:, 0] >= x0) & (v[:, 0] <= x1) & (v[:, 1] >= y0) &
                   (v[:, 1] <= y1) & (v[:, 2] >= z0) & (v[:, 2] <= z1))
    assert inside.any(), "no vertex in any box: check the coordinates"
    assert inside.mean() < 0.1, "that would cut a tenth of the model: not a loose part"
    # load_mesh normalised the coordinates; put the originals back so the
    # file keeps its own frame.
    skin._set_coords(obj.data, raw)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    doomed = [bm.verts[i] for i in np.flatnonzero(inside)]
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    for o in list(bpy.context.scene.objects):
        o.select_set(o is obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(filepath=dst, export_format="GLB",
                              use_selection=True, export_apply=True)
    print(f"cut {int(inside.sum())} of {len(v)} vertices: {dst}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--height", type=float, required=True)
    ap.add_argument("--box", type=float, nargs=6, action="append", required=True,
                    metavar=("X0", "X1", "Y0", "Y1", "Z0", "Z1"))
    ap.add_argument("--mirror", action="store_true")
    a = ap.parse_args()
    cut(a.src, a.dst, a.height, [tuple(b) for b in a.box], a.mirror)


if __name__ == "__main__":
    main()
