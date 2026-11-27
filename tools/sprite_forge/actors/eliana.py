"""Eliana, the Witch (Winger). Body from art/models/Eliana.glb.

Shape brief: a gothic witch in violet. A great mass of curly purple hair
is the top of her outline; a long, ragged purple coat flares from the
hips to the shins and widens her below the waist, in a different way
from Lilith's red hem. Lean and upright, one hand on her hip, the other
raised at her chest with the fingers curled, mid-hex.

Kit (Character Concepts doc):
  attack     Hex: a flick of the raised hand toward the target.
  secondary  Weft and Weave (hold): both hands weaving in front of her,
             arms opening wider as it grows.
  special    Unmake: reach out, close the fist, rip it back.
  mobility   Sanctuary Totem: crouch and plant it in the ground.
"""

import math
import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Eliana"
MODEL = "art/models/Eliana.glb"
HEIGHT = 1.9
YAW = 0.0
STYLE = "ink"
RIM = (240, 230, 255)                # her registry colour: pale lilac light
CELL = (196, 172, 132)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    return m, stand()


def stand(breath=0.0):
    """Hip cocked, left hand on the hip, right hand up at the chest."""
    return {
        "hips": (0, 0, -6),
        "abdomen": (-2 + 1 * breath, 0, 2),
        "chest": (-4 + 2 * breath, 0, 4),
        "neck": (0, 0, 0),
        "head": (-4, 6, -6),
        "shoulder_L": (4, -30 - 2 * breath, 0), "elbow_L": (-90, 0, 0),
        "shoulder_R": (-40, 14, 0), "elbow_R": (-100 - 6 * breath, 0, 0),
        "hip_L": (-6, -4, 0), "knee_L": (10, 0, 0), "ankle_L": (-4, 0, 0),
        "hip_R": (2, 6, 0), "knee_R": (2, 0, 0), "ankle_R": (0, 0, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=6, arm=28, leg=30, knee=56)

    cock = with_(s, chest=(-6, 0, -16), shoulder_R=(-80, 30, 0),
                 elbow_R=(-120, 0, 0))
    flick = with_(s, chest=(6, 0, 12), shoulder_R=(-90, 0, 0),
                  elbow_R=(-6, 0, 0), hip_L=(-24, -4, 0), knee_L=(20, 0, 0))

    def weave(t, wide):
        a = 20 * math.sin(t * 2 * math.pi)
        return with_(s, chest=(-6, 0, 0), head=(-10, 0, 0),
                     shoulder_L=(-70 + a, -20 - 40 * wide, 0), elbow_L=(-60 + 20 * wide, 0, 0),
                     shoulder_R=(-70 - a, 20 + 40 * wide, 0), elbow_R=(-60 + 20 * wide, 0, 0),
                     hip_L=(-4, -10, 0), hip_R=(-4, 10, 0))

    reach = with_(s, chest=(14, 0, 10), shoulder_R=(-95, 6, 0), elbow_R=(-4, 0, 0),
                  hip_L=(-30, -4, 0), knee_L=(24, 0, 0))
    rip = with_(s, chest=(-12, 0, -14), head=(-12, 0, 0),
                shoulder_R=(-20, 30, 0), elbow_R=(-130, 0, 0),
                hip_R=(10, 6, 0), knee_R=(20, 0, 0))

    kneel = with_(s, hips=(-10, 0, 0), abdomen=(20, 0, 0), chest=(24, 0, 0),
                  shoulder_R=(-50, 10, 0), elbow_R=(-10, 0, 0),
                  hip_L=(-90, -8, 0), knee_L=(110, 0, 0), ankle_L=(-20, 0, 0),
                  hip_R=(-10, 8, 0), knee_R=(120, 0, 0), ankle_R=(40, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(run, K),
        h.shot("attack", 7, 16, [
            (0.0, s, o(0, 0, 0)),
            (0.4, cock, o(0, 0.03, 0)),
            (0.6, flick, o(0, -0.05, -0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 12, 12, [(i / 12, weave(i / 6, i / 12), o(0, 0, 0), "linear")
                                     for i in range(13)], overlap=0.02),
        h.shot("special", 8, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.35, reach, o(0, -0.06, -0.02), "out"),
            (0.5, reach, o(0, -0.06, -0.02)),
            (0.7, rip, o(0, 0.06, 0), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, kneel, o(0, 0, -0.42), "in"),
            (0.7, kneel, o(0, 0, -0.44)),
            (1.0, s, o(0, 0, 0))]),
    ] + h.hit_and_death(stand, K)
