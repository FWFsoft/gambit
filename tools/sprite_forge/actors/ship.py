"""Ship: the crew's crashed RV-turned-spaceship, nose down in the rubble.

The body is art/models/Ship.glb (Meshy, "Crashed RV Spaceship"). It is
drawn at the same scale as the heroes: an RV with rockets strapped to the
back, about LENGTH metres from bumper to nozzles, next to people under 2 m
tall. The ship never moves or turns; the game draws one direction (the
one that reads best, ArenaView's SHIP_DIRECTION).

One still frame per direction.
"""

import os

from forge import Clip, Key, Model
from kade_mirror import attach_prop

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Ship"
MODEL = "art/models/Ship.glb"
STYLE = "ink"
RIM = (255, 190, 120)        # warm: the one lit thing the crew calls home
LENGTH = 11.0                # metres, bumper to nozzles
# The model is normalised by height; it is 1.352 times longer than tall
# (measured from the file), so this height gives LENGTH.
HEIGHT = LENGTH / 1.352
CELL = (640, 560, 440)       # w, h, ground line from the top (std pixels)


def build():
    m = Model(NAME)
    m.joint("hull", None, (0.0, 0.0, 0.0))
    attach_prop(m, os.path.join(REPO, MODEL), "hull", HEIGHT, material="hull")
    return m, {"hull": (0, 0, 0)}


def clips():
    still = {"hull": (0, 0, 0)}
    return [Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                      Key(1.0, still, (0, 0, 0))])]
