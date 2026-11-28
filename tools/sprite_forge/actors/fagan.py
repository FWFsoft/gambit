"""Fagan, Purveyor of Relics. Body from art/models/Fagan.glb; his knife is
art/models/FaganKnife.glb and his lantern (the Time Crystal relic, a
caged hourglass of blue light) is art/models/FaganLantern.glb.

Shape brief: a grinning relic hunter: wild dark hair, goggles on the
brow, a blue scarf, one steel pauldron, a belt of pouches and a long tan
duster that flares to the boots. The lantern hangs at his left hip and
glows: it is the signature piece. The knife is always in his right hand.
He leans back on his heels like he is about to sell you something.

Kit (Character Concepts doc; "Characters that Probably Won't Work"):
  attack     I'll Knife Ya!: a quick underhand stab.
  secondary  Activate Relic (hold): he unhooks the lantern and holds it
             up; the first 45% is held while charging.
  special    the relic's power: the lantern thrust out at arm's length.
  mobility   Pocket Sand: a handful flung forward in a cone.

His coat hangs beside his hands: FLOORS keeps the wrists off its hem.
"""

import os

from forge import Model
import _hero as h
from _hero import HIDE, SHOW, with_
import skin

NAME = "Fagan"
MODEL = "art/models/Fagan.glb"
KNIFE = "art/models/FaganKnife.glb"
LANTERN = "art/models/FaganLantern.glb"
HEIGHT = 1.85
YAW = 0.0
STYLE = "ink"
RIM = (100, 150, 50)                 # his forest green
CELL = (196, 172, 136)
K = HEIGHT / 1.95
ARMS = [f"{j}_{s}" for j in ("shoulder", "elbow", "wrist") for s in "LR"]
# The duster hangs right beside his arms all the way up: below the belt
# the coat follows his hips, above it (to the shoulders) his chest. Only
# what lies on the arm bones themselves keeps following the arms.
FLOORS = ((ARMS, 1.0 * K, "hips", 0.10 * K),
          (ARMS, 1.5 * K, "chest", 0.10 * K))

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                floors=FLOORS)
    # The knife in his right fist, blade down along the forearm's line.
    h.prop(m, KNIFE, "knife", "wrist_R", h.palm(m, "R"), 0.40 * K,
           turn=(180, 0, 0))
    # The lantern on his left hip, and a second in his left hand for the
    # relic moves; a pose shows one or the other.
    hp = m.rest_world("hips")
    h.prop(m, LANTERN, "lantern_hip", "hips",
           (hp[0] + 0.26 * K, hp[1] - 0.02, hp[2] - 0.22 * K), 0.38 * K)
    h.prop(m, LANTERN, "lantern_hand", "wrist_L", h.palm(m, "L"), 0.38 * K,
           material="lantern_hand")
    return m, stand()


def held(lantern=False):
    return {"knife": (0, 0, 0),
            "lantern_hip": (0, 0, 0, HIDE if lantern else SHOW),
            "lantern_hand": (0, 0, 0, SHOW if lantern else HIDE)}


def stand(breath=0.0):
    """Leaning back on his heels, knife hand loose and low, left thumb
    hooked by the lantern."""
    p = {
        "hips": (4, 0, 6),
        "abdomen": (-4 + 1 * breath, 0, -2),
        "chest": (-8 + 2 * breath, 0, -4),
        "neck": (0, 0, 0),
        "head": (-6, 4, 6),
        "shoulder_L": (0, -14, 0), "elbow_L": (-30, 0, 0),
        "shoulder_R": (-20, 16, 0), "elbow_R": (-50, 0, 0),
        "hip_L": (-6, -6, -6), "knee_L": (8, 0, 0), "ankle_L": (-2, 4, 0),
        "hip_R": (6, 8, 8), "knee_R": (4, 0, 0), "ankle_R": (-8, -4, 0),
    }
    p.update(held())
    return p


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=10, arm=32, leg=34, knee=62)

    cock = with_(s, chest=(4, 0, 20), shoulder_R=(30, 30, 0), elbow_R=(-90, 0, 0))
    stab = with_(s, chest=(14, 0, -14), hips=(0, 0, -8),
                 shoulder_R=(-80, 6, 0), elbow_R=(-6, 0, 0),
                 hip_L=(-30, -6, 0), knee_L=(30, 0, 0))

    unhook = with_(s, **held(True), chest=(4, 0, 0),
                   shoulder_L=(-40, -14, 0), elbow_L=(-60, 0, 0))
    # Held up in front at head height, not overhead: a full overhead raise
    # drags the duster's upper panel up with the shoulder.
    lift = with_(s, **held(True), chest=(-8, 0, 0), head=(-12, 0, 0),
                 shoulder_L=(-105, -12, 0), elbow_L=(-55, 0, 0))
    thrust = with_(s, **held(True), chest=(10, 0, 10),
                   shoulder_L=(-88, -4, 0), elbow_L=(-4, 0, 0),
                   hip_L=(-30, -6, 0), knee_L=(28, 0, 0))

    dig = with_(s, chest=(18, 0, 14), hips=(-8, 0, 10),
                shoulder_L=(30, -14, 0), elbow_L=(-40, 0, 0),
                hip_L=(-30, -6, 0), knee_L=(40, 0, 0))
    fling = with_(s, chest=(4, 0, -16),
                  shoulder_L=(-100, -30, 0), elbow_L=(-6, 0, 0),
                  hip_L=(-30, -6, 0), knee_L=(30, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(run, K),
        h.shot("attack", 6, 18, [
            (0.0, s, o(0, 0, 0)),
            (0.4, cock, o(0, 0.02, 0)),
            (0.6, stab, o(0, -0.06, -0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.2, unhook, o(0, 0, 0)),
            (0.45, lift, o(0, 0, 0.02), "out"),
            (0.85, lift, o(0, 0, 0.02)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.2, unhook, o(0, 0, 0)),
            (0.5, thrust, o(0, -0.05, -0.02), "out"),
            (0.8, thrust, o(0, -0.05, -0.02)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 7, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.35, dig, o(0, 0.02, -0.08)),
            (0.6, fling, o(0, -0.04, 0), "out"),
            (1.0, s, o(0, 0, 0))]),
    ] + h.hit_and_death(stand, K)
