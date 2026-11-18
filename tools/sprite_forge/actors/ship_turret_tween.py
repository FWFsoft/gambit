"""ShipTurretTween: the roof cannon's head, 22.5 degrees past each of the
forge's 8 directions, so the game can swivel it through 16 headings.
Direction i of this sheet is the barrel pointing (i * 45 + 22.5) degrees
clockwise from straight down. See ship_turret.py."""

from ship_turret import CELL, HEAD_FACE, RIM, STYLE, build_head, still_clips  # noqa: F401

NAME = "ShipTurretTween"
TWEEN = 22.5    # the forge turns the model counter-clockwise from above


def build():
    return build_head(NAME, TWEEN)


def clips():
    return still_clips("head", (0, 0, HEAD_FACE + TWEEN))
