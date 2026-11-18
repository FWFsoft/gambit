"""ShipTurretBase: the roof cannon's bolted foot, below the slewing ring.
It never turns; the game draws direction 0 under the swivelling head.
See ship_turret.py."""

from forge import Model
from ship_turret import CELL, RIM, STYLE, attach_part, still_clips  # noqa: F401

NAME = "ShipTurretBase"


def build():
    m = Model(NAME)
    m.joint("foot", None, (0.0, 0.0, 0.0))
    attach_part(m, "foot", keep_above=False)
    return m, {"foot": (0, 0, 0)}


def clips():
    return still_clips("foot", (0, 0, 0))
