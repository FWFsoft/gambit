"""Suds Sullivan, the Brew Brawler. Body from art/models/Suds.glb (Meshy),
on a human-placed fit; the cooler is art/models/SudsCooler.glb and the can
is still built in code. The clips were written for the earlier code-built
body and carry over: the joints keep their names.

A big, grinning bruiser: grey flat-top, orange shades, a blue Hawaiian
shirt open over a white tank, an orange strap across the chest, red
power-armour gauntlets and boots, a tool belt, a battered white cooler
slung at his hip and a brew can always in hand.

Shape brief: the barrel and the cooler. A wide barrel of a torso on armoured
legs; the cooler juts out at his left hip and the can is raised high in his
right hand, so his outline is lopsided from every side, unlike any other hero.
"""

import math
import os

from forge import Clip, Key, Model
import _hero as h
import skin

PIXEL_SCALE = 1.35  # chunkier art pixels, same size on screen (see forge.ActorSpec)
NAME = "Suds"
CELL = (176, 176, 150)
MODEL = "art/models/Suds.glb"
COOLER_MODEL = "art/models/SudsCooler.glb"
HEIGHT = 2.0
STYLE = "ink"
RIM = (150, 200, 255)                # his light blue
FIT = skin.load_fit(os.path.join(h.REPO, MODEL))

SKIN = (226, 146, 92)
HAIR = (196, 198, 204)
SHADES = (255, 112, 36)
SHIRT = (58, 148, 214)
FLOWER = (238, 234, 214)
PETAL = (242, 120, 58)
TANK = (232, 228, 216)
STRAP = (226, 112, 38)
ARMOR = (212, 62, 34)
ARMOR_DARK = (96, 64, 66)
BELT = (122, 92, 58)
POUCH = (176, 132, 70)
CAN = (210, 214, 222)
LABEL = (240, 204, 58)
COOLER = (234, 226, 206)
COOLER_TRIM = (170, 66, 48)


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT)
    # The can, in the right hand: it rises with the arm. Code-built: the
    # model's hands are empty.
    can = m.material("can", CAN)
    label = m.material("label", LABEL)
    w = m.rest_world("wrist_R")
    m.limb("wrist_R", can, (w[0], w[1] - 0.04, w[2] - 0.16),
           (w[0], w[1] - 0.04, w[2] - 0.54), 0.10, 0.10)
    m.limb("wrist_R", label, (w[0], w[1] - 0.04, w[2] - 0.30),
           (w[0], w[1] - 0.04, w[2] - 0.42), 0.104, 0.104, cap=False)
    # The cooler hangs at his left hip, sticking out past his body.
    hp = m.rest_world("hips")
    h.prop(m, COOLER_MODEL, "cooler", "hips",
           (hp[0] + 0.52, hp[1] - 0.04, hp[2] - 0.20), 0.70, turn=(0, 0, 90))
    return m, stance()


def stance(sway=0.0):
    """"Round's on me!": a toast with the can raised beside his grin, belly
    out, leaning back with his weight on one leg, left hand resting on the
    cooler. The life of the party, not a statue."""
    return {
        "hips": (0, 0, 6),
        "chest": (-12 + sway, 0, -4),
        "neck": (-8, 0, 8),
        "shoulder_R": (-58 - sway, 58, 0), "elbow_R": (0, 100 + sway * 2, 0),
        "shoulder_L": (-4, -12, 0), "elbow_L": (-20, 0, 0),
        "hip_L": (-2, -10, 0), "knee_L": (6, 0, 0), "ankle_L": (-4, 8, 0),
        "hip_R": (-12, 16, 6), "knee_R": (22, 0, 0), "ankle_R": (-10, -10, 0),
    }


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(phase):
    s = math.sin(phase * 2 * math.pi)
    p = stance()
    p.update({
        "hips": (0, 0, 6 * s),
        "chest": (10, 0, -6 * s),
        "shoulder_R": (-120 + 10 * s, 22, 0), "elbow_R": (-60, 0, 0),
        "shoulder_L": (-10 + 25 * s, -14, 0), "elbow_L": (-40, 0, 0),
        "hip_L": (30 * s - 6, -8, 0), "knee_L": (14 + max(0.0, s) * 55, 0, 0),
        "ankle_L": (-8, 0, 0),
        "hip_R": (-30 * s - 6, 8, 0), "knee_R": (14 + max(0.0, -s) * 55, 0, 0),
        "ankle_R": (-8, 0, 0),
    })
    return p


def throw_keys(start, end, wind):
    """One overhand lob of the can between t=start and t=end."""
    s = stance()
    back = with_(s, chest=(-14, 0, 20), shoulder_R=(-175, 30, 0),
                 elbow_R=(-110, 0, 0))
    release = with_(s, chest=(18, 0, -16), neck=(6, 0, 0),
                    shoulder_R=(-95, 10, 0), elbow_R=(-10, 0, 0),
                    hip_L=(-20, -14, 0), knee_L=(24, 0, 0))
    span = end - start
    return [Key(start + span * wind, back, (0, 0.03, -0.02), ease="out"),
            Key(start + span * 0.75, release, (0, -0.06, -0.04), ease="in"),
            Key(end, with_(release, shoulder_R=(-110, 14, 0),
                           elbow_R=(-30, 0, 0)), (0, -0.04, -0.03))]


def clips():
    s = stance()
    idle = [Key(f / 8, stance(3 * math.sin(f / 8 * 2 * math.pi)),
                (0, 0, -0.02 - 0.012 * math.sin(f / 8 * 2 * math.pi)))
            for f in range(9)]
    run = [Key(f / 8, run_pose(f / 8),
               (0, -0.02, -0.03 + 0.04 * abs(math.cos(f / 8 * 2 * math.pi))),
               ease="linear") for f in range(9)]

    # Frothy Cleanse: a belly-deep breath, then a belch that bends him forward.
    inhale = with_(s, chest=(-24, 0, 0), neck=(-20, 0, 0),
                   shoulder_L=(-30, -50, 0), elbow_L=(-40, 0, 0),
                   shoulder_R=(-40, 50, 0), elbow_R=(-40, 0, 0))
    belch = with_(s, chest=(30, 0, 0), neck=(26, 0, 0),
                  shoulder_L=(-20, -70, 0), elbow_L=(-20, 0, 0),
                  shoulder_R=(-30, 70, 0), elbow_R=(-20, 0, 0),
                  hip_L=(-24, -14, 0), knee_L=(30, 0, 0))
    # What's In This Stuff?: chug the can, head back, then a roaring flex.
    chug = with_(s, chest=(-14, 0, 0), neck=(-40, 0, 0),
                 shoulder_R=(-120, -10, 10), elbow_R=(-120, 0, 0))
    flex = with_(s, chest=(-10, 0, 0), neck=(-16, 0, 0),
                 shoulder_L=(-10, -88, 0), elbow_L=(0, -105, 0),
                 shoulder_R=(-10, 88, 0), elbow_R=(0, 105, 0))
    hurt = with_(s, chest=(-18, -6, 10), neck=(-20, 0, 0),
                 shoulder_L=(-20, -50, 0), elbow_L=(-30, 0, 0),
                 shoulder_R=(-60, 50, 0), elbow_R=(-50, 0, 0))
    kneel = with_(s, chest=(30, 0, 0), neck=(30, 0, 0),
                  shoulder_L=(-10, -20, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-10, 20, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-80, -6, 0), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 6, 0), knee_R=(120, 0, 0), ankle_R=(-40, 0, 0))
    lying = {"hips": (0, 0, 0), "chest": (-4, 0, 6), "neck": (-10, 0, 14),
             "shoulder_L": (-150, -60, 0), "elbow_L": (-20, 0, 0),
             "shoulder_R": (-40, 70, 0), "elbow_R": (-30, 0, 0),
             "hip_L": (-40, -14, 0), "knee_L": (60, 0, 0),
             "hip_R": (-20, 14, 0), "knee_R": (30, 0, 0)}

    rounds = [Key(0.0, s)]
    for k in range(3):
        rounds += throw_keys(k / 3, (k + 1) / 3, 0.35)
    rounds.append(Key(1.0, s))

    return [
        Clip("idle", 8, 8, True, idle),
        Clip("run", 8, 12, True, run),
        Clip("toss", 6, 18, False,
             [Key(0.0, s)] + throw_keys(0.0, 0.8, 0.3) + [Key(1.0, s)]),
        Clip("belch", 8, 14, False, [
            Key(0.0, s), Key(0.3, inhale, (0, 0.04, 0.02), ease="out"),
            Key(0.5, belch, (0, -0.08, -0.06), ease="in"),
            Key(0.8, belch, (0, -0.08, -0.06)), Key(1.0, s)]),
        Clip("rounds", 12, 18, False, rounds),
        Clip("chug", 10, 12, False, [
            Key(0.0, s), Key(0.25, chug, (0, 0.02, 0)),
            Key(0.5, chug, (0, 0.02, 0.01)),
            Key(0.7, flex, (0, 0, 0.04), ease="out"),
            Key(0.9, with_(flex, chest=(-16, 0, 0)), (0, 0, 0.05)),
            Key(1.0, s)]),
        Clip("hit", 4, 16, False, [
            Key(0.0, s), Key(0.3, hurt, (0, 0.06, -0.02), ease="out"),
            Key(1.0, s)]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, (0, 0.04, -0.02)),
            Key(0.35, kneel, (0, 0.06, -0.44)),
            Key(0.7, lying, (0, 0.10, 0.18), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, 0.10, 0.18), rot=(-90, 0, 0))]),
    ]
