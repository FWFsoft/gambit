"""ShipTurret: the roof cannon the crew bolts onto the ship's hull.

The body is art/models/ShipTurret.glb (Meshy, "RV Spaceship Roof Cannon",
cut down with shrink.py). It is baked as two sheets so the cannon can
swivel on its round base in the game:

  ShipTurretBase  the bolted foot, below the slewing ring. One direction
                  is enough; it never turns.
  ShipTurret      everything above the ring: the gun housing and barrel.
                  The forge's 8 directions are the barrel pointing S, SE,
                  E, ... (ship_turret_tween.py bakes the 8 in between, so
                  the game has 16 headings).

All three share one cell and one pivot, the centre of the ring on the
ground line, so the game draws them at the same point and the head turns
about the ring. The model's barrel points along -X; HEAD_FACE turns it to
the forge's front (-Y), so direction 0 is the barrel pointing at the
camera.

Clips: idle, one still frame per direction.
"""

import os

import bmesh
import numpy as np

from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "ShipTurret"
MODEL = "art/models/ShipTurret.glb"
STYLE = "ink"
RIM = (150, 255, 190)        # the "turrets online" green
HEIGHT = 1.9                 # metres, foot to the top of the housing
CELL = (224, 184, 136)       # w, h, ground line from the top (std pixels)
# Measured on the model normalised to height 1 (feet on z = 0): the
# slewing ring sits at z 0.25 to 0.30, centred at (0.17, -0.04), and the
# barrel runs out along -X.
RING_Z = 0.27
RING_CENTRE = (0.17, -0.04)
HEAD_FACE = 90.0             # barrel from -X to the front (-Y)


def attach_part(model, joint, keep_above):
    """The model's part above (or below) the ring, rigid on `joint`, with
    the ring's centre on the joint."""
    obj, v = skin.load_mesh(os.path.join(REPO, MODEL), HEIGHT)
    split = RING_Z * HEIGHT
    drop = v[:, 2] < split if keep_above else v[:, 2] >= split
    assert drop.any() and not drop.all(), "the split left nothing to draw"
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[bm.verts[i] for i in np.flatnonzero(drop)],
                     context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    v = v[~drop] - np.array([RING_CENTRE[0] * HEIGHT,
                             RING_CENTRE[1] * HEIGHT, 0.0])
    assert len(v) == len(obj.data.vertices), "vertex order changed"
    rest = np.array(model.rest_world(joint))
    v = v + rest
    model.material("gun", (220, 214, 200))
    model.materials["gun"].textured = True
    s = skin.Skin(obj, v, [joint], {joint: rest}, np.ones((len(v), 1)),
                  model.materials["gun"].bmat)
    s.obj.name = f"{model.name}_gun"
    skin._set_coords(s.obj.data, v)
    model.skins.append(s)
    return s


def build_head(name, extra_yaw):
    m = Model(name)
    m.joint("head", None, (0.0, 0.0, 0.0))
    attach_part(m, "head", keep_above=True)
    return m, {"head": (0, 0, HEAD_FACE + extra_yaw)}


def build():
    return build_head(NAME, 0.0)


def still_clips(joint, rot):
    still = {joint: rot}
    return [Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                      Key(1.0, still, (0, 0, 0))])]


def clips():
    return still_clips("head", (0, 0, HEAD_FACE))
