"""Mordryn, the Shadowguard, in Demon form. Body from
art/models/MordrynDemon.glb. His Guardian form is mordryn.py; the game
swaps sheets when Demonic Rage fills, as it swaps Jeff's legs and treads.

Shape brief: a hulking, many-eyed brute: a fanged maw where the head
should be, raw pink hide studded with eyes, both arms gone to molten
claws wrapped in chains, and one great burning bat wing off his left
shoulder. The single wing makes him lopsided from every side; nobody
else in the cast is. Hunched low, claws hanging, head thrust forward.

The wing pulls the automatic centring off his body: CENTRE_X (measured
from his feet and shins) puts his centre line back.

Kit (Character Concepts doc, Demon Form):
  attack     Infernal Cleave: a wide two-claw rake in front of him.
  secondary  Hellfire Burst (hold): crouch and gather, then fling the
             arms wide; the first 45% is held while charging.
  special    Demonic Frenzy: a roar, claws up.
  mobility   Abyssal Eruption: a short dash and a rising uppercut.
"""

import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "MordrynDemon"
MODEL = "art/models/MordrynDemon.glb"
HEIGHT = 2.3
YAW = 0.0
CENTRE_X = -0.18
STYLE = "ink"
RIM = (255, 96, 40)                  # hellfire
CELL = (264, 220, 168)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                centre_x=CENTRE_X)
    return m, stand()


def stand(breath=0.0):
    """Hunched, maw forward, claws hanging wide and low."""
    return {
        "hips": (-10, 0, 0),
        "abdomen": (12 + 3 * breath, 0, 0),
        "chest": (16 + 4 * breath, 0, 0),
        "neck": (-16, 0, 0),
        "head": (-10, 0, 0),
        "shoulder_L": (-20, -22 - 3 * breath, 0), "elbow_L": (-30, 0, 0),
        "shoulder_R": (-20, 22 + 3 * breath, 0), "elbow_R": (-30, 0, 0),
        "hip_L": (-20, -14, -8), "knee_L": (30, 0, 0), "ankle_L": (-10, 10, 0),
        "hip_R": (-20, 14, 8), "knee_R": (30, 0, 0), "ankle_R": (-10, -10, 0),
    }


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(stand, lean=24, arm=40, leg=36, knee=70)

    raise_ = with_(s, chest=(-6, 0, 0), head=(-14, 0, 0),
                   shoulder_L=(-130, -40, 0), elbow_L=(-30, 0, 0),
                   shoulder_R=(-130, 40, 0), elbow_R=(-30, 0, 0))
    rake = with_(s, hips=(-20, 0, 0), abdomen=(24, 0, 0), chest=(30, 0, 0),
                 shoulder_L=(-40, 20, 0), elbow_L=(-10, 0, 0),
                 shoulder_R=(-40, -20, 0), elbow_R=(-10, 0, 0),
                 hip_L=(-46, -14, 0), knee_L=(56, 0, 0))

    gather = with_(s, hips=(-20, 0, 0), abdomen=(24, 0, 0), chest=(28, 0, 0),
                   shoulder_L=(-30, 10, 0), elbow_L=(-110, 0, 0),
                   shoulder_R=(-30, -10, 0), elbow_R=(-110, 0, 0),
                   hip_L=(-50, -14, 0), knee_L=(70, 0, 0),
                   hip_R=(-50, 14, 0), knee_R=(70, 0, 0))
    burst = with_(s, chest=(-16, 0, 0), head=(-20, 0, 0),
                  shoulder_L=(-60, -90, 0), elbow_L=(-6, 0, 0),
                  shoulder_R=(-60, 90, 0), elbow_R=(-6, 0, 0),
                  hip_L=(-10, -20, 0), hip_R=(-10, 20, 0))

    roar = with_(s, abdomen=(-10, 0, 0), chest=(-20, 0, 0), head=(-26, 0, 0),
                 shoulder_L=(-150, -30, 0), elbow_L=(-60, 0, 0),
                 shoulder_R=(-150, 30, 0), elbow_R=(-60, 0, 0))

    dash = with_(s, hips=(-14, 0, 0), abdomen=(24, 0, 0), chest=(30, 0, 0),
                 shoulder_R=(30, 20, 0), elbow_R=(-60, 0, 0),
                 hip_L=(-60, -10, 0), knee_L=(70, 0, 0),
                 hip_R=(20, 10, 0), knee_R=(20, 0, 0))
    upper = with_(s, chest=(-14, 0, -20), head=(-20, 0, 0),
                  shoulder_R=(-170, 10, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-20, -10, 0), knee_L=(20, 0, 0), ankle_L=(30, 0, 0))

    return [
        h.breathe(stand, K, fps=7, sink=0.02),
        h.stride(run, K, fps=11),
        h.shot("attack", 8, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.4, raise_, o(0, 0.04, 0.02)),
            (0.6, rake, o(0, -0.10, -0.10), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, gather, o(0, 0, -0.18)),
            (0.6, burst, o(0, 0, 0.04), "out"),
            (0.85, burst, o(0, 0, 0.04)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.3, roar, o(0, 0.02, 0.02), "out"),
            (0.8, roar, o(0, 0.02, 0.02)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 8, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.35, dash, o(0, -0.40, -0.12), "out"),
            (0.6, upper, o(0, -0.55, 0.20), "out"),
            (1.0, s, o(0, -0.55, 0))]),
    ] + h.hit_and_death(stand, K)
