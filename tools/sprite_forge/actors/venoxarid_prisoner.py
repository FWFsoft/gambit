"""Venoxarid prisoner: someone to save. Body from
art/models/VenoxaridPrisoner.glb (made on Meshy as "alien_withered_prisoner").

Shape brief: withered, hunched and pale next to every other enemy, 0.7x a
hero. Iron collar and wrist shackles, thin limbs, big tired eyes, a torn
smock. It must read "not an enemy": it never squares up, never raises a
fist, and its every pose folds inward. Code drains its colour further and
floods it back as it is healed.

Clips (New Enemy Models brief, "Prisoner"): idle (cowering, looped),
run (the shuffle while dragged: pulled forward by the collar, feet
dragging), freed (straightens up and runs: plays once, then the game
uses "flee"), flee (an upright run, looped), hit, death. There is no
attack: it has none.

Pose values are joint angles in degrees (pitch, roll, yaw), as in
_venox_thug.py.
"""

import math
import os

from forge import Clip, Key
import _venox_thug as thug
import skin

with_ = thug.with_

NAME = "VenoxaridPrisoner"
MODEL = "art/models/VenoxaridPrisoner.glb"
HEIGHT = 1.4                         # 0.7x a hero
YAW = 0.0
STYLE = "ink"
RIM = (150, 168, 120)                # a sick, drained green: no genus glow
CELL = (136, 124, 100)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, 0.0)


def off(x, y, z):
    return (x * K, y * K, z * K)


def cower(breath=0.0, shiver=0.0):
    """Folded in on itself: back rounded, head down between the shoulders,
    shackled wrists pulled up to the chest."""
    return {
        "hips": (-10, 0, 0),
        "abdomen": (16 + 2 * breath, 0, 0),
        "chest": (20 + 3 * breath, 0, shiver),
        "neck": (10, 0, 0),
        "head": (12, 0, -shiver),
        "shoulder_L": (-36, 4, 0), "elbow_L": (-110, 0, 0),
        "shoulder_R": (-36, -4, 0), "elbow_R": (-110, 0, 0),
        "hip_L": (-30, -6, -4), "knee_L": (46, 0, 0), "ankle_L": (-14, 4, 0),
        "hip_R": (-24, 6, 4), "knee_R": (40, 0, 0), "ankle_R": (-14, -4, 0),
    }


def shuffle(phase):
    """Dragged by the collar: the neck and head are pulled out ahead of
    the body, the arms trail, the feet barely leave the ground."""
    s = math.sin(phase * 2 * math.pi)
    return with_(cower(), **{
        "hips": (-4, 0, 4 * s),
        "abdomen": (8, 0, 0),
        "chest": (14, 0, -4 * s),
        "neck": (-24, 0, 0),
        "head": (-10, 0, 3 * s),
        "shoulder_L": (16, -6, 0), "elbow_L": (-20, 0, 0),
        "shoulder_R": (16, 6, 0), "elbow_R": (-20, 0, 0),
        "hip_L": (14 * s - 18, -6, 0), "knee_L": (30 + max(0.0, s) * 16, 0, 0),
        "ankle_L": (-10, 0, 0),
        "hip_R": (-14 * s - 18, 6, 0), "knee_R": (30 + max(0.0, -s) * 16, 0, 0),
        "ankle_R": (-10, 0, 0),
    })


def upright():
    """Freed: it stands up straight for the first time."""
    return {
        "hips": (0, 0, 0),
        "abdomen": (-4, 0, 0), "chest": (-10, 0, 0),
        "neck": (-6, 0, 0), "head": (-14, 0, 0),
        "shoulder_L": (10, -30, 0), "elbow_L": (-20, 0, 0),
        "shoulder_R": (10, 30, 0), "elbow_R": (-20, 0, 0),
        "hip_L": (-2, -4, 0), "knee_L": (4, 0, 0), "ankle_L": (0, 0, 0),
        "hip_R": (2, 4, 0), "knee_R": (4, 0, 0), "ankle_R": (0, 0, 0),
    }


def flee_pose(phase):
    s = math.sin(phase * 2 * math.pi)
    return with_(upright(), **{
        "hips": (0, 0, 8 * s),
        "abdomen": (6, 0, -3 * s), "chest": (8, 0, -8 * s),
        "neck": (-4, 0, 2 * s), "head": (-6, 0, 0),
        "shoulder_L": (-36 * s, -10, 0), "elbow_L": (-70, 0, 0),
        "shoulder_R": (36 * s, 10, 0), "elbow_R": (-70, 0, 0),
        "hip_L": (36 * s - 8, -4, 0), "knee_L": (14 + max(0.0, s) * 70, 0, 0),
        "ankle_L": (-8, 0, 0),
        "hip_R": (-36 * s - 8, 4, 0), "knee_R": (14 + max(0.0, -s) * 70, 0, 0),
        "ankle_R": (-8, 0, 0),
    })


def clips():
    c = cower()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        shiver = 3 * math.sin(4 * math.pi * f / 8)
        idle.append(Key(f / 8, cower(b, shiver), off(0, 0, -0.06 - 0.01 * b),
                        ease="linear"))

    run = []
    for f in range(9):
        p = f / 8
        run.append(Key(p, shuffle(p),
                       off(0, 0, -0.05 + 0.015 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    flee = []
    for f in range(9):
        p = f / 8
        flee.append(Key(p, flee_pose(p),
                        off(0, 0, 0.04 * abs(math.cos(p * 2 * math.pi))),
                        ease="linear"))

    # Freed: the shackles fall (code), it uncurls, rises onto its toes with
    # its arms opening, looks up, and bolts.
    rise = with_(upright(), chest=(-16, 0, 0), head=(-24, 0, 0),
                 shoulder_L=(-20, -50, 0), shoulder_R=(-20, 50, 0),
                 elbow_L=(-10, 0, 0), elbow_R=(-10, 0, 0),
                 ankle_L=(20, 0, 0), ankle_R=(20, 0, 0))

    flinch = with_(c, chest=(28, 0, 10), neck=(14, 0, 0), head=(18, 0, 14),
                   shoulder_L=(-60, 10, 0), elbow_L=(-130, 0, 0),
                   shoulder_R=(-60, -10, 0), elbow_R=(-130, 0, 0))
    curled = with_(c, hips=(0, 0, 0), abdomen=(30, 0, 0), chest=(30, 0, 0),
                   neck=(14, 0, 0), head=(20, 0, 0),
                   hip_L=(-90, -6, 0), knee_L=(120, 0, 0),
                   hip_R=(-84, 6, 0), knee_R=(116, 0, 0))

    return [
        Clip("idle", 8, 8, True, idle, overlap=0.02),
        Clip("run", 8, 10, True, run, overlap=0.02),
        Clip("freed", 8, 12, False, [
            Key(0.0, c, off(0, 0, -0.06)),
            Key(0.5, rise, off(0, 0, 0.04), ease="out"),
            Key(0.75, rise, off(0, 0, 0.05)),
            Key(1.0, flee_pose(0.0), off(0, -0.1, 0.0))]),
        Clip("flee", 8, 14, True, flee),
        Clip("hit", 4, 16, False, [
            Key(0.0, c, off(0, 0, -0.06)),
            Key(0.3, flinch, off(0, 0.06, -0.08), ease="out"),
            Key(1.0, c, off(0, 0, -0.06))]),
        # It sinks and curls up on its side rather than being thrown down:
        # a death that looks like a loss, not a kill.
        Clip("death", 9, 10, False, [
            Key(0.0, flinch, off(0, 0, -0.08)),
            Key(0.5, curled, off(0, 0, -0.40), ease="in"),
            Key(1.0, curled, off(0.55, 0, 0.12), rot=(0, -84, 0))]),
    ]
