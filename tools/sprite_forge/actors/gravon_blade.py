"""GravonBlade: Gravon himself, with no host.

When the host falls the player *is* the blade (Soulblade). The game draws
it from this sheet, in two clips of one still frame per direction:

  floor   stuck point-first in the floor where the host fell, leaning a
          little, taller than anyone in the arena: the thing to run to.
  back    on an ally's back, on the same diagonal Gravon wears it, drawn at
          the carrier's feet and facing the carrier's way.

Same model and size as on Gravon (gravon.BLADE_MODEL, gravon.BLADE_HEIGHT),
so the swap from his sheet to this one does not change its size.
"""

import os

from forge import Clip, Key, Model
import gravon
from nyx import attach_prop

NAME = "GravonBlade"
STYLE = "ink"
RIM = gravon.RIM
PIXEL_SCALE = gravon.PIXEL_SCALE
CELL = (200, 200, 150)

FLOOR_LEAN = 12.0          # degrees off vertical, toward his left
FLOOR_SINK = 0.10          # how far the point is in the floor
# On a carrier: the blade's centre, from their feet. Carriers run from
# 1.85 to 2.2 tall; this sits on a 1.95 back.
ON_BACK = (-0.10, 0.20, 1.24)


def build():
    m = Model(NAME)
    h = gravon.BLADE_HEIGHT
    blade = os.path.join(gravon.REPO, gravon.BLADE_MODEL)
    # Each mount's joint is at the blade's centre, so a turn pivots there.
    m.joint("floor", None, (0.0, 0.0, h / 2.0 - FLOOR_SINK))
    attach_prop(m, blade, "floor", h, (0.0, 0.0, 0.0), material="floor")
    m.joint("back", None, ON_BACK)
    attach_prop(m, blade, "back", h, (0.0, 0.0, 0.0), material="back")
    return m, {"floor": (0, 0, 0, 1), "back": (0, 0, 0, 0)}


def clips():
    floor = {"floor": (0, FLOOR_LEAN, 0, 1), "back": (0, 0, 0, 0)}
    back = {"floor": (0, 0, 0, 0), "back": (0, gravon.BACK_TILT, 0, 1)}
    low = (0, 0, 0)
    return [Clip("floor", 1, 1, True, [Key(0.0, floor, low), Key(1.0, floor, low)]),
            Clip("back", 1, 1, True, [Key(0.0, back, low), Key(1.0, back, low)])]
