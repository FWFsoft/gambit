"""ShipFort: the ship's second upgrade tier. Less a ship than a fort: sandbag
walls, corrugated sheet, stairs, solar panels and a dish, a hint that
there may be no way off this planet.

The body is art/models/ShipFort.glb (Meshy, "Fortified RV Outpost",
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

NAME = "ShipFort"
MODEL = "art/models/ShipFort.glb"
STYLE = "ink"
RIM = (255, 190, 120)        # warm: the one lit thing the crew calls home
LENGTH = 12.5                # metres across the sandbags, so the RV reads at 11 m
# Normalised by height, the model is 1.910 times longer than tall.
HEIGHT = LENGTH / 1.910
CELL = (760, 600, 380)       # w, h, ground line from the top (std pixels)


def build():
    m = Model(NAME)
    m.joint("hull", None, (0.0, 0.0, 0.0))
    attach_prop(m, os.path.join(REPO, MODEL), "hull", HEIGHT, material="hull")
    return m, {"hull": (0, 0, 0)}


def clips():
    still = {"hull": (0, 0, 0)}
    return [Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                      Key(1.0, still, (0, 0, 0))])]
