"""Lilith, the Temptress (Winger). Body from art/models/Lilith.glb; the
forbidden fruit in her right hand is art/models/LilithApple.glb, a
swirling crimson orb she always carries and raises for Forbidden Fruit.

Shape brief: a crimson witch-hunter. The wide black hat brim is the top
of her outline and nobody else has it; puffed red sleeves square her
shoulders; a long red coat with a ragged, flared hem widens her to the
ankles. Poised and still: weight on one leg, one hand on the hip, the
other held loose at her chest.

The coat hem hangs beside her hands: FLOORS keeps the wrists from pulling
it (skin.prepare), so it follows her hips.

Kit (Character Concepts doc):
  attack     Serpent's Whisper: a wide backhand sweep, a cone.
  secondary  Forbidden Fruit (hold): she raises the fruit overhead.
  special    The Lovers: one hand reaching out to the bonded ally, the
             other on her heart.
  mobility   Veil of Night: a turn that sinks into shadow.
"""

import math
import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Lilith"
MODEL = "art/models/Lilith.glb"
APPLE = "art/models/LilithApple.glb"
HEIGHT = 1.95
YAW = 0.0
STYLE = "ink"
RIM = (150, 50, 255)                 # her registry colour
CELL = (204, 176, 136)
K = HEIGHT / 1.95
ARMS = [f"{j}_{s}" for j in ("shoulder", "elbow", "wrist") for s in "LR"]
FLOORS = ((ARMS, 0.95 * K, "hips", 0.09 * K),)

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                floors=FLOORS)
    h.prop(m, APPLE, "apple", "wrist_R", h.palm(m, "R"), 0.15 * K)
    return m, stand()


def stand(breath=0.0):
    """Weight on the right leg, left hand on the hip, right hand loose at
    the chest, chin down under the brim."""
    return {
        "hips": (0, 0, 6),
        "abdomen": (-2 + 1 * breath, 0, -2),
        "chest": (-2 + 2 * breath, 0, -4),
        "neck": (0, 0, 0),
        "head": (6, -4, 4),
        "shoulder_L": (4, -28 - 2 * breath, 0), "elbow_L": (-90, 0, 0),
        "shoulder_R": (-30, 10, 0), "elbow_R": (-80, 0, 0),
        "hip_L": (-8, -4, 0), "knee_L": (12, 0, 0), "ankle_L": (-4, 0, 0),
        "hip_R": (2, 4, 0), "knee_R": (2, 0, 0), "ankle_R": (0, 0, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=6, arm=26, leg=28, knee=52)

    wind = with_(s, chest=(-4, 0, 30), hips=(0, 0, 14),
                 shoulder_R=(-60, -20, 0), elbow_R=(-110, 0, 0))
    sweep = with_(s, chest=(4, 0, -30), hips=(0, 0, -10),
                  shoulder_R=(-80, 60, 0), elbow_R=(-6, 0, 0),
                  hip_L=(-26, -4, 0), knee_L=(20, 0, 0))

    def offer(t):
        return with_(s, chest=(-10, 0, 0), head=(-20, 0, 0),
                     shoulder_R=(-170, 10 + 4 * t, 0), elbow_R=(-20, 0, 0),
                     shoulder_L=(-30, -40, 0), elbow_L=(-40, 0, 0))

    lovers = with_(s, chest=(6, 0, 10),
                   shoulder_R=(-85, 10, 0), elbow_R=(-10, 0, 0),
                   shoulder_L=(-40, -4, 0), elbow_L=(-120, 0, 0),
                   hip_L=(-24, -4, 0), knee_L=(16, 0, 0))

    twirl1 = with_(s, hips=(0, 0, 90), chest=(0, 0, 30),
                   shoulder_L=(-20, -70, 0), shoulder_R=(-20, 70, 0),
                   elbow_L=(-10, 0, 0), elbow_R=(-10, 0, 0))
    sink = with_(twirl1, hips=(-20, 0, 180), abdomen=(20, 0, 0), chest=(20, 0, 30),
                 hip_L=(-70, -8, 0), knee_L=(110, 0, 0),
                 hip_R=(-70, 8, 0), knee_R=(110, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(run, K),
        h.shot("attack", 8, 16, [
            (0.0, s, o(0, 0, 0)),
            (0.4, wind, o(0, 0.02, 0)),
            (0.6, sweep, o(0, -0.05, -0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, offer(0), o(0, 0, 0.03), "out"),
            (0.7, offer(1), o(0, 0, 0.04)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.4, lovers, o(0, -0.04, 0), "out"),
            (0.75, lovers, o(0, -0.04, 0)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.4, twirl1, o(0, 0, 0)),
            (0.75, sink, o(0, 0, -0.45), "in"),
            (1.0, sink, o(0, 0, -0.9), "in")]),
    ] + h.hit_and_death(stand, K)
