"""Venoxarid elite: the enforcer. Body from art/models/VenoxaridElite.glb.

Shape brief: the grunt's boss, one and a half times his height and twice
his width. Spiked leather shoulders and gauntlets square off the outline,
and the glowing venom tank on his back with its hose is the signature
piece: it shows over his shoulder from every side. Motion is shared with
the grunt (_venox_thug.py).
"""

import os

import _venox_thug as thug
import skin

NAME = "VenoxaridElite"
MODEL = "art/models/VenoxaridElite.glb"
SIZE = 1.5                           # times the grunt
HEIGHT = 1.95 * SIZE
YAW = 0.0
# Degrees the model's head is turned to its own left in the A-pose; the
# poses turn it back (measured by eye from the fit render; tune by looking).
HEAD_TURN = 12.0
STYLE = "ink"
RIM = (190, 255, 60)                 # the venom tank's glow
CELL = (264, 234, 192)               # the grunt's cell, times SIZE

# Joint positions: art/models/<Name>.fit.json (placed by a human in the
# joint editor, fit.py --editor).
FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, HEAD_TURN)


def clips():
    return thug.clips(SIZE, HEAD_TURN)
