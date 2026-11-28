"""Zephyr, the Galewalker (Playmaker). Body from art/models/Zephyr.glb;
the speaker pack on his back is art/models/ZephyrPack.glb.

Shape brief: a white-haired wind monk in goggles, a high-collared navy
vest over bare orange arms with turbine gauntlets, and a long, ragged
violet robe from the belt down. The pack on his back, six orange turbine
speakers in a black frame, is the signature piece: a square block behind
his shoulders from the side and the back. Light and ready, weight
forward on the balls of his feet, one gauntlet raised.

His robe hem hangs beside his hands; FLOORS keeps the wrists off it.

Kit (src/ArenaData.cpp):
  attack     Wind Slash: a fast horizontal chop of the arm.
  secondary  Vortex (hold): both hands circling in front of him, drawing
             the air in; the first 45% is held while charging.
  special    Gale's Embrace: arms sweep up and round, wrapping air.
  mobility   Tempest Dash: a low, long lunge forward.
"""

import math
import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Zephyr"
MODEL = "art/models/Zephyr.glb"
PACK = "art/models/ZephyrPack.glb"
HEIGHT = 1.95
YAW = 0.0
STYLE = "ink"
RIM = (120, 230, 255)                # storm blue
CELL = (204, 180, 140)
K = HEIGHT / 1.95
ARMS = [f"{j}_{s}" for j in ("shoulder", "elbow", "wrist") for s in "LR"]
FLOORS = ((ARMS, 0.95 * K, "hips", 0.09 * K),)

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                floors=FLOORS)
    # Square to his back, turbines facing out behind him.
    h.prop(m, PACK, "pack", "chest", h.back(m, 0.24, 0.10), 0.62 * K,
           turn=(0, 0, 180))
    return m, stand()


def stand(breath=0.0):
    """Weight forward, knees soft, right gauntlet raised at the shoulder,
    left arm loose."""
    return {
        "pack": (0, 0, 0),
        "hips": (-6, 0, 8),
        "abdomen": (4 + 1 * breath, 0, -4),
        "chest": (6 + 2 * breath, 0, -6),
        "neck": (0, 0, 0),
        "head": (-4, 0, 6),
        "shoulder_L": (-10, -16 - 2 * breath, 0), "elbow_L": (-30, 0, 0),
        "shoulder_R": (-50, 24, 0), "elbow_R": (-100, 0, 0),
        "hip_L": (-14, -8, -6), "knee_L": (18, 0, 0), "ankle_L": (-6, 6, 0),
        "hip_R": (-6, 8, 6), "knee_R": (16, 0, 0), "ankle_R": (-10, -6, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=16, arm=36, leg=40, knee=72)

    chamber = with_(s, chest=(0, 0, 30), hips=(-6, 0, 20),
                    shoulder_R=(-80, -30, 0), elbow_R=(-110, 0, 0))
    chop = with_(s, chest=(8, 0, -30), hips=(-6, 0, -14),
                 shoulder_R=(-86, 60, 0), elbow_R=(-4, 0, 0),
                 hip_L=(-30, -8, 0), knee_L=(30, 0, 0))

    def circle(t):
        a = 30 * math.sin(t * 2 * math.pi)
        b = 30 * math.cos(t * 2 * math.pi)
        return with_(s, chest=(4, 0, 0),
                     shoulder_L=(-70 + a, -30 + b * 0.5, 0), elbow_L=(-50, 0, 0),
                     shoulder_R=(-70 - a, 30 - b * 0.5, 0), elbow_R=(-50, 0, 0),
                     hip_L=(-20, -12, 0), hip_R=(-20, 12, 0))

    gather = with_(s, chest=(10, 0, 0), shoulder_L=(-20, -10, 0),
                   shoulder_R=(-20, 10, 0), elbow_L=(-80, 0, 0), elbow_R=(-80, 0, 0),
                   hip_L=(-30, -10, 0), knee_L=(40, 0, 0),
                   hip_R=(-30, 10, 0), knee_R=(40, 0, 0))
    wrap = with_(s, chest=(-14, 0, 0), head=(-14, 0, 0),
                 shoulder_L=(-160, -40, 0), elbow_L=(-40, 0, 0),
                 shoulder_R=(-160, 40, 0), elbow_R=(-40, 0, 0))

    lunge = with_(s, hips=(-10, 0, 0), abdomen=(24, 0, 0), chest=(30, 0, 0),
                  head=(-20, 0, 0),
                  shoulder_L=(40, -16, 0), elbow_L=(-10, 0, 0),
                  shoulder_R=(40, 16, 0), elbow_R=(-10, 0, 0),
                  hip_L=(-70, -8, 0), knee_L=(70, 0, 0),
                  hip_R=(30, 8, 0), knee_R=(10, 0, 0), ankle_R=(30, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(run, K, fps=13),
        h.shot("attack", 6, 18, [
            (0.0, s, o(0, 0, 0)),
            (0.4, chamber, o(0, 0.02, 0)),
            (0.6, chop, o(0, -0.05, -0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 12, 12, [(i / 12, circle(i / 6), o(0, 0, -0.03), "linear")
                                     for i in range(13)], overlap=0.02),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.35, gather, o(0, 0, -0.12)),
            (0.65, wrap, o(0, 0, 0.06), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 6, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.4, lunge, o(0, -0.35, -0.22), "out"),
            (0.8, lunge, o(0, -0.55, -0.22)),
            (1.0, s, o(0, -0.6, 0))]),
    ] + h.hit_and_death(stand, K)
