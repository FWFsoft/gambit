"""ShipPatched: the ship's first upgrade tier. The crew has dug the RV out of
the crater, set it level on its wheels and patched it loosely.

The body is art/models/ShipPatched.glb (Meshy, "Patched RV Spaceship",
cut down with shrink.py). It is drawn at the same scale as the crashed
ship (ship.py): about LENGTH metres bumper to nozzles. The ship never
moves; the game draws one direction (ArenaView's SHIP_TIER_DIRECTION).

One still frame per direction.
"""

import os

from forge import Clip, Key, Model
from kade_mirror import attach_prop

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "ShipPatched"
MODEL = "art/models/ShipPatched.glb"
STYLE = "ink"
RIM = (255, 190, 120)        # warm: the one lit thing the crew calls home
LENGTH = 11.0                # metres, bumper to nozzles
# Normalised by height, the model is 2.786 times longer than tall.
HEIGHT = LENGTH / 2.786
CELL = (720, 520, 330)       # w, h, ground line from the top (std pixels)


def build():
    m = Model(NAME)
    m.joint("hull", None, (0.0, 0.0, 0.0))
    attach_prop(m, os.path.join(REPO, MODEL), "hull", HEIGHT, material="hull")
    return m, {"hull": (0, 0, 0)}


def clips():
    still = {"hull": (0, 0, 0)}
    return [Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                      Key(1.0, still, (0, 0, 0))])]
