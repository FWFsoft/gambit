"""Venoxarid warden: the jailer. Body from art/models/VenoxaridWarden.glb.

Shape brief: a crooked jailer, 1.5x a hero. A riveted armoured coat to
mid-thigh, a ring of brass keys and padlocks hanging from the belt, a
glowing venom vial on one thigh. Broad and upright: it walks tall behind
the prisoners it drags. The chains are not modelled; code runs them from
its back to each prisoner's collar.

Clips (New Enemy Models brief, "Warden"): idle, run (walk while dragging:
leaning forward into the load, one fist closed on the chains at the
hip), attack (yank the chains: rears back and hauls both fists to the
chest; the first 45% is the telegraph), exposed (its prisoners are gone:
shoulders drop, it hunches, guard down; looped), hit, death.

Pose values are joint angles in degrees (pitch, roll, yaw), as in
_venox_thug.py.
"""

import math
import os

from forge import Clip, Key
import _venox_thug as thug
import skin

with_ = thug.with_

NAME = "VenoxaridWarden"
MODEL = "art/models/VenoxaridWarden.glb"
HEIGHT = 2.6                         # 1.5x a hero's bulk, a head taller
YAW = 0.0
STYLE = "ink"
RIM = (150, 232, 64)                 # Venoxarid acid green
CELL = (240, 216, 172)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, 0.0)


def off(x, y, z):
    return (x * K, y * K, z * K)


def stand(breath=0.0):
    """Upright and proprietary: chest out, chin up, the left fist closed
    at the hip where the chains are held, the right hand loose."""
    return {
        "hips": (0, 0, 0),
        "abdomen": (-2 + 1 * breath, 0, 0),
        "chest": (-4 + 2 * breath, 0, 0),
        "neck": (2, 0, 0),
        "head": (-4, 0, 0),
        "shoulder_L": (10, -12 - 2 * breath, 0), "elbow_L": (-50, 0, 0),
        "shoulder_R": (-6, 12 + 2 * breath, 0), "elbow_R": (-24, 0, 0),
        "hip_L": (-6, -6, -6), "knee_L": (8, 0, 0), "ankle_L": (-2, 4, 0),
        "hip_R": (4, 6, 6), "knee_R": (6, 0, 0), "ankle_R": (-8, -4, 0),
    }


def drag(phase):
    """A heavy walk leaning into the load. The left fist stays clamped at
    the hip, pulled back; only the right arm swings."""
    s = math.sin(phase * 2 * math.pi)
    return with_(stand(), **{
        "hips": (6, 0, 6 * s),
        "abdomen": (8, 0, -2 * s),
        "chest": (10, 0, -6 * s),
        "neck": (-6, 0, 2 * s),
        "head": (-6, 0, 0),
        "shoulder_L": (30, -14, 0), "elbow_L": (-60, 0, 0),
        "shoulder_R": (-6 + 26 * s, 14, 0), "elbow_R": (-34, 0, 0),
        "hip_L": (24 * s - 6, -6, 0), "knee_L": (10 + max(0.0, s) * 44, 0, 0),
        "ankle_L": (-6, 0, 0),
        "hip_R": (-24 * s - 6, 6, 0), "knee_R": (10 + max(0.0, -s) * 44, 0, 0),
        "ankle_R": (-6, 0, 0),
    })


def hunched(breath=0.0):
    """Exposed: the prisoners are gone. Shoulders drop, the back rounds,
    the head sinks, both hands hang open: no guard."""
    return with_(stand(breath), **{
        "hips": (-6, 0, 0),
        "abdomen": (14 + 2 * breath, 0, 0),
        "chest": (20 + 3 * breath, 0, 0),
        "neck": (10, 0, 0),
        "head": (10, 0, 0),
        "shoulder_L": (-4, -4, 0), "elbow_L": (-10, 0, 0),
        "shoulder_R": (-4, 4, 0), "elbow_R": (-10, 0, 0),
        "hip_L": (-14, -8, -6), "knee_L": (24, 0, 0),
        "hip_R": (-10, 8, 6), "knee_R": (22, 0, 0),
    })


def clips():
    s = stand()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        idle.append(Key(f / 8, stand(b), off(0, 0, -0.01 - 0.01 * b),
                        ease="linear"))

    run = []
    for f in range(9):
        p = f / 8
        run.append(Key(p, drag(p),
                       off(0, 0, -0.03 + 0.03 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    exposed = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        exposed.append(Key(f / 8, hunched(b), off(0, 0, -0.06 - 0.02 * b),
                           ease="linear"))

    # The yank. Telegraph: both fists reach out front and low, gripping the
    # chains, body leaning in. Then it hauls: rears back, fists to the
    # chest, elbows flaring: everything chained to it lurches.
    reach = with_(s, hips=(10, 0, 0), abdomen=(14, 0, 0), chest=(16, 0, 0),
                  neck=(-10, 0, 0), head=(-8, 0, 0),
                  shoulder_L=(-56, -10, 0), elbow_L=(-10, 0, 0),
                  shoulder_R=(-56, 10, 0), elbow_R=(-10, 0, 0),
                  hip_L=(-30, -8, 0), knee_L=(34, 0, 0),
                  hip_R=(10, 8, 0), knee_R=(16, 0, 0))
    haul = with_(s, hips=(-8, 0, 0), abdomen=(-14, 0, 0), chest=(-20, 0, 0),
                 neck=(10, 0, 0), head=(-12, 0, 0),
                 shoulder_L=(40, -34, 0), elbow_L=(-120, 0, 0),
                 shoulder_R=(40, 34, 0), elbow_R=(-120, 0, 0),
                 hip_L=(-4, -8, 0), knee_L=(20, 0, 0),
                 hip_R=(24, 8, 0), knee_R=(30, 0, 0))
    hurt = with_(s, abdomen=(-10, 0, 6), chest=(-16, 0, 10), neck=(-8, 0, 0),
                 head=(-18, 0, 12),
                 shoulder_L=(-20, -36, 0), elbow_L=(-40, 0, 0),
                 shoulder_R=(-20, 36, 0), elbow_R=(-40, 0, 0),
                 hip_L=(4, -8, -6), knee_L=(18, 0, 0))
    buckle = with_(hurt, abdomen=(-18, 0, 0), chest=(-26, 0, 0),
                   head=(-30, 0, 0),
                   hip_L=(-30, -12, 0), knee_L=(70, 0, 0),
                   hip_R=(-20, 12, 0), knee_R=(64, 0, 0))
    lying = with_(s, hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 0),
                  neck=(0, 0, 0), head=(-8, 0, 24),
                  shoulder_L=(-10, -38, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-6, 28, 0), elbow_R=(-64, 0, 0),
                  hip_L=(-8, -16, 0), knee_L=(10, 0, 0), ankle_L=(30, 0, 0),
                  hip_R=(-34, 20, 0), knee_R=(62, 0, 0), ankle_R=(30, 0, 0))

    return [
        Clip("idle", 8, 8, True, idle, overlap=0.03),
        Clip("run", 8, 10, True, run, overlap=0.015),
        Clip("attack", 10, 14, False, [
            Key(0.0, s, off(0, 0, -0.01)),
            Key(0.45, reach, off(0, -0.08, -0.08)),
            Key(0.6, haul, off(0, 0.16, -0.04), ease="out"),
            Key(0.8, haul, off(0, 0.18, -0.05)),
            Key(1.0, s, off(0, 0, -0.01))], overlap=0.015),
        Clip("exposed", 8, 8, True, exposed, overlap=0.03),
        Clip("hit", 4, 16, False, [
            Key(0.0, s, off(0, 0, -0.01)),
            Key(0.3, hurt, off(0, 0.06, -0.03), ease="out"),
            Key(1.0, s, off(0, 0, -0.01))]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, off(0, 0.05, -0.03)),
            Key(0.35, buckle, off(0, 0.10, -0.22)),
            Key(0.7, lying, off(0, -1.15, 0.20), rot=(-86, 0, 8), ease="in"),
            Key(1.0, lying, off(0, -1.15, 0.20), rot=(-90, 0, 8))]),
    ]
