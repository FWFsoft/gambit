"""Mordryn, the Shadowguard, in Guardian form. Body from
art/models/Mordryn.glb; his Demon form is mordryn_demon.py, a separate
sheet the game swaps to when Demonic Rage fills.

Shape brief: a masked shadow-fighter: spiky black hair, a long dark red
scarf over the face whose tails stream off one shoulder, a torn black
vest-cape over a bare, scarred chest, a mechanical pauldron on the left
shoulder, glowing teal bracers. His left leg is already the demon's: raw
red, swollen and clawed below the knee, the transformation waiting. That
one red leg is his signature from the front and side. Low and ready,
weight on the human leg.

The cape and scarf pull the automatic centring to one side: CENTRE_X
(measured from the front of his face) puts the centre line back.

Kit (Character Concepts doc, Guardian Form):
  attack     Shadow Slash: a fast, narrow arc of the right hand.
  secondary  Nightfall Strike (hold): coil low, then a two-handed
             cleave; the first 45% is held while charging.
  special    Midnight Ward: forearms crossed in front of the face.
  mobility   Phantom Assault: a long, low leap at the target.
"""

import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Mordryn"
MODEL = "art/models/Mordryn.glb"
HEIGHT = 1.95
YAW = 0.0
CENTRE_X = 0.10
STYLE = "ink"
RIM = (60, 220, 200)                 # his bracers' teal
CELL = (212, 180, 140)
K = HEIGHT / 1.95
ARMS = [f"{j}_{s}" for j in ("shoulder", "elbow", "wrist") for s in "LR"]
# The torn cape hangs beside his arms: below the belt it follows the hips,
# above it the chest.
FLOORS = ((ARMS, 1.0 * K, "hips", 0.10 * K),
          (ARMS, 1.5 * K, "chest", 0.10 * K))

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                centre_x=CENTRE_X, floors=FLOORS)
    return m, stand()


def stand(breath=0.0):
    """Low and ready: weight on the human right leg, the demon leg out in
    front, hands loose and open at the hips."""
    return {
        "hips": (-6, 0, -10),
        "abdomen": (6 + 1 * breath, 0, 4),
        "chest": (8 + 2 * breath, 0, 6),
        "neck": (-4, 0, 0),
        "head": (-6, 0, 6),
        "shoulder_L": (-20, -20 - 2 * breath, 0), "elbow_L": (-40, 0, 0),
        "shoulder_R": (-10, 22 + 2 * breath, 0), "elbow_R": (-30, 0, 0),
        "hip_L": (-26, -10, -10), "knee_L": (24, 0, 0), "ankle_L": (-6, 6, 0),
        "hip_R": (-6, 10, 10), "knee_R": (30, 0, 0), "ankle_R": (-20, -6, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=18, arm=40, leg=40, knee=72)

    draw = with_(s, chest=(4, 0, 30), hips=(-6, 0, 20),
                 shoulder_R=(-60, -30, 0), elbow_R=(-120, 0, 0))
    slash = with_(s, chest=(12, 0, -30), hips=(-6, 0, -14),
                  shoulder_R=(-80, 70, 0), elbow_R=(-6, 0, 0),
                  hip_L=(-36, -10, 0), knee_L=(36, 0, 0))

    coil = with_(s, hips=(-20, 0, 30), abdomen=(20, 0, 10), chest=(20, 0, 20),
                 shoulder_L=(20, -10, 0), elbow_L=(-60, 0, 0),
                 shoulder_R=(20, 30, 0), elbow_R=(-60, 0, 0),
                 hip_L=(-50, -10, 0), knee_L=(70, 0, 0),
                 hip_R=(-30, 10, 0), knee_R=(70, 0, 0))
    cleave = with_(s, hips=(-10, 0, -20), abdomen=(16, 0, -10), chest=(24, 0, -20),
                   shoulder_L=(-90, -20, 0), elbow_L=(-10, 0, 0),
                   shoulder_R=(-90, 20, 0), elbow_R=(-10, 0, 0),
                   hip_L=(-50, -10, 0), knee_L=(50, 0, 0))

    ward = with_(s, chest=(-4, 0, 0), head=(4, 0, 0),
                 shoulder_L=(-70, 20, 0), elbow_L=(-110, 0, 0),
                 shoulder_R=(-70, -20, 0), elbow_R=(-110, 0, 0),
                 hip_L=(-20, -14, 0), hip_R=(-20, 14, 0))

    crouch = with_(s, hips=(-20, 0, 0), abdomen=(24, 0, 0), chest=(24, 0, 0),
                   shoulder_L=(30, -10, 0), shoulder_R=(30, 10, 0),
                   hip_L=(-70, -8, 0), knee_L=(100, 0, 0),
                   hip_R=(-70, 8, 0), knee_R=(100, 0, 0))
    leap = with_(s, hips=(10, 0, 0), abdomen=(14, 0, 0), chest=(20, 0, 0),
                 head=(-20, 0, 0),
                 shoulder_L=(-120, -10, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(30, 20, 0), elbow_R=(-30, 0, 0),
                 hip_L=(-60, -8, 0), knee_L=(70, 0, 0),
                 hip_R=(30, 8, 0), knee_R=(40, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(run, K, fps=13),
        h.shot("attack", 6, 18, [
            (0.0, s, o(0, 0, 0)),
            (0.4, draw, o(0, 0.02, 0)),
            (0.6, slash, o(0, -0.06, -0.03), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, coil, o(0, 0.04, -0.22)),
            (0.6, cleave, o(0, -0.12, -0.10), "out"),
            (0.85, cleave, o(0, -0.12, -0.10)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.35, ward, o(0, 0.02, -0.04), "out"),
            (0.8, ward, o(0, 0.02, -0.04)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 10, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.3, crouch, o(0, 0.04, -0.32)),
            (0.6, leap, o(0, -0.55, 0.40), "out"),
            (0.85, crouch, o(0, -0.85, -0.25), "in"),
            (1.0, s, o(0, -0.85, 0))]),
    ] + h.hit_and_death(stand, K)
