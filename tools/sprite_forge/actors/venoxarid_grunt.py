"""Venoxarid grunt: the street thug. Body from art/models/VenoxaridGrunt.glb.

Shape brief: a hero-sized brawler, upright and slouched. The swept-back
green crest is the top of his outline; the open purple vest and baggy
striped track pants make the middle. Motion is in _venox_thug.py.
"""

import os

import _venox_thug as thug
import skin

NAME = "VenoxaridGrunt"
MODEL = "art/models/VenoxaridGrunt.glb"
HEIGHT = 1.95                        # the average hero (Wade 1.95, Nolan 1.9)
YAW = 0.0
# Degrees the model's head is turned to its own left in the A-pose; the
# poses turn it back (measured by eye from the fit render; tune by looking).
HEAD_TURN = 18.0
STYLE = "ink"
RIM = (150, 232, 64)                 # Venoxarid acid green
CELL = (176, 156, 128)               # the heroes' cell

# Joint positions: art/models/<Name>.fit.json (placed by a human in the
# joint editor, fit.py --editor).
FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, HEAD_TURN)


def clips():
    return thug.clips(head_turn=HEAD_TURN)
