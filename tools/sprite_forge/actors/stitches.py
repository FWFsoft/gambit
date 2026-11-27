"""Stan "Stitches" Stanislofsky, the Stowaway (Winger). Body from
art/models/Stitches.glb; his bag is art/models/StitchesBag.glb.

Shape brief: a hulking, grinning back-alley medic. Blood-spattered,
ragged white coat, huge bare hands, a strap across his chest to the
battered duffle bag slung across his back, which is the signature piece:
it widens his shoulders from behind and the side. He stands wide and
loose, big hands open, like he is about to grab you for your own good.

Kit (Character Concepts doc):
  attack     Shrug Off: a self-heal: he thumps his own chest and roars.
  secondary  I'll patch you up (hold): crouch, a big leap, land with his
             hands out. The first 45% is the crouch held while charging.
  special    Let me see that: reach out, grab, yank it back to himself.
  mobility   Bloody Blur: a hunched sprint.
"""

import math
import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Stitches"
MODEL = "art/models/Stitches.glb"
BAG = "art/models/StitchesBag.glb"
HEIGHT = 2.05
YAW = 0.0
STYLE = "ink"
RIM = (100, 200, 50)                 # his lime green
CELL = (200, 184, 144)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    # Across his back, below the shoulder blades.
    h.prop(m, BAG, "bag", "chest", h.back(m, 0.26, 0.12), 0.80, turn=(0, 8, 0))
    return m, stand()


def stand(breath=0.0):
    """Wide and loose, shoulders rolled forward, big hands open."""
    return {
        "bag": (0, 0, 0),
        "hips": (-4, 0, 0),
        "abdomen": (6 + 2 * breath, 0, 0),
        "chest": (10 + 3 * breath, 0, 0),
        "neck": (-6, 0, 0),
        "head": (-4, 0, 0),
        "shoulder_L": (-14, -18 - 3 * breath, 0), "elbow_L": (-30, 0, 0),
        "shoulder_R": (-14, 18 + 3 * breath, 0), "elbow_R": (-30, 0, 0),
        "hip_L": (-10, -10, -8), "knee_L": (16, 0, 0), "ankle_L": (-4, 8, 0),
        "hip_R": (-10, 10, 8), "knee_R": (16, 0, 0), "ankle_R": (-4, -8, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=14, arm=36, leg=34, knee=64)

    def blur(phase):
        p = h.run_pose(stand, lean=30, arm=50, leg=42, knee=80)(phase)
        return with_(p, head=(-20, 0, 0))

    thump = with_(s, chest=(-14, 0, 0), head=(-26, 0, 0),
                  shoulder_L=(-60, -10, 0), elbow_L=(-120, 0, 0),
                  shoulder_R=(-60, 10, 0), elbow_R=(-120, 0, 0))
    roar = with_(s, chest=(-20, 0, 0), head=(-30, 0, 0),
                 shoulder_L=(-30, -70, 0), elbow_L=(-70, 0, 0),
                 shoulder_R=(-30, 70, 0), elbow_R=(-70, 0, 0),
                 hip_L=(-14, -14, 0), hip_R=(-14, 14, 0))

    coil = with_(s, hips=(-30, 0, 0), abdomen=(24, 0, 0), chest=(24, 0, 0),
                 head=(-14, 0, 0),
                 shoulder_L=(40, -20, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(40, 20, 0), elbow_R=(-20, 0, 0),
                 hip_L=(-80, -10, 0), knee_L=(110, 0, 0), ankle_L=(-30, 0, 0),
                 hip_R=(-80, 10, 0), knee_R=(110, 0, 0), ankle_R=(-30, 0, 0))
    air = with_(s, chest=(-6, 0, 0), head=(-10, 0, 0),
                shoulder_L=(-120, -40, 0), elbow_L=(-20, 0, 0),
                shoulder_R=(-120, 40, 0), elbow_R=(-20, 0, 0),
                hip_L=(-50, -8, 0), knee_L=(80, 0, 0),
                hip_R=(-20, 8, 0), knee_R=(40, 0, 0))
    land = with_(coil, shoulder_L=(-70, -30, 0), shoulder_R=(-70, 30, 0),
                 elbow_L=(-10, 0, 0), elbow_R=(-10, 0, 0))

    grab = with_(s, hips=(0, 0, 14), chest=(16, 0, 16),
                 shoulder_R=(-90, 10, 0), elbow_R=(-6, 0, 0),
                 shoulder_L=(10, -24, 0), elbow_L=(-40, 0, 0),
                 hip_L=(-36, -10, 0), knee_L=(40, 0, 0))
    yank = with_(s, hips=(0, 0, -10), chest=(-10, 0, -14), head=(-10, 0, 0),
                 shoulder_R=(-30, 20, 0), elbow_R=(-130, 0, 0),
                 hip_R=(10, 10, 0), knee_R=(24, 0, 0))

    return [
        h.breathe(stand, K, fps=7),
        h.stride(run, K, fps=11),
        h.shot("attack", 8, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.25, thump, o(0, 0.04, 0), "out"),
            (0.4, with_(thump, chest=(-4, 0, 0)), o(0, 0.02, 0)),
            (0.55, thump, o(0, 0.04, 0), "out"),
            (0.75, roar, o(0, 0, 0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 12, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.45, coil, o(0, 0.04, -0.30)),
            (0.65, air, o(0, -0.5, 0.55), "out"),
            (0.85, land, o(0, -1.0, -0.28), "in"),
            (1.0, s, o(0, -1.0, 0))]),
        h.shot("special", 8, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.35, grab, o(0, -0.10, -0.04), "out"),
            (0.55, grab, o(0, -0.10, -0.04)),
            (0.8, yank, o(0, 0.06, 0), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.stride(blur, K, name="mobility", fps=16, bounce=0.05, low=0.04),
    ] + h.hit_and_death(stand, K)
