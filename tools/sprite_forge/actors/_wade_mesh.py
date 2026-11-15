"""Trial: Wade's body from his generated model, on his existing motion.

The body is art/models/Wade.glb (Meshy, from the A-pose concept art); the
guard, the punches and every other clip are the ones in wade.py, unchanged.
This is a test of the generated-model path, not the shipped Wade: the
leading underscore keeps build.py from baking it over the game's sheet.

    <py> tools/sprite_forge/fit.py _wade_mesh out.png
    <py> tools/sprite_forge/preview.py _wade_mesh idle out.png 0,6

To ship it: fix what the previews show, then move build() and FIT into
wade.py (see .claude/skills/character-art/SKILL.md, step 3).
"""

import os

from forge import Model
import skin
import wade

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Wade"
MODEL = "art/models/Wade.glb"
HEIGHT = 1.95
YAW = 0.0
STYLE = "ink"
RIM = wade.RIM
CELL = wade.CELL
FOLLOW = {}      # the sash is part of the mesh here, so nothing swings yet

# Saved from the joint editor (fit.py --editor) as art/models/Wade.fit.json.
# Until a human saves one this is empty and the automatic guess is used.
FIT = skin.load_fit(os.path.join(REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    # wade.py's clips key these two; they carry nothing on the mesh body.
    m.joint("hair", "head", m.rest_world("head"))
    m.joint("sash", "hips", m.rest_world("hips"))
    return m, wade.guard()


clips = wade.clips
