#!/usr/bin/env python3
"""Self-test for the skinned-mesh path (skin.py, the albedo pass, the ink
style), needing no outside model file.

    <forge python> tools/sprite_forge/selftest_skin.py [out_dir]

It makes its own "image-to-3D output": the code-built Wade, stood in an
A-pose, merged into one textured mesh and exported as a GLB. It then loads
that file the way a real one is loaded (import, normalise, auto-fit, bind,
re-pose, attach), poses it with Wade's real clips and bakes frames in the
ink style. It checks that:

  * the mesh survives the round trip at the right size, feet on the floor;
  * the texture comes back in the albedo pass with the right colours;
  * posing moves the mesh (the jab reaches forward) without tearing it
    apart (the sprite stays one connected shape);
  * every vertex has weights.

This proves the plumbing. It says nothing about how good a real model
looks: for that, bake a real one and look at it.
"""

import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "actors"))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

import forge  # noqa: E402
import skin  # noqa: E402
import wade  # noqa: E402

A_POSE = {"shoulder_L": (0, -42, 0), "shoulder_R": (0, 42, 0),
          "hip_L": (0, -7, 0), "hip_R": (0, 7, 0)}
CELL = (176, 156, 128)


def export_test_model(path):
    """The code-built Wade in an A-pose as one textured GLB."""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    model, _ = wade.build()
    model.pose(A_POSE)
    mats = list(model.materials.values())
    cells = 8
    img = bpy.data.images.new("palette", cells * 8, cells * 8)
    px = np.zeros((cells * 8, cells * 8, 4), dtype=np.float32)
    px[..., 3] = 1.0
    uv_of = {}
    for i, m in enumerate(mats):
        cx, cy = i % cells, i // cells
        # A byte image holds sRGB values as they are.
        px[cy * 8:(cy + 1) * 8, cx * 8:(cx + 1) * 8, :3] = np.array(m.base) / 255.0
        uv_of[m.bmat.name] = ((cx + 0.5) / cells, (cy + 0.5) / cells)
    img.pixels.foreach_set(px.ravel())
    img.pack()

    tex = bpy.data.materials.new("wade_texture")
    tex.use_nodes = True
    bsdf = next(n for n in tex.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    node = tex.node_tree.nodes.new("ShaderNodeTexImage")
    node.image = img
    tex.node_tree.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])

    for part in model.parts:
        mesh = part.data
        uv = uv_of[mesh.materials[0].name]
        layer = mesh.uv_layers.new(name="UVMap")
        layer.data.foreach_set("uv", np.tile(uv, len(mesh.loops)).astype(np.float32))
        mesh.materials[0] = tex
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for part in model.parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = model.parts[0]
    bpy.ops.object.join()
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB",
                              use_selection=True, export_yup=True)
    return {m.name: m.base for m in mats}


def connected_fraction(alpha):
    """Share of the sprite's pixels in its largest 4-connected piece."""
    h, w = alpha.shape
    seen = np.zeros_like(alpha, dtype=bool)
    best = 0
    for sy, sx in zip(*np.nonzero(alpha)):
        if seen[sy, sx]:
            continue
        stack, count = [(sy, sx)], 0
        seen[sy, sx] = True
        while stack:
            y, x = stack.pop()
            count += 1
            for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= ny < h and 0 <= nx < w and alpha[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        best = max(best, count)
    return best / max(1, alpha.sum())


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else tempfile.mkdtemp(prefix="forge_selftest_")
    os.makedirs(out_dir, exist_ok=True)
    glb = os.path.join(out_dir, "selftest_wade.glb")
    palette = export_test_model(glb)
    print("exported", glb, f"({os.path.getsize(glb) // 1024} KB)")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    model = forge.Model("Selftest")
    body = skin.attach(model, glb, height=1.95, cache=False)
    v = body.rest
    assert abs(v[:, 2].max() - v[:, 2].min() - 1.95) < 0.12, "height changed in re-posing"
    assert abs(v[:, 2].min()) < 1e-6, "feet are not on the floor"
    assert np.isfinite(body.w).all() and (body.w.sum(1) > 0.999).all(), "unweighted vertices"
    # Arms hang straight down in the forge rest pose.
    for side in ("L", "R"):
        s, w = model.rest_world(f"shoulder_{side}"), model.rest_world(f"wrist_{side}")
        assert abs(s[0] - w[0]) < 1e-6 and w[2] < s[2], "arms were not straightened"

    k = wade.PIXEL_SCALE
    r = forge.Renderer(*CELL, forge.PIXEL_SIZE * k, supersample=2)
    clips = {c.name: c for c in wade.clips()}
    base = wade.guard()
    rows = []
    reach = {}
    for clip_name, frame in (("idle", 0), ("jab", 3), ("run", 2), ("uppercut_R", 5)):
        pose, off, rot = forge.clip_frames(clips[clip_name], base, model, None)[frame]
        row = []
        for d in range(8):
            model.pose(pose, off, rot, d * 45.0)
            img = r.frame(model, "ink", wade.RIM)
            a = np.array(img)[..., 3] > 128
            assert a.sum() > 600, f"{clip_name} dir {d}: sprite is empty"
            frac = connected_fraction(a)
            assert frac > 0.97, f"{clip_name} dir {d}: sprite in pieces ({frac:.2f})"
            row.append(img)
        rows.append(row)
        # How far forward (-y) the lead fist is in model space, yaw 0.
        model.pose(pose, off, rot, 0.0)
        reach[clip_name] = -model.joints["wrist_L"].matrix_world.translation.y
        if clip_name == "idle":
            alb = r._render_albedo(model)
            idp = r._render(None, "id")
            inside = idp[..., 3] > 0.99
            # Away from colour boundaries the texture must return a palette colour.
            cols = np.array(list(palette.values()), dtype=np.float32)
            dist = np.abs(alb[inside][:, None, :] - cols[None]).max(-1).min(-1)
            share = (dist < 10).mean()
            assert share > 0.80, f"albedo pass lost the texture ({share:.2f} match)"
            print(f"albedo: {share:.0%} of pixels match the palette")
    assert reach["jab"] > reach["idle"] + 0.15, "the jab did not move the fist"

    sheet = Image.new("RGBA", (CELL[0] * 8, CELL[1] * len(rows)), (30, 32, 44, 255))
    for y, row in enumerate(rows):
        for x, img in enumerate(row):
            sheet.alpha_composite(img, (x * CELL[0], y * CELL[1]))
    out = os.path.join(out_dir, "selftest_skin.png")
    sheet.save(out)
    print("wrote", out)
    print("OK: skinned-mesh path works")


if __name__ == "__main__":
    main()
