#!/usr/bin/env python3
"""Bake the art for the crash intro (ArenaIntro.cpp) from the 3D models.

    tools/sprite_forge/.venv/bin/python tools/cutscene/render_intro.py
    tools/sprite_forge/.venv/bin/python tools/cutscene/render_intro.py --only ship

Writes assets/cutscene/:
    planet.png   the planet from space, night side and atmosphere rim
    stars.png    the starfield behind it (a little wider than the screen,
                 so the shot can drift)
    ship.png     ShipPatched seen from the camera as it passes and falls
                 away, SHIP_FRAMES cells in a row-major grid

The planet is art/models/Planet.glb (Meshy "Earthlike Planet", cut down with
shrink.py). The ship is art/models/ShipPatched.glb, the intact RV before the
crash. Flames, smoke and glow are drawn by the game, not baked here.
"""

import argparse
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Euler, Vector
from PIL import Image, ImageFilter

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "assets", "cutscene")

PLANET_PX = 1100         # rendered square; the disc fills ~70% of it
STARS_SIZE = (1440, 810)
SHIP_CELL = 512
SHIP_COLUMNS = 6
# The angle between the camera and the ship's tail, frame by frame: first
# side-on as it passes the camera, then more and more from behind as it
# falls away toward the planet. Must match ArenaIntro.cpp (SHIP_FRAMES).
SHIP_VIEW_DEG = [78, 70, 62, 55, 48, 42, 37, 32, 28, 24, 21, 18,
                 15, 13, 11, 9, 7, 6]
SHIP_ROLL_DEG = 30.0     # the ship tumbles this much over the pass
SHIP_ROLL_START = 80.0   # side-on to the camera (windows, stripes) at first


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"
    world = bpy.data.worlds.new("space")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.0
    sc.world = world
    return sc


def import_model(path):
    bpy.ops.import_scene.gltf(filepath=path)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    assert meshes, path
    root = bpy.data.objects.new("root", None)
    bpy.context.scene.collection.objects.link(root)
    for o in bpy.context.scene.objects:
        if o.parent is None and o is not root:
            o.parent = root
    # Matte: the forge's look, no plastic highlights.
    for o in meshes:
        for slot in o.material_slots:
            m = slot.material
            if m and m.use_nodes:
                for n in m.node_tree.nodes:
                    if n.type == "BSDF_PRINCIPLED":
                        n.inputs["Roughness"].default_value = 0.85
                        n.inputs["Metallic"].default_value = 0.0
    return root


def sun(sc, direction, strength, color=(1, 1, 1)):
    light = bpy.data.lights.new("sun", "SUN")
    light.energy = strength
    light.color = color
    light.angle = math.radians(2.0)
    obj = bpy.data.objects.new("sun", light)
    sc.collection.objects.link(obj)
    # A sun shines along its local -Z: point +Z at where the light is from.
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    return obj


def camera(sc, location, target, fov_deg, size):
    cam = bpy.data.cameras.new("cam")
    cam.angle = math.radians(fov_deg)
    cam.clip_start = 0.01
    cam.clip_end = 1000.0
    obj = bpy.data.objects.new("cam", cam)
    sc.collection.objects.link(obj)
    obj.location = location
    obj.rotation_euler = (Vector(target) - Vector(location)).to_track_quat("-Z", "Y").to_euler()
    sc.camera = obj
    sc.render.resolution_x = size
    sc.render.resolution_y = size
    return obj


def render(sc, path):
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return np.asarray(Image.open(path).convert("RGBA")).astype(np.float32) / 255.0


def save(arr, name):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    Image.fromarray((np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)).save(path, optimize=True)
    print("wrote", os.path.relpath(path, REPO), arr.shape[1], "x", arr.shape[0])


# ---------------------------------------------------------------------------
# Planet
# ---------------------------------------------------------------------------

def bake_planet(tmp):
    sc = reset()
    sc.cycles.samples = 64
    sc.cycles.samples = 16
    root = import_model(os.path.join(REPO, "art/models/Planet.glb"))
    # Turn the side with the pink glowing cracks (the one thing that says
    # "alien" on this model; on import they face -Y, toward the camera) to
    # just left of the disc's centre, in the daylight. Facing away, the
    # planet reads as an Earth clone (playtest, issue #46).
    root.rotation_euler = Euler((math.radians(-5), 0.0, math.radians(353)))
    # Flat colour only (texture as emission): the light is added below from
    # the sphere's normals, where the day side and terminator can be placed
    # exactly.
    for m in bpy.data.materials:
        nodes, links = m.node_tree.nodes, m.node_tree.links
        tex = next(n for n in nodes if n.type == "TEX_IMAGE")
        out = next(n for n in nodes if n.type == "OUTPUT_MATERIAL")
        emit = nodes.new("ShaderNodeEmission")
        links.new(tex.outputs["Color"], emit.inputs["Color"])
        links.new(emit.outputs["Emission"], out.inputs["Surface"])
    # Camera looks along +Y at the planet (radius ~0.95).
    camera(sc, (0, -6.0, 0), (0, 0, 0), 25.5, PLANET_PX)
    img = render(sc, os.path.join(tmp, "planet_raw.png"))

    rgb, alpha = img[..., :3], img[..., 3]
    h, w = alpha.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cy, cx = h / 2.0, w / 2.0
    # The disc's radius from the alpha mask.
    radius = math.sqrt(float((alpha > 0.5).sum()) / math.pi)
    nx, ny = (xx - cx) / radius, (yy - cy) / radius
    r = np.sqrt(nx * nx + ny * ny)
    nz = np.sqrt(np.clip(1.0 - r * r, 0.0, 1.0))
    # Sun from the upper left and a little behind the camera: a lit
    # crescent-and-a-half, night over the lower right where the ship falls.
    L = np.array([-0.62, -0.48, 0.62])
    L = L / np.linalg.norm(L)
    light = nx * L[0] + ny * L[1] + nz * L[2]
    day = np.clip((light + 0.08) / 0.55, 0.0, 1.0) ** 1.2
    night = 0.05
    rgb = rgb ** 1.1 * (night + (1.0 - night) * day * 1.05)[..., None]
    # Cool the shadow; the world is dark and quiet.
    rgb = rgb * (1.0 - (1 - day)[..., None] * np.array([0.3, 0.15, 0.0]))
    light = np.clip(nx * L[0] + ny * L[1] + 0.35, -1, 1)  # for the rim

    # Atmosphere: a thin blue rim, bright on the day side, gone at night.
    rim_in = np.clip((r - 0.86) / 0.14, 0, 1) * (r <= 1.0)
    halo = np.exp(-np.clip(r - 1.0, 0, None) * 16.0) * (r > 1.0)
    rimlight = np.clip(light * 1.6 + 0.35, 0, 1)
    atmos = np.array([0.45, 0.7, 1.0])
    rgb = rgb + atmos * (rim_in ** 2 * 0.55 * rimlight)[..., None]
    glow_a = halo * 0.85 * rimlight
    glow_a = np.where(glow_a < 2.0 / 255.0, 0.0, glow_a)
    out_a = np.maximum(alpha, glow_a)
    blend = np.where(out_a > 0, alpha / np.maximum(out_a, 1e-4), 0)[..., None]
    out_rgb = rgb * blend + atmos * (1 - blend)
    save(np.dstack([out_rgb, out_a]), "planet.png")


# ---------------------------------------------------------------------------
# Stars
# ---------------------------------------------------------------------------

def bake_stars():
    w, h = STARS_SIZE
    rng = np.random.default_rng(7)
    img = np.zeros((h, w, 3), np.float32)
    # A faint band of dust across the sky, cold blue to violet.
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    band = np.exp(-((yy - h * 0.35 - (xx - w / 2) * -0.35) / (h * 0.22)) ** 2)
    noise = rng.random((h // 8 + 1, w // 8 + 1)).astype(np.float32)
    noise = np.asarray(Image.fromarray((noise * 255).astype(np.uint8)).resize(
        (w, h), Image.BICUBIC).filter(ImageFilter.GaussianBlur(18))).astype(np.float32) / 255
    dust = band * (0.35 + noise) * 0.11
    img += dust[..., None] * np.array([0.35, 0.4, 0.9])
    img += (band * noise)[..., None] ** 3 * np.array([0.5, 0.25, 0.6]) * 0.25

    def dot(x, y, bright, size, tint):
        r = int(math.ceil(size * 3))
        x0, x1 = max(0, int(x) - r), min(w, int(x) + r + 1)
        y0, y1 = max(0, int(y) - r), min(h, int(y) + r + 1)
        gy, gx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        d2 = (gx - x) ** 2 + (gy - y) ** 2
        img[y0:y1, x0:x1] += np.exp(-d2 / (2 * size * size))[..., None] * bright * tint

    tints = [np.array(t) for t in ((1, 1, 1), (0.75, 0.85, 1), (1, 0.9, 0.75), (1, 0.8, 0.8))]
    for _ in range(1600):
        dot(rng.random() * w, rng.random() * h, rng.random() ** 3 * 0.8 + 0.08,
            0.55 + rng.random() * 0.25, tints[rng.integers(len(tints))])
    for _ in range(40):  # a few bright ones
        dot(rng.random() * w, rng.random() * h, 0.9 + rng.random() * 0.6,
            0.9 + rng.random() * 0.5, tints[rng.integers(len(tints))])
    save(np.dstack([img, np.ones((h, w), np.float32)]), "stars.png")


# ---------------------------------------------------------------------------
# Ship
# ---------------------------------------------------------------------------

def bake_ship(tmp):
    sc = reset()
    sc.cycles.samples = 48
    root = import_model(os.path.join(REPO, "art/models/ShipPatched.glb"))
    # The model's length is X, nozzles at +X. Turned so the nose points -Z
    # (away from the camera, toward the planet) and the nozzles face +Z.
    key = sun(sc, Vector((0, 0, 1)), 3.5, (1.0, 0.96, 0.9))
    # Fire light on the hull, from the burning boosters.
    fire = bpy.data.lights.new("fire", "POINT")
    fire.energy = 25.0
    fire.color = (1.0, 0.45, 0.15)
    fire.shadow_soft_size = 0.4
    fire_obj = bpy.data.objects.new("fire", fire)
    sc.collection.objects.link(fire_obj)
    rim = bpy.data.lights.new("rim", "SUN")
    rim.energy = 1.2
    rim.color = (0.45, 0.65, 1.0)  # planet light from ahead
    rim_obj = bpy.data.objects.new("rim", rim)
    sc.collection.objects.link(rim_obj)
    rim_obj.rotation_euler = Vector((0, 0, 1)).to_track_quat("Z", "Y").to_euler()

    cells = []
    n = len(SHIP_VIEW_DEG)
    for i, deg in enumerate(SHIP_VIEW_DEG):
        u = i / max(1, n - 1)
        # Model X is the length. Rotate so the nose points -Z (away), tumbling.
        root.rotation_euler = Euler((0.0, math.radians(-90), 0.0))
        root.rotation_euler.rotate(Euler((0.0, 0.0, math.radians(SHIP_ROLL_DEG * u + SHIP_ROLL_START))))
        # The camera sits up and to the left of the ship's tail axis.
        th = math.radians(deg)
        phi = math.radians(40)
        d = Vector((-math.sin(th) * math.cos(phi), math.sin(th) * math.sin(phi), math.cos(th)))
        cam_pos = d * 7.0
        fire_obj.location = (0.0, 0.0, 1.35)
        # Key light from the camera's side, above and to the left: the same
        # sun that lights the planet's day side.
        source = (d + Vector((-0.3, 0.5, 0.0))).normalized()
        key.rotation_euler = source.to_track_quat("Z", "Y").to_euler()
        camera(sc, cam_pos, (0, 0, 0), 19.0, SHIP_CELL)
        cells.append(render(sc, os.path.join(tmp, f"ship_{i:02d}.png")))
        for o in list(bpy.data.objects):
            if o.type == "CAMERA":
                bpy.data.objects.remove(o)

    rows = (n + SHIP_COLUMNS - 1) // SHIP_COLUMNS
    sheet = np.zeros((rows * SHIP_CELL, SHIP_COLUMNS * SHIP_CELL, 4), np.float32)
    for i, c in enumerate(cells):
        r, k = divmod(i, SHIP_COLUMNS)
        sheet[r * SHIP_CELL:(r + 1) * SHIP_CELL, k * SHIP_CELL:(k + 1) * SHIP_CELL] = c
    # Premultiplied-looking edges: keep colour under zero alpha clean.
    sheet[..., :3] *= (sheet[..., 3:] > 0.0)
    save(sheet, "ship.png")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--only", choices=("planet", "stars", "ship"))
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else None)
    tmp = os.environ.get("TMPDIR", "/tmp")
    if args.only in (None, "stars"):
        bake_stars()
    if args.only in (None, "planet"):
        bake_planet(tmp)
    if args.only in (None, "ship"):
        bake_ship(tmp)


if __name__ == "__main__":
    main()
