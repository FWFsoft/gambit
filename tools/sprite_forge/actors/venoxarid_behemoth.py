"""Venoxarid behemoth: the ship-breaker. Body from
art/models/VenoxaridBehemoth.glb.

Shape brief: the biggest thing in a fight, about twice a hero's height
and three times as wide. A gorilla-built brute in spiked, riveted harness,
arms like pillars that nearly reach the ground, a crest of acid-yellow
spines, head carried low and forward between the shoulders like a ram so
you can see which way it is going. Wide and low rather than tall, so it
does not hide the crew.

The model is upright where the brief asked for a knuckle-walker; the
human kept it (2026-11-27). The long arms still carry the knuckle-walk
read: in the lumber the fists swing down near the ground.

Clips (New Enemy Models brief, "Behemoth"): idle, run (lumber forward),
attack (overhead two-fist smash: the first 45% is the telegraph, both
fists high), blocked (digging in and pushing against the blocker,
looped), braced (stopped, steaming, open to damage; looped), hit, death.

Pose values are joint angles in degrees (pitch, roll, yaw), as in
_venox_thug.py.
"""

import math
import os

from forge import Clip, Key
import _venox_thug as thug
import skin

with_ = thug.with_

NAME = "VenoxaridBehemoth"
MODEL = "art/models/VenoxaridBehemoth.glb"
HEIGHT = 4.2                         # ~2.15x a hero's height, ~3x the width
YAW = 0.0
STYLE = "ink"
RIM = (150, 232, 64)                 # Venoxarid acid green
CELL = (560, 520, 360)               # deep below the feet: it falls toward you
PIXEL_SCALE = 2.0                    # chunky pixels: the same art pixels per body as a hero
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, 0.0)


def off(x, y, z):
    return (x * K, y * K, z * K)


def stand(breath=0.0):
    """Head low and forward, back humped, arms hanging in front of the
    knees with the fists half-closed. Breathing moves the whole mass."""
    return {
        "hips": (-8, 0, 0),
        "abdomen": (10 + 3 * breath, 0, 0),
        "chest": (12 + 4 * breath, 0, 0),
        "neck": (-10, 0, 0),
        "head": (-6, 0, 0),
        "shoulder_L": (-16, -10 - 3 * breath, 0), "elbow_L": (-20, 0, 0),
        "shoulder_R": (-16, 10 + 3 * breath, 0), "elbow_R": (-20, 0, 0),
        "hip_L": (-16, -10, -8), "knee_L": (24, 0, 0), "ankle_L": (-8, 6, 0),
        "hip_R": (-12, 10, 8), "knee_R": (22, 0, 0), "ankle_R": (-10, -6, 0),
    }


def lumber(phase):
    """Slow and heavy: the whole body rolls side to side over each planted
    foot, the arms swing low like a knuckle-walker's."""
    s = math.sin(phase * 2 * math.pi)
    return with_(stand(), **{
        "hips": (-10, 6 * s, 8 * s),
        "abdomen": (16, -3 * s, -2 * s),
        "chest": (18, -4 * s, -8 * s),
        "neck": (-16, 0, 4 * s),
        "head": (-8, 0, 0),
        "shoulder_L": (-30 - 26 * s, -12, 0), "elbow_L": (-26, 0, 0),
        "shoulder_R": (-30 + 26 * s, 12, 0), "elbow_R": (-26, 0, 0),
        "hip_L": (22 * s - 16, -8, 0), "knee_L": (22 + max(0.0, s) * 36, 0, 0),
        "ankle_L": (-8, 0, 0),
        "hip_R": (-22 * s - 16, 8, 0), "knee_R": (22 + max(0.0, -s) * 36, 0, 0),
        "ankle_R": (-8, 0, 0),
    })


def clips():
    s = stand()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        idle.append(Key(f / 8, stand(b), off(0, 0, -0.05 - 0.02 * b),
                        ease="linear"))

    run = []
    for f in range(9):
        p = f / 8
        run.append(Key(p, lumber(p),
                       off(0, 0, -0.08 + 0.03 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    # The smash. Telegraph: rears up, both fists locked together high over
    # the head, chest open. Then everything comes down at once.
    rear = with_(s, hips=(4, 0, 0), abdomen=(-10, 0, 0), chest=(-18, 0, 0),
                 neck=(6, 0, 0), head=(-14, 0, 0),
                 shoulder_L=(-170, 14, 0), elbow_L=(-40, 0, 0),
                 shoulder_R=(-170, -14, 0), elbow_R=(-40, 0, 0),
                 hip_L=(-10, -10, 0), knee_L=(14, 0, 0),
                 hip_R=(-6, 10, 0), knee_R=(12, 0, 0))
    smash = with_(s, hips=(-24, 0, 0), abdomen=(28, 0, 0), chest=(34, 0, 0),
                  neck=(-24, 0, 0), head=(-10, 0, 0),
                  shoulder_L=(-70, 10, 0), elbow_L=(-6, 0, 0),
                  shoulder_R=(-70, -10, 0), elbow_R=(-6, 0, 0),
                  hip_L=(-50, -12, 0), knee_L=(66, 0, 0),
                  hip_R=(-40, 12, 0), knee_R=(60, 0, 0))

    # Blocked: shoulder down into whatever stands in front, legs driving.
    push_a = with_(s, hips=(-30, 0, 0), abdomen=(24, 0, 0), chest=(26, 0, 0),
                   neck=(-30, 0, 0), head=(-18, 0, 0),
                   shoulder_L=(-80, -20, 0), elbow_L=(-60, 0, 0),
                   shoulder_R=(-80, 20, 0), elbow_R=(-60, 0, 0),
                   hip_L=(-56, -10, 0), knee_L=(60, 0, 0), ankle_L=(-10, 0, 0),
                   hip_R=(10, 10, 0), knee_R=(14, 0, 0), ankle_R=(20, 0, 0))
    push_b = with_(push_a, hips=(-30, 0, 0),
                   hip_L=(10, -10, 0), knee_L=(14, 0, 0), ankle_L=(20, 0, 0),
                   hip_R=(-56, 10, 0), knee_R=(60, 0, 0), ankle_R=(-10, 0, 0),
                   shoulder_L=(-84, -20, 0), shoulder_R=(-76, 20, 0))

    # Braced: stopped dead, winded, fists on the ground, head hanging.
    # Code adds the steam and the damage highlight.
    def slump(b):
        return with_(s, hips=(-14, 0, 0), abdomen=(20 + 4 * b, 0, 0),
                     chest=(24 + 5 * b, 0, 0), neck=(6, 0, 0), head=(14, 0, 0),
                     shoulder_L=(-34, -16, 0), elbow_L=(-4, 0, 0),
                     shoulder_R=(-34, 16, 0), elbow_R=(-4, 0, 0),
                     hip_L=(-30, -12, 0), knee_L=(40, 0, 0),
                     hip_R=(-26, 12, 0), knee_R=(36, 0, 0))
    braced = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        braced.append(Key(f / 8, slump(b), off(0, 0, -0.14 - 0.03 * b),
                          ease="linear"))

    hurt = with_(s, abdomen=(2, 0, 6), chest=(0, 0, 10), neck=(-4, 0, 0),
                 head=(-20, 0, 10),
                 shoulder_L=(-10, -24, 0), elbow_L=(-30, 0, 0),
                 shoulder_R=(-10, 24, 0), elbow_R=(-30, 0, 0))
    kneel = with_(slump(1.0), hip_L=(-80, -12, 0), knee_L=(110, 0, 0),
                  hip_R=(-70, 12, 0), knee_R=(104, 0, 0))
    lying = with_(s, hips=(0, 0, 0), abdomen=(0, 0, 0), chest=(0, 0, 0),
                  neck=(-10, 0, 0), head=(-6, 0, 20),
                  shoulder_L=(-150, -30, 0), elbow_L=(-10, 0, 0),
                  shoulder_R=(-150, 30, 0), elbow_R=(-10, 0, 0),
                  hip_L=(0, -14, 0), knee_L=(10, 0, 0),
                  hip_R=(0, 14, 0), knee_R=(10, 0, 0))

    return [
        Clip("idle", 8, 6, True, idle, overlap=0.04),
        Clip("run", 8, 8, True, run, overlap=0.03),
        Clip("attack", 12, 12, False, [
            Key(0.0, s, off(0, 0, -0.05)),
            Key(0.45, rear, off(0, 0.10, 0.04), ease="out"),
            Key(0.6, smash, off(0, -0.18, -0.18), ease="in"),
            Key(0.8, smash, off(0, -0.18, -0.19)),
            Key(1.0, s, off(0, 0, -0.05))], overlap=0.02),
        Clip("blocked", 8, 8, True, [
            Key(0.0, push_a, off(0, -0.06, -0.12)),
            Key(0.5, push_b, off(0, -0.06, -0.12)),
            Key(1.0, push_a, off(0, -0.06, -0.12))], overlap=0.02),
        Clip("braced", 8, 6, True, braced, overlap=0.03),
        Clip("hit", 4, 12, False, [
            Key(0.0, s, off(0, 0, -0.05)),
            Key(0.3, hurt, off(0, 0.03, -0.06), ease="out"),
            Key(1.0, s, off(0, 0, -0.05))]),
        # Too heavy to be thrown: it drops to its knees, then topples
        # forward onto its face, arms out.
        Clip("death", 12, 10, False, [
            Key(0.0, hurt, off(0, 0, -0.06)),
            Key(0.35, kneel, off(0, 0, -0.40), ease="in"),
            Key(0.55, kneel, off(0, 0, -0.42)),
            # The root turns about the feet: slide it back so the body
            # lands inside the cell.
            Key(0.85, lying, off(0, 1.2, 0.10), rot=(84, 0, 0), ease="in"),
            Key(1.0, lying, off(0, 1.2, 0.10), rot=(88, 0, 0))]),
    ]
