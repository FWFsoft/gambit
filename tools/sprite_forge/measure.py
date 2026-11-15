#!/usr/bin/env python3
"""Where the joints end up in a pose: numbers for getting reach right.

    <forge python> tools/sprite_forge/measure.py wade guard
    <forge python> tools/sprite_forge/measure.py _wade_mesh guard

Poses the actor with a zero-argument pose function from its module (guard,
stand, ...) and prints each joint's position, in forge units, measured from
the head joint: x is the character's left, -y is forward, z is up. For a
skinned mesh it also prints where the surface each hand owns sits (the fist
itself, not the wrist joint).

Use it to move a pose from one body to another: measure both, then change
angles until the hands sit in the same place relative to the head.
"""

import importlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "actors"))

import bpy  # noqa: E402
import numpy as np  # noqa: E402


def measure(module, pose_name):
    mod = importlib.import_module(module)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    model, _base = mod.build()
    model.pose(getattr(mod, pose_name)(), (0, 0, 0), (0, 0, 0), 0.0)
    bpy.context.view_layer.update()
    at = {n: np.array(j.matrix_world.translation) for n, j in model.joints.items()}
    out = {n: p - at["head"] for n, p in at.items()}
    for s in getattr(model, "skins", []):
        co = np.empty(len(s.obj.data.vertices) * 3)
        s.obj.data.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        for side in "LR":
            k = s.joints.index(f"wrist_{side}")
            own = (s.idx[:, 0] == k)
            if own.any():
                out[f"fist_{side} (surface centre)"] = co[own].mean(0) - at["head"]
                out[f"fist_{side} (top of surface)"] = co[own][np.argmax(co[own][:, 2])] - at["head"]
    return out


def main():
    module, pose_name = sys.argv[1], sys.argv[2]
    print(f"{module}.{pose_name}(): position from the head joint (left, forward -, up)")
    for name, p in measure(module, pose_name).items():
        print(f"  {name:28} {p[0]:+.3f} {p[1]:+.3f} {p[2]:+.3f}")


if __name__ == "__main__":
    main()
