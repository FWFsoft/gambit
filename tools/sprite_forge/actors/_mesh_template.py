"""Template: a character whose body is a generated model (image-to-3D).

Copy to actors/<name>.py and work through the TODOs. The steps, the checks
and what to ask the human for are in .claude/skills/character-art/SKILL.md.

The body comes from MODEL: its shape and its texture carry the concept
art's drawing. This file adds what a generated mesh cannot: the fit of the
skeleton, the poses and clips (motion is where the feel lives), and any
part that must move on its own (a sword that changes hands, a cape or sash
on springs), built in code on extra joints as before.
"""

import os

from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Template"                    # TODO: the character's name in the game
MODEL = "art/models/Template.glb"    # TODO: repo-relative path of the model
HEIGHT = 1.95                        # forge units; Wade is 1.95, Presidente 2.2
YAW = 0.0                            # degrees, if the model does not face -Y
STYLE = "ink"                        # required for skinned meshes
RIM = (255, 176, 72)                 # TODO: the character's signature colour
CELL = (176, 156, 128)               # cell width, height, feet line (px)

# Joint positions on the imported mesh. Run fit.py, paste what it prints,
# then correct the joints that sit wrong. Only the left side and the centre
# line are needed; the right side mirrors the left unless you give it.
FIT = {}


def build():
    m = Model(NAME)
    path = MODEL if os.path.isabs(MODEL) else os.path.join(REPO, MODEL)
    skin.attach(m, path, HEIGHT, FIT, yaw=YAW)
    # TODO: props and springy cloth on their own joints, for example:
    #   m.material("steel", (150, 156, 170))
    #   m.joint("sword", "wrist_R", m.rest_world("wrist_R"))
    #   m.box("sword", "steel", ...)
    return m, stand()


def stand(breath=0.0):
    """TODO: the character's own idle pose. Pose who they are (see the
    shape brief in docs/design/ART_DIRECTION.md)."""
    return {
        "hips": (0, 0, 8),
        "abdomen": (4 + 2 * breath, 0, -2),
        "chest": (4 + 3 * breath, 0, -4),
        "head": (2, 0, -2),
        "shoulder_L": (-8, -10, 0), "elbow_L": (-22, 0, 0),
        "shoulder_R": (-4, 10, 0), "elbow_R": (-26, 0, 0),
        "hip_L": (-8, -8, -6), "knee_L": (10, 0, 0), "ankle_L": (-2, 8, 0),
        "hip_R": (4, 8, 8), "knee_R": (10, 0, 0), "ankle_R": (-12, -8, 0),
    }


def clips():
    """TODO: idle, run, hit, death, plus one clip per ability (the names
    are mapped in actionClip() in src/ArenaView.cpp)."""
    import math
    idle = [Key(f / 8, stand(0.5 - 0.5 * math.cos(2 * math.pi * f / 8)),
                (0, 0, -0.01), ease="linear") for f in range(9)]
    return [Clip("idle", 8, 8, True, idle, overlap=0.03)]
