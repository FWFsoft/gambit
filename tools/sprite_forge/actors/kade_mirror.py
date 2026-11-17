"""KadeMirror: one of Kade's prismatic mirrors, standing where he set it.

The body is art/models/KadeMirror.glb (Meshy, from "Kade's Prismatic Mirror
Arsenal"). The model's glass faces +y at rest, so the prop turns 180 degrees
to face the forge's front (-y); the game picks the direction column so the
glass faces along its barrier.

It is as tall as a person (MIRROR_HEIGHT, Kade's height): stepping through
one and a beam striking one both read at that scale. The concept sheet has
it smaller (about 1.4 m); the human asked for character height.

Clips:
  idle    one still frame per direction
  deploy  stands up out of the floor where Kade slapped it: tipped flat, it
          swings upright, overshoots and settles
"""

import os

import numpy as np

from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "KadeMirror"
MODEL = "art/models/KadeMirror.glb"
MIRROR_HEIGHT = 1.9
STYLE = "ink"
RIM = (60, 150, 255)                 # the glass's VHS blue
CELL = (176, 156, 128)               # same cell as Kade: the same scale on screen
FACE = 180.0                         # turns the glass to the front


def attach_prop(model, path, joint, height, material="mirror"):
    """A model file as a rigid part of `joint`, standing on the joint."""
    obj, v = skin.load_mesh(path, height)
    rest = model.rest_world(joint)
    v = v + np.array(rest)
    model.material(material, (220, 214, 200))
    model.materials[material].textured = True
    weights = np.ones((len(v), 1))
    s = skin.Skin(obj, v, [joint], {joint: np.array(rest)}, weights,
                  model.materials[material].bmat)
    s.obj.name = f"{model.name}_{material}"
    skin._set_coords(s.obj.data, v)
    model.skins.append(s)
    return s


def build():
    m = Model(NAME)
    m.joint("mirror", None, (0.0, 0.0, 0.0))
    attach_prop(m, os.path.join(REPO, MODEL), "mirror", MIRROR_HEIGHT)
    return m, {"mirror": (0, 0, FACE)}


def stand(squash=1.0):
    """The mirror on its feet; squash < 1 flattens it on landing."""
    stretch = 1.0 / max(0.5, squash) ** 0.5
    return {"mirror": (0, 0, FACE, stretch, stretch, squash)}


def clips():
    still = stand()
    idle = Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                     Key(1.0, still, (0, 0, 0))])
    # Stands up out of the floor where his hand struck: tipped flat, it
    # swings upright, overshoots and settles.
    def tipped(deg, squash=1.0):
        k = stand(squash)
        k["mirror"] = (deg,) + k["mirror"][1:]
        return k
    deploy = Clip("deploy", 8, 20, False, [
        Key(0.0, tipped(84, 1.0), (0, 0, 0.0)),
        Key(0.45, tipped(-10, 1.04), (0, 0, 0.0), ease="out"),   # swings up
        Key(0.65, tipped(4, 0.94), (0, 0, 0.0)),
        Key(1.0, still, (0, 0, 0.0), ease="in"),
    ])
    return [idle, deploy]
