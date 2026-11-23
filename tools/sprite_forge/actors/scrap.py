"""Scrap: a pile of salvage, the crew's currency (dropped by kills, carried,
banked at the ship).

The body is art/models/Scrap.glb (Meshy, "Enemy_Scrap_Loot_Drop_3D", cut
down with shrink.py to 20,000 triangles). It never turns; the game draws
direction 0 for a pickup on the ground and for a load carried overhead,
scaling it with the amount.

It is knee high (HEIGHT): small enough to sit under a character's feet as
loot, big enough to read as machinery at game zoom.

Clips: idle, one still frame per direction.
"""

import os

import numpy as np

from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Scrap"
MODEL = "art/models/Scrap.glb"
STYLE = "ink"
RIM = (255, 200, 110)        # the warm amber of scrap labels and hauling
HEIGHT = 0.6                 # metres, ground to the top of the pile
CELL = (128, 96, 76)         # w, h, ground line from the top (std pixels)


def build():
    m = Model(NAME)
    m.joint("pile", None, (0.0, 0.0, 0.0))
    obj, v = skin.load_mesh(os.path.join(REPO, MODEL), HEIGHT)
    rest = np.array(m.rest_world("pile"))
    v = v + rest
    m.material("scrap", (220, 214, 200))
    m.materials["scrap"].textured = True
    s = skin.Skin(obj, v, ["pile"], {"pile": rest}, np.ones((len(v), 1)),
                  m.materials["scrap"].bmat)
    s.obj.name = f"{NAME}_scrap"
    skin._set_coords(s.obj.data, v)
    m.skins.append(s)
    return m, {"pile": (0, 0, 0)}


def clips():
    still = {"pile": (0, 0, 0)}
    return [Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                      Key(1.0, still, (0, 0, 0))])]
