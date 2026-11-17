#!/usr/bin/env python3
"""Look at how a model file sits on the forge skeleton, before baking.

    <forge python> tools/sprite_forge/fit.py wade_mesh out.png      # an actor
    <forge python> tools/sprite_forge/fit.py art/models/Wade.glb out.png --height 1.95

Writes one image with four panels:

  1. front, as imported, with the joints drawn on it
  2. side, as imported, with the joints
  3. front, coloured by which joint owns each part of the surface
  4. front, re-posed into the forge rest pose (arms and legs straight down)

and prints the full joint fit, ready to paste into the actor as FIT and
correct. Coordinates are in forge units after normalising: x is the
character's left, -y is forward, z is up, feet at z = 0.

With --editor page.html it also writes a small web page with the front and
side views, where a human drags the joints into place and saves. An agent
publishes that page as an Artifact, gives the human the link, then reads the
saved fit back and writes it to art/models/<Name>.fit.json, which
skin.load_fit() gives to the actor. The steps are in
.claude/skills/character-art/SKILL.md, step 2.

What to check:
  * every joint sits inside the body at the real joint (elbows at the
    elbows, knees at the knees, shoulders inside the deltoids);
  * the colours in panel 3 change at the joints, and no arm colour leaks
    onto the torso or thigh;
  * panel 4 looks like the same character standing with arms at the sides.
"""

import argparse
import base64
import hashlib
import importlib
import io
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "actors"))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

import forge  # noqa: E402
import skin  # noqa: E402

PANEL_W, PANEL_H = 520, 560
BACKGROUND = (58, 62, 74)


def joint_colours(n):
    out = []
    for i in range(n):
        out.append(forge._hsv_to_rgb((i * 0.618034) % 1.0, 0.75, 0.95))
    return np.array(out, dtype=np.float64) / 255.0


class Views:
    def __init__(self, height, scale=1):
        self.w, self.h = PANEL_W * scale, PANEL_H * scale
        self.ps = height * 1.12 / self.h
        self.zc = height / 2
        self.r = forge.Renderer(self.w, self.h, self.h // 2, self.ps)
        self.r.camera.data.shift_y = 0.0
        self.r.camera.data.ortho_scale = max(self.w, self.h) * self.ps

    def shoot(self, side, srgb):
        cam = self.r.camera
        if side:
            cam.location = (20.0, 0.0, self.zc)
            cam.rotation_euler = (math.pi / 2, 0.0, math.pi / 2)
        else:
            cam.location = (0.0, -20.0, self.zc)
            cam.rotation_euler = (math.pi / 2, 0.0, 0.0)
        bpy.context.view_layer.update()
        sc = bpy.context.scene
        sc.cycles.samples, sc.cycles.filter_width = 8, 1.5
        img = self.r._render(None, "fit")
        rgb, a = np.clip(img[..., :3], 0, 1), img[..., 3:4]
        if srgb:
            rgb = np.where(rgb <= 0.0031308, rgb * 12.92,
                           1.055 * np.power(rgb, 1 / 2.4) - 0.055)
        bg = np.array(BACKGROUND) / 255.0
        return Image.fromarray(((rgb * a + bg * (1 - a)) * 255).astype(np.uint8))

    def project(self, p, side):
        u = p[1] if side else p[0]
        return (self.w / 2 + u / self.ps, self.h / 2 - (p[2] - self.zc) / self.ps)


def draw_joints(img, views, positions, skeleton, side, tails=None):
    d = ImageDraw.Draw(img)
    for name, parent in skeleton:
        if parent is not None:
            d.line([views.project(positions[parent], side),
                    views.project(positions[name], side)], fill=(255, 255, 255), width=2)
    for name, tail in (tails or {}).items():
        if tail in positions and name in positions and tail not in dict(skeleton):
            d.line([views.project(positions[name], side),
                    views.project(positions[tail], side)], fill=(170, 170, 170), width=1)
    for name, _ in skeleton:
        x, y = views.project(positions[name], side)
        d.ellipse((x - 4, y - 4, x + 4, y + 4), fill=(255, 60, 60), outline=(0, 0, 0))
        if not side and not name.endswith("_R"):
            d.text((x + 6, y - 5), name, fill=(255, 255, 255))
    return img


def write_editor(out, path, height, fit):
    """The joint editor: the front and side views with nothing drawn on
    them, plus the numbers the page needs to turn pixels back into forge
    coordinates. The mesh must already wear its unlit materials."""
    views = Views(height, scale=2)
    images = []
    for side in (False, True):
        buf = io.BytesIO()
        views.shoot(side, True).save(buf, "JPEG", quality=88)
        images.append("data:image/jpeg;base64," + base64.b64encode(buf.getvalue()).decode())
    with open(path, "rb") as f:
        model_id = hashlib.sha1(f.read()).hexdigest()[:12]
    name = os.path.splitext(os.path.basename(path))[0]
    payload = {
        "name": name, "model_id": model_id, "height": height,
        "w": views.w, "h": views.h, "ps": views.ps, "zc": views.zc,
        "front": images[0], "side": images[1],
        "skeleton": skin.HUMANOID, "tails": skin.HUMANOID_TAILS,
        "fit": {k: [round(c, 4) for c in p] for k, p in fit.items()},
    }
    with open(os.path.join(HERE, "fit_editor.html")) as f:
        page = f.read()
    assert "/*FIT_DATA*/null" in page, "fit_editor.html lost its data slot"
    page = page.replace("/*FIT_DATA*/null", json.dumps(payload))
    page = page.replace("<title>Joint Fit</title>", f"<title>{name} Joint Fit</title>")
    with open(out, "w") as f:
        f.write(page)
    print("wrote", out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", help="an actor module name or a model file")
    ap.add_argument("out")
    ap.add_argument("--height", type=float, default=None)
    ap.add_argument("--yaw", type=float, default=None)
    ap.add_argument("--no-cache", action="store_true")
    ap.add_argument("--editor", metavar="HTML",
                    help="also write a page where a human drags the joints "
                         "into place (see the docstring)")
    args = ap.parse_args()

    fit, height, yaw, path = None, args.height, args.yaw, args.source
    centre_x = 0.0
    if not os.path.splitext(args.source)[1]:
        mod = importlib.import_module(args.source)
        path = mod.MODEL
        fit = getattr(mod, "FIT", None)
        height = height or mod.HEIGHT
        yaw = getattr(mod, "YAW", 0.0) if yaw is None else yaw
        centre_x = getattr(mod, "CENTRE_X", 0.0)
    if not os.path.isabs(path) and not os.path.exists(path):
        path = os.path.join(REPO, path)
    height = height or 1.95

    bpy.ops.wm.read_factory_settings(use_empty=True)
    data = skin.prepare(path, height, fit, yaw=yaw or 0.0, cache=not args.no_cache,
                        centre_x=centre_x)
    obj, mesh = data["obj"], data["obj"].data
    views = Views(height)

    # The mesh's own colours, unlit.
    originals = list(mesh.materials)
    albedo = [skin._texture_emission(m, f"fit_albedo_{i}")[0]
              for i, m in enumerate(originals)] or []
    for i, m in enumerate(albedo):
        mesh.materials[i] = m
    if args.editor:
        write_editor(args.editor, path, height, data["fit"])
        for i, m in enumerate(albedo):      # write_editor rendered; restore
            mesh.materials[i] = m
        views = Views(height)
    front = draw_joints(views.shoot(False, True), views, data["fit"],
                        skin.HUMANOID, False, skin.HUMANOID_TAILS)
    side = draw_joints(views.shoot(True, True), views, data["fit"],
                       skin.HUMANOID, True, skin.HUMANOID_TAILS)

    skin._set_coords(mesh, data["rest"])
    rest = draw_joints(views.shoot(False, True), views, data["rest_pos"],
                       skin.HUMANOID, False)
    skin._set_coords(mesh, data["imported"])

    # Who owns what.
    colours = data["weights"] @ joint_colours(data["weights"].shape[1])
    attr = mesh.color_attributes.new("forge_weights", "FLOAT_COLOR", "POINT")
    attr.data.foreach_set("color", np.concatenate(
        [colours, np.ones((len(colours), 1))], 1).astype(np.float32).ravel())
    wm = bpy.data.materials.new("fit_weights")
    wm.use_nodes = True
    nt = wm.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    an = nt.nodes.new("ShaderNodeAttribute")
    an.attribute_name = "forge_weights"
    nt.links.new(an.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs[0], out.inputs[0])
    for i in range(len(mesh.materials)):
        mesh.materials[i] = wm
    weights = views.shoot(False, False)

    sheet = Image.new("RGB", (PANEL_W * 4, PANEL_H + 24), (20, 20, 26))
    d = ImageDraw.Draw(sheet)
    for i, (img, label) in enumerate(((front, "1. front, as imported"),
                                      (side, "2. side (faces left)"),
                                      (weights, "3. skin weights by joint"),
                                      (rest, "4. forge rest pose"))):
        sheet.paste(img, (i * PANEL_W, 24))
        d.text((i * PANEL_W + 8, 6), label, fill=(255, 255, 255))
    sheet.save(args.out)

    print(f"{os.path.basename(path)}: {len(mesh.vertices)} vertices, "
          f"{len(mesh.polygons)} faces, height {height}")
    print("FIT = {")
    for name in sorted(data["fit"], key=lambda n: (n.endswith("_R"), n)):
        if name.endswith("_R"):
            continue                      # mirrored from the left side
        p = data["fit"][name]
        print(f'    "{name}": ({p[0]:.3f}, {p[1]:.3f}, {p[2]:.3f}),')
    print("}")
    print("wrote", args.out)


if __name__ == "__main__":
    main()
