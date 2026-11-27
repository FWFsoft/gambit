"""Venoxarid channeler: the pack's priest. Body from
art/models/VenoxaridChanneler.glb.

Shape brief: a robed caster, 0.9x a hero's bulk, standing tall behind the
hunched pack. A spiked iron halo rises behind its head and two glowing
venom canisters ride on its back: the halo is the signature piece and
tops its outline from every side. Riveted pauldrons and bracers, a long
banner hanging from the belt with the gang's glyph.

The brief asked for a hype man with a cap and a megaphone; the human kept
this robed priest instead (2026-11-27). It still says "a caster: stop it
before it finishes". It carries no prop: the channel is in the hands.

Clips (New Enemy Models brief, "Channeler"): idle, run (walk), plant
(stops, sets its feet, hands rise: plays once into "channel"), channel
(the 3 s loop: arms raised to the halo, whole body shaking), interrupted
(stagger), rally (rally done: a shout, arms flung wide), hit, death.

Pose values are joint angles in degrees (pitch, roll, yaw), as in
_venox_thug.py.
"""

import math
import os

from forge import Clip, Key
import _venox_thug as thug
import skin

with_ = thug.with_

NAME = "VenoxaridChanneler"
MODEL = "art/models/VenoxaridChanneler.glb"
HEIGHT = 1.85                        # 0.9x a hero; the halo adds the height
YAW = 0.0
STYLE = "ink"
RIM = (190, 255, 60)                 # the canisters' venom glow
CELL = (184, 168, 132)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(thug.REPO, MODEL))


def build():
    return thug.build(NAME, MODEL, HEIGHT, FIT, YAW, 0.0)


def off(x, y, z):
    return (x * K, y * K, z * K)


def stand(breath=0.0):
    """The preacher: tall and still, chin raised, both arms held down and
    out in a wide A, claws spread, palms toward you. Always
    half-way into a cast; the open arms keep its outline from reading as
    an upright thug (silhouette.py flagged the plain stand against the
    grunt)."""
    return {
        "hips": (0, 0, 0),
        "abdomen": (-2 + 1 * breath, 0, 0),
        "chest": (-4 + 2 * breath, 0, 0),
        "neck": (0, 0, 0),
        "head": (-6, 0, 0),
        "shoulder_L": (-8, -48 - 3 * breath, 0), "elbow_L": (-14, 0, 0),
        "shoulder_R": (-8, 48 + 3 * breath, 0), "elbow_R": (-14, 0, 0),
        "hip_L": (-2, -4, -4), "knee_L": (4, 0, 0), "ankle_L": (0, 4, 0),
        "hip_R": (2, 4, 4), "knee_R": (4, 0, 0), "ankle_R": (-4, -4, 0),
    }


def walk(phase):
    """A measured processional walk: short steps, upright, hands held."""
    s = math.sin(phase * 2 * math.pi)
    return with_(stand(), **{
        "hips": (0, 0, 5 * s),
        "abdomen": (2, 0, -2 * s),
        "chest": (2, 0, -5 * s),
        "head": (-6, 0, 2 * s),
        "shoulder_L": (-10 - 14 * s, -20, 0), "elbow_L": (-44, 0, 0),
        "shoulder_R": (-10 + 14 * s, 20, 0), "elbow_R": (-44, 0, 0),
        "hip_L": (24 * s - 4, -4, 0), "knee_L": (6 + max(0.0, s) * 44, 0, 0),
        "ankle_L": (-4, 0, 0),
        "hip_R": (-24 * s - 4, 4, 0), "knee_R": (6 + max(0.0, -s) * 44, 0, 0),
        "ankle_R": (-4, 0, 0),
    })


def raised(shake=0.0):
    """Channelling: arms raised in a wide V either side of the halo, head back, feet wide.
    `shake` jitters the whole body (the brief: its whole body shakes)."""
    return {
        "hips": (0, shake * 0.5, 0),
        "abdomen": (-8, shake, 0),
        "chest": (-14, -shake, shake),
        "neck": (6, 0, 0),
        "head": (-20, shake, -shake),
        "shoulder_L": (-110 + 2 * shake, -58, 0), "elbow_L": (-20, 0, 0),
        "shoulder_R": (-110 - 2 * shake, 58, 0), "elbow_R": (-20, 0, 0),
        "hip_L": (-4, -14, 0), "knee_L": (10, 0, 0), "ankle_L": (0, 10, 0),
        "hip_R": (-4, 14, 0), "knee_R": (10, 0, 0), "ankle_R": (0, -10, 0),
    }


def clips():
    s = stand()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        idle.append(Key(f / 8, stand(b), off(0, 0, -0.01 - 0.008 * b),
                        ease="linear"))

    run = []
    for f in range(9):
        p = f / 8
        run.append(Key(p, walk(p),
                       off(0, 0, -0.02 + 0.02 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    planted = with_(s, hip_L=(-4, -14, 0), hip_R=(-4, 14, 0),
                    ankle_L=(0, 10, 0), ankle_R=(0, -10, 0),
                    knee_L=(14, 0, 0), knee_R=(14, 0, 0),
                    shoulder_L=(-70, -24, 0), elbow_L=(-60, 0, 0),
                    shoulder_R=(-70, 24, 0), elbow_R=(-60, 0, 0),
                    chest=(-8, 0, 0), head=(-12, 0, 0))

    channel = []
    for f in range(13):
        shake = 4 * math.sin(f * 2.4) * (1 if f % 2 else -1)
        channel.append(Key(f / 12, raised(shake), off(0, 0, 0.0),
                           ease="linear"))

    # Interrupted: snapped out of it, arms drop and it reels back.
    reel = with_(s, abdomen=(-14, 0, 10), chest=(-22, 0, 16), neck=(-10, 0, 0),
                 head=(-24, 0, 18),
                 shoulder_L=(-30, -60, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(20, 40, 0), elbow_R=(-70, 0, 0),
                 hip_L=(10, -8, 0), knee_L=(30, 0, 0),
                 hip_R=(-24, 8, 0), knee_R=(40, 0, 0))
    # Rally: the cast lands. Lean in, arms flung wide, a shout.
    shout = with_(s, hips=(-6, 0, 0), abdomen=(10, 0, 0), chest=(16, 0, 0),
                  neck=(-14, 0, 0), head=(-8, 0, 0),
                  shoulder_L=(-40, -90, 0), elbow_L=(-6, 0, 0),
                  shoulder_R=(-40, 90, 0), elbow_R=(-6, 0, 0),
                  hip_L=(-24, -12, 0), knee_L=(30, 0, 0),
                  hip_R=(10, 12, 0), knee_R=(10, 0, 0))

    hurt = with_(s, abdomen=(-10, 0, 6), chest=(-16, 0, 10), neck=(-8, 0, 0),
                 head=(-20, 0, 12),
                 shoulder_L=(-20, -40, 0), elbow_L=(-50, 0, 0),
                 shoulder_R=(-20, 40, 0), elbow_R=(-50, 0, 0))
    buckle = with_(hurt, abdomen=(-18, 0, 0), chest=(-26, 0, 0),
                   head=(-30, 0, 0),
                   hip_L=(-30, -12, 0), knee_L=(70, 0, 0),
                   hip_R=(-20, 12, 0), knee_R=(64, 0, 0))
    lying = with_(s, hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 0),
                  neck=(0, 0, 0), head=(-8, 0, 24),
                  shoulder_L=(-10, -50, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-6, 40, 0), elbow_R=(-50, 0, 0),
                  hip_L=(-8, -16, 0), knee_L=(10, 0, 0), ankle_L=(30, 0, 0),
                  hip_R=(-34, 20, 0), knee_R=(62, 0, 0), ankle_R=(30, 0, 0))

    return [
        Clip("idle", 8, 8, True, idle, overlap=0.03),
        Clip("run", 8, 10, True, run, overlap=0.015),
        Clip("plant", 6, 12, False, [
            Key(0.0, s, off(0, 0, -0.01)),
            Key(1.0, planted, off(0, 0, -0.03))], overlap=0.02),
        Clip("channel", 12, 12, True, channel),
        Clip("interrupted", 8, 14, False, [
            Key(0.0, raised(), off(0, 0, 0)),
            Key(0.3, reel, off(0, 0.16, -0.04), ease="out"),
            Key(0.6, reel, off(0, 0.18, -0.05)),
            Key(1.0, s, off(0, 0.12, -0.01))]),
        Clip("rally", 8, 12, False, [
            Key(0.0, raised(), off(0, 0, 0)),
            Key(0.3, shout, off(0, -0.06, -0.03), ease="out"),
            Key(0.75, shout, off(0, -0.06, -0.03)),
            Key(1.0, s, off(0, 0, -0.01))], overlap=0.015),
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
