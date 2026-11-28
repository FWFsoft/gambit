"""Volgore, the Berserker (Defender). Body from art/models/Volgore.glb.

Shape brief: a roaring mountain of muscle in camouflage cargo pants and
combat boots, blue war paint over orange skin, a short blue mohawk,
heavy wrist wraps. He is the widest human in the cast: shoulders and
arms make a broad triangle, and he stands hunched forward with the arms
hanging wide and the fists half-closed, mouth open, ready to grab.

Kit (src/ArenaData.cpp):
  attack     Hamstring: a low backhand swing across the shins.
  secondary  Hooked (hold): reach out with both hands, then haul back
             into his chest; the first 45% is the reach held while
             charging.
  special    Ferocious Bellow: chest up, arms flung back, roaring.
  mobility   Rage Unending: he beats his chest, two blows, and goes
             Berserk.
"""

import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "Volgore"
MODEL = "art/models/Volgore.glb"
HEIGHT = 2.2
YAW = 0.0
STYLE = "ink"
RIM = (255, 50, 50)                  # his rage red
CELL = (220, 196, 152)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    return m, stand()


def stand(breath=0.0):
    """Hunched forward, arms hanging wide, fists half-closed."""
    return {
        "hips": (-8, 0, 0),
        "abdomen": (10 + 3 * breath, 0, 0),
        "chest": (14 + 4 * breath, 0, 0),
        "neck": (-14, 0, 0),
        "head": (-10, 0, 0),
        "shoulder_L": (-16, -30 - 4 * breath, 0), "elbow_L": (-24, 0, 0),
        "shoulder_R": (-16, 30 + 4 * breath, 0), "elbow_R": (-24, 0, 0),
        "hip_L": (-16, -14, -8), "knee_L": (24, 0, 0), "ankle_L": (-8, 10, 0),
        "hip_R": (-16, 14, 8), "knee_R": (24, 0, 0), "ankle_R": (-8, -10, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=20, arm=40, leg=36, knee=66)

    wind = with_(s, hips=(-10, 0, 30), chest=(16, 0, 30),
                 shoulder_R=(-40, 70, 0), elbow_R=(-40, 0, 0))
    swing = with_(s, hips=(-14, 0, -20), abdomen=(20, 0, 0), chest=(26, 0, -30),
                  shoulder_R=(-50, -10, 0), elbow_R=(-10, 0, 0),
                  hip_L=(-40, -14, 0), knee_L=(56, 0, 0))

    reach = with_(s, chest=(24, 0, 0), head=(-20, 0, 0),
                  shoulder_L=(-90, -20, 0), elbow_L=(-6, 0, 0),
                  shoulder_R=(-90, 20, 0), elbow_R=(-6, 0, 0),
                  hip_L=(-40, -14, 0), knee_L=(44, 0, 0))
    haul = with_(s, chest=(-10, 0, 0), head=(-6, 0, 0),
                 shoulder_L=(10, -40, 0), elbow_L=(-120, 0, 0),
                 shoulder_R=(10, 40, 0), elbow_R=(-120, 0, 0),
                 hip_R=(10, 14, 0), knee_R=(30, 0, 0))

    bellow = with_(s, abdomen=(-10, 0, 0), chest=(-20, 0, 0), neck=(-10, 0, 0),
                   head=(-20, 0, 0),
                   shoulder_L=(30, -60, 0), elbow_L=(-30, 0, 0),
                   shoulder_R=(30, 60, 0), elbow_R=(-30, 0, 0),
                   hip_L=(-20, -18, 0), hip_R=(-20, 18, 0))

    beat_l = with_(s, chest=(-6, 0, 6), head=(-20, 0, 0),
                   shoulder_L=(-70, -10, 0), elbow_L=(-130, 0, 0),
                   shoulder_R=(-30, 40, 0), elbow_R=(-70, 0, 0))
    beat_r = with_(s, chest=(-6, 0, -6), head=(-20, 0, 0),
                   shoulder_R=(-70, 10, 0), elbow_R=(-130, 0, 0),
                   shoulder_L=(-30, -40, 0), elbow_L=(-70, 0, 0))

    return [
        h.breathe(stand, K, fps=7, sink=0.02),
        h.stride(run, K, fps=11),
        h.shot("attack", 7, 16, [
            (0.0, s, o(0, 0, 0)),
            (0.4, wind, o(0, 0.03, -0.02)),
            (0.6, swing, o(0, -0.06, -0.08), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, reach, o(0, -0.10, -0.04)),
            (0.62, haul, o(0, 0.08, 0), "out"),
            (0.85, haul, o(0, 0.08, 0)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 9, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.3, bellow, o(0, 0.02, 0.02), "out"),
            (0.8, bellow, o(0, 0.02, 0.02)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.25, beat_l, o(0, 0, 0), "out"),
            (0.5, beat_r, o(0, 0, 0), "out"),
            (0.75, bellow, o(0, 0, 0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
    ] + h.hit_and_death(stand, K)
