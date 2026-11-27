"""Valthor, the Sovereign (Defender). Body from art/models/Valthor.glb.

Shape brief: a white-haired commander in a long, high-collared royal blue
greatcoat with gold buttons and belt, a glowing visor, armoured gauntlets.
He stands like he owns the ground: chest out, chin up, left fist planted
on his hip with the elbow jutting out, the right gauntlet hanging
clenched and a little forward.

Kit (Character Concepts doc):
  attack     Dominion Pulse: a gauntlet thrust forward, palm open.
  secondary  Aegis of Retribution (hold): feet planted, both gauntlets
             forward, bracing the barrier; the first 45% is the stance
             held while charging.
  special    Sovereign Domain: one arm sweeps out and down, claiming the
             ground.
  mobility   Judgment of the Sovereign: a fist raised high and brought
             down. (His kit has no movement ability; the slot plays this.)
"""

import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Valthor"
MODEL = "art/models/Valthor.glb"
HEIGHT = 2.0
YAW = 0.0
STYLE = "ink"
RIM = (190, 200, 210)                # his silver
CELL = (188, 172, 136)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    return m, stand()


def stand(breath=0.0):
    """Feet apart, chest out, left fist on the hip, right gauntlet
    clenched at his side."""
    return {
        "hips": (0, 0, 0),
        "abdomen": (-4 + 1 * breath, 0, 0),
        "chest": (-6 + 2 * breath, 0, 0),
        "neck": (2, 0, 0),
        "head": (-6, 0, 0),
        "shoulder_L": (6, -42 - 2 * breath, 0), "elbow_L": (-100, 0, 0),
        "shoulder_R": (-12, 10 + 2 * breath, 0), "elbow_R": (-24, 0, 0),
        "hip_L": (-2, -8, -4), "knee_L": (2, 0, 0), "ankle_L": (0, 6, 0),
        "hip_R": (-2, 8, 4), "knee_R": (2, 0, 0), "ankle_R": (0, -6, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    march = h.run_pose(lambda b=0.0: with_(stand(b), shoulder_L=(0, -10, 0),
                                           shoulder_R=(0, 10, 0)),
                       lean=4, arm=30, leg=32, knee=56)

    draw = with_(s, chest=(-4, 0, -20), hips=(0, 0, -10),
                 shoulder_R=(10, 30, 0), elbow_R=(-110, 0, 0),
                 shoulder_L=(-20, -20, 0), elbow_L=(-40, 0, 0))
    thrust = with_(s, chest=(6, 0, 16), hips=(0, 0, 10),
                   shoulder_R=(-90, 4, 0), elbow_R=(-4, 0, 0),
                   shoulder_L=(10, -24, 0), elbow_L=(-50, 0, 0),
                   hip_L=(-26, -8, 0), knee_L=(24, 0, 0))

    brace = with_(s, hips=(-10, 0, 0), abdomen=(10, 0, 0), chest=(10, 0, 0),
                  head=(-4, 0, 0),
                  shoulder_L=(-80, -16, 0), elbow_L=(-20, 0, 0),
                  shoulder_R=(-80, 16, 0), elbow_R=(-20, 0, 0),
                  hip_L=(-36, -12, 0), knee_L=(40, 0, 0),
                  hip_R=(14, 12, 0), knee_R=(24, 0, 0))
    push = with_(brace, chest=(14, 0, 0), shoulder_L=(-90, -10, 0),
                 shoulder_R=(-90, 10, 0), elbow_L=(-4, 0, 0), elbow_R=(-4, 0, 0))

    claim_up = with_(s, chest=(-8, 0, 20), shoulder_R=(-120, 50, 0),
                     elbow_R=(-10, 0, 0))
    claim = with_(s, chest=(10, 0, -20), shoulder_R=(-40, 70, 0),
                  elbow_R=(-6, 0, 0), hip_R=(-20, 10, 0), knee_R=(20, 0, 0))

    raise_ = with_(s, chest=(-14, 0, 0), head=(-16, 0, 0),
                   shoulder_R=(-175, 10, 0), elbow_R=(-20, 0, 0))
    down = with_(s, hips=(-12, 0, 0), abdomen=(16, 0, 0), chest=(20, 0, 0),
                 shoulder_R=(-40, 10, 0), elbow_R=(-10, 0, 0),
                 hip_L=(-50, -10, 0), knee_L=(70, 0, 0),
                 hip_R=(-30, 10, 0), knee_R=(80, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(march, K, fps=11),
        h.shot("attack", 7, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.4, draw, o(0, 0.03, 0)),
            (0.6, thrust, o(0, -0.06, -0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, brace, o(0, 0, -0.08)),
            (0.6, push, o(0, -0.08, -0.08), "out"),
            (0.85, push, o(0, -0.08, -0.08)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.4, claim_up, o(0, 0, 0.02)),
            (0.65, claim, o(0, -0.03, -0.04), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, raise_, o(0, 0, 0.04)),
            (0.6, down, o(0, -0.04, -0.20), "in"),
            (0.85, down, o(0, -0.04, -0.20)),
            (1.0, s, o(0, 0, 0))]),
    ] + h.hit_and_death(stand, K)
