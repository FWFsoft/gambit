"""Venoxarid diver: the pouncer. Body from art/models/VenoxaridDiver.glb
(made on Meshy as "alien_assassin").

Shape brief: a skinny, long-limbed gang runner, 0.9x a hero but stretched.
Hood up with the green crest poking through, wrapped forearms, long clawed
fingers and toes. Lean and crouched where the grunt is upright and
slouched. It is seen for about a second, mostly in the air, so the leap
(attack at 0.55-0.75) is the most recognisable shape in the roster: limbs
spread wide, claws forward.

Clips (New Enemy Models brief, "Diver"): idle, run (the low stalking
creep), attack (crouched wind-up, held by the client for the 1 s
telegraph, then the spread-eagle leap and landing), maul (on top of a
target, looped), caught (pinned, then bursts free), hit, death.

Pose values are joint angles in degrees (pitch, roll, yaw), as in
_venox_thug.py: negative shoulder or hip pitch swings the limb forward, a
negative elbow bends it, a positive knee bends it.
"""

import math
import os

from forge import Clip, Key
import _venox_thug as thug
import skin

with_ = thug.with_

NAME = "VenoxaridDiver"
MODEL = "art/models/VenoxaridDiver.glb"
HEIGHT = 1.85                        # 0.9x a hero, all of it limbs
YAW = 0.0
STYLE = "ink"
RIM = (150, 232, 64)                 # Venoxarid acid green
CELL = (264, 184, 140)               # wide: room for the spread leap
K = HEIGHT / 1.95                    # root offsets, relative to the grunt

FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, 0.0)


def off(x, y, z):
    return (x * K, y * K, z * K)


def stand(breath=0.0):
    """The crouch: knees deep, back low and long, head pushed forward
    under the hood, claws hanging ahead of the knees."""
    return {
        "hips": (-18, 0, 0),
        "abdomen": (16 + 2 * breath, 0, 0),
        "chest": (14 + 3 * breath, 0, 0),
        "neck": (-14, 0, 0),
        "head": (-12, 0, 0),
        "shoulder_L": (-30, -16 - 2 * breath, 0), "elbow_L": (-30, 0, 0),
        "shoulder_R": (-26, 18 + 2 * breath, 0), "elbow_R": (-36, 0, 0),
        "hip_L": (-44, -14, -6), "knee_L": (70, 0, 0), "ankle_L": (-20, 6, 0),
        "hip_R": (-30, 14, 6), "knee_R": (66, 0, 0), "ankle_R": (-30, -6, 0),
    }


def creep(phase):
    """Low and quick, shoulders rolling, claws trailing near the ground."""
    s = math.sin(phase * 2 * math.pi)
    c = math.cos(phase * 2 * math.pi)
    return with_(stand(), **{
        "hips": (-24, 0, 8 * s),
        "abdomen": (20, 0, -4 * s),
        "chest": (18, 0, -12 * s),
        "neck": (-18, 0, 4 * s),
        "head": (-14, 0, 2 * s),
        "shoulder_L": (-30 - 40 * s, -20, 0), "elbow_L": (-44, 0, 0),
        "shoulder_R": (-30 + 40 * s, 20, 0), "elbow_R": (-44, 0, 0),
        "hip_L": (-46 + 36 * s, -12, 0),
        "knee_L": (66 + max(0.0, c) * 40, 0, 0), "ankle_L": (-18, 0, 0),
        "hip_R": (-46 - 36 * s, 12, 0),
        "knee_R": (66 + max(0.0, -c) * 40, 0, 0), "ankle_R": (-18, 0, 0),
    })


def clips():
    s = stand()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        # A twitchy idle: the head ticks side to side as it watches.
        head_yaw = 14 * math.sin(2 * math.pi * f / 8)
        pose = with_(stand(b), head=(-12, 0, head_yaw))
        idle.append(Key(f / 8, pose, off(0, 0, -0.16 - 0.015 * b),
                        ease="linear"))

    run = []
    for f in range(9):
        p = f / 8
        run.append(Key(p, creep(p),
                       off(0, 0, -0.2 + 0.05 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    # Wind-up: coiled tight, chest to the knees, claws drawn back behind
    # the hips. Held for the telegraph: it must read as "about to spring".
    coil = with_(s, hips=(-34, 0, 0), abdomen=(26, 0, 0), chest=(24, 0, 0),
                 neck=(-30, 0, 0), head=(-20, 0, 0),
                 shoulder_L=(40, -30, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(40, 30, 0), elbow_R=(-20, 0, 0),
                 hip_L=(-80, -16, 0), knee_L=(110, 0, 0), ankle_L=(-30, 0, 0),
                 hip_R=(-70, 16, 0), knee_R=(104, 0, 0), ankle_R=(-30, 0, 0))
    # The leap: spread-eagle, every limb out, claws forward and wide. The
    # one silhouette nothing else in the roster makes.
    leap = with_(s, hips=(10, 0, 0), abdomen=(-8, 0, 0), chest=(-10, 0, 0),
                 neck=(-6, 0, 0), head=(-4, 0, 0),
                 shoulder_L=(-100, -60, 0), elbow_L=(-14, 0, 0),
                 shoulder_R=(-100, 60, 0), elbow_R=(-14, 0, 0),
                 hip_L=(26, -36, 0), knee_L=(40, 0, 0), ankle_L=(40, 0, 0),
                 hip_R=(26, 36, 0), knee_R=(40, 0, 0), ankle_R=(40, 0, 0))
    # Landing: claws driven down into the target, legs folded under.
    land = with_(s, hips=(-30, 0, 0), abdomen=(30, 0, 0), chest=(26, 0, 0),
                 neck=(-24, 0, 0), head=(-10, 0, 0),
                 shoulder_L=(-70, -26, 0), elbow_L=(-30, 0, 0),
                 shoulder_R=(-70, 26, 0), elbow_R=(-30, 0, 0),
                 hip_L=(-90, -20, 0), knee_L=(120, 0, 0), ankle_L=(-20, 0, 0),
                 hip_R=(-84, 20, 0), knee_R=(116, 0, 0), ankle_R=(-20, 0, 0))

    # Mauling, on top of its target: alternate claw rakes, looped.
    rake_l = with_(land, chest=(30, 0, 10), shoulder_L=(-110, -20, 0),
                   elbow_L=(-60, 0, 0), shoulder_R=(-40, 30, 0),
                   elbow_R=(-20, 0, 0))
    rake_r = with_(land, chest=(30, 0, -10), shoulder_R=(-110, 20, 0),
                   elbow_R=(-60, 0, 0), shoulder_L=(-40, -30, 0),
                   elbow_L=(-20, 0, 0))

    # Caught: grabbed and pinned, limbs drawn in and twisting; then it
    # bursts out, every limb flung wide (code adds the CAUGHT burst).
    pinned = with_(s, hips=(-10, 0, 0), abdomen=(-6, 0, 14),
                   chest=(-12, 0, 20), neck=(-6, 0, -14), head=(-18, 0, -20),
                   shoulder_L=(-40, -4, 0), elbow_L=(-120, 0, 0),
                   shoulder_R=(-50, 4, 0), elbow_R=(-120, 0, 0),
                   hip_L=(-60, -6, 0), knee_L=(100, 0, 0),
                   hip_R=(-40, 6, 0), knee_R=(96, 0, 0))
    squirm = with_(pinned, abdomen=(-6, 0, -14), chest=(-12, 0, -20),
                   neck=(-6, 0, 14), head=(-18, 0, 20))
    burst = with_(leap, hips=(0, 0, 0), chest=(-20, 0, 0), head=(-24, 0, 0),
                  shoulder_L=(-30, -90, 0), shoulder_R=(-30, 90, 0),
                  hip_L=(0, -40, 0), hip_R=(0, 40, 0),
                  knee_L=(20, 0, 0), knee_R=(20, 0, 0))

    hurt = with_(s, abdomen=(4, 0, 8), chest=(-6, 0, 12), neck=(-6, 0, 0),
                 head=(-24, 0, 16),
                 shoulder_L=(-10, -50, 0), elbow_L=(-60, 0, 0),
                 shoulder_R=(-6, 52, 0), elbow_R=(-60, 0, 0))
    lying = with_(s, hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 0),
                  neck=(0, 0, 0), head=(-8, 0, 24),
                  shoulder_L=(-20, -60, 0), elbow_L=(-50, 0, 0),
                  shoulder_R=(-10, 50, 0), elbow_R=(-70, 0, 0),
                  hip_L=(-20, -20, 0), knee_L=(40, 0, 0), ankle_L=(30, 0, 0),
                  hip_R=(-50, 24, 0), knee_R=(90, 0, 0), ankle_R=(30, 0, 0))

    return [
        Clip("idle", 8, 10, True, idle, overlap=0.03),
        Clip("run", 8, 14, True, run, overlap=0.01),
        Clip("attack", 12, 14, False, [
            Key(0.0, s, off(0, 0, -0.16)),
            Key(0.45, coil, off(0, 0.12, -0.42), ease="out"),
            Key(0.55, leap, off(0, -0.5, 0.35), ease="out"),
            Key(0.75, leap, off(0, -1.1, 0.30), ease="linear"),
            Key(0.88, land, off(0, -1.3, -0.30), ease="in"),
            Key(1.0, land, off(0, -1.3, -0.26))], overlap=0.01),
        Clip("maul", 6, 14, True, [
            Key(0.0, rake_l, off(0, 0, -0.3)),
            Key(0.5, rake_r, off(0, 0, -0.3)),
            Key(1.0, rake_l, off(0, 0, -0.3))], overlap=0.012),
        Clip("caught", 10, 12, False, [
            Key(0.0, pinned, off(0, 0, -0.05)),
            Key(0.2, squirm, off(0.03, 0, -0.05)),
            Key(0.4, pinned, off(-0.03, 0, -0.05)),
            Key(0.6, squirm, off(0.03, 0, -0.05)),
            Key(0.75, coil, off(0, 0.05, -0.40), ease="out"),
            Key(0.88, burst, off(0, 0, 0.25), ease="out"),
            Key(1.0, s, off(0, 0, -0.16), ease="in")]),
        Clip("hit", 4, 16, False, [
            Key(0.0, s, off(0, 0, -0.16)),
            Key(0.3, hurt, off(0, 0.12, -0.12), ease="out"),
            Key(1.0, s, off(0, 0, -0.16))]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, off(0, 0.05, -0.12)),
            Key(0.35, pinned, off(0, 0.10, -0.30)),
            Key(0.7, lying, off(0, -1.0, 0.16), rot=(-86, 0, 8), ease="in"),
            Key(1.0, lying, off(0, -1.0, 0.16), rot=(-90, 0, 8))]),
    ]
