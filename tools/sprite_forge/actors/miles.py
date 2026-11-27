"""MILES, the Overmind (Winger). Body from art/models/MILES.glb; his
keyboard is art/models/MILESKeyboard.glb.

Shape brief: a hooded hacker in an oversized cream parka, gas-mask
respirator, cargo pants and chunky orange-soled sneakers. The keyboard is
the signature piece: he holds it in front of his belly like a tray and is
always typing, so his outline has a flat slab at the waist that nobody
else has. Shoulders hunched, head down over the keys.

Kit (Character Concepts doc):
  attack     White Hat/Black Hat: he dives into a target (code takes him
             off the field): a forward plunge, arms out, into the screen.
  secondary  Exploit (hold): typing furiously, keyboard up at the chest.
  special    Man-in-the-Middle: arms flung wide, keyboard raised in one
             hand, the other pointing.
  mobility   XSS: slam the enter key and drop into the network: a crouch
             that sinks.
"""

import math
import os

from forge import Model
import _hero as h
from _hero import with_
import skin

NAME = "MILES"
MODEL = "art/models/MILES.glb"
KEYBOARD = "art/models/MILESKeyboard.glb"
HEIGHT = 1.85
YAW = 0.0
STYLE = "ink"
RIM = (255, 220, 0)                  # his bright yellow
CELL = (184, 168, 132)
K = HEIGHT / 1.95

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    a = m.rest_world("abdomen")
    # In front of his belly, the near edge tipped up toward him.
    h.prop(m, KEYBOARD, "kb", "abdomen", (a[0], a[1] - 0.30, a[2] - 0.06),
           0.55, turn=(-25, 0, 0))
    return m, stand()


def typing(breath=0.0, tap=0.0):
    """Hands on the keys: forearms forward, elbows in. `tap` (-1..1)
    alternates the hands."""
    return {
        "shoulder_L": (-34 - 4 * tap, -10, 0), "elbow_L": (-62 + 6 * tap, 0, 0),
        "shoulder_R": (-34 + 4 * tap, 10, 0), "elbow_R": (-62 - 6 * tap, 0, 0),
    }


def stand(breath=0.0, tap=0.0):
    """Hunched over the keyboard, head down, knees soft."""
    p = {
        "kb": (0, 0, 0),
        "hips": (-2, 0, 0),
        "abdomen": (6 + 1 * breath, 0, 0),
        "chest": (10 + 2 * breath, 0, 0),
        "neck": (6, 0, 0),
        "head": (14, 0, 0),
        "hip_L": (-4, -4, -4), "knee_L": (8, 0, 0), "ankle_L": (-2, 2, 0),
        "hip_R": (-4, 4, 4), "knee_R": (8, 0, 0), "ankle_R": (-2, -2, 0),
    }
    p.update(typing(breath, tap))
    return p


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    from forge import Clip, Key
    s = stand()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        idle.append(Key(f / 8, stand(b, math.sin(4 * math.pi * f / 8)),
                        o(0, 0, -0.01 * b), ease="linear"))

    def run(phase):
        s_ = math.sin(phase * 2 * math.pi)
        legs = h.run_pose(stand, lean=12, arm=0, leg=30, knee=60)(phase)
        # The keyboard stays clamped at his belly while he runs.
        return with_(legs, **typing(), chest=(14, 0, -6 * s_), head=(10, 0, 0))

    dive_in = with_(s, hips=(-10, 0, 0), abdomen=(16, 0, 0), chest=(20, 0, 0),
                    head=(4, 0, 0),
                    shoulder_L=(10, -20, 0), elbow_L=(-30, 0, 0),
                    shoulder_R=(10, 20, 0), elbow_R=(-30, 0, 0),
                    hip_L=(-40, -6, 0), knee_L=(70, 0, 0),
                    hip_R=(-30, 6, 0), knee_R=(60, 0, 0))
    plunge = with_(s, hips=(30, 0, 0), abdomen=(10, 0, 0), chest=(10, 0, 0),
                   head=(-10, 0, 0),
                   shoulder_L=(-160, -10, 0), elbow_L=(-6, 0, 0),
                   shoulder_R=(-160, 10, 0), elbow_R=(-6, 0, 0),
                   hip_L=(10, -6, 0), knee_L=(20, 0, 0),
                   hip_R=(20, 6, 0), knee_R=(30, 0, 0))

    def hammer(t):
        return with_(stand(0.0, t), chest=(4, 0, 0), head=(6, 0, 0),
                     kb=(-20, 0, 0),
                     shoulder_L=(-50 - 6 * t, -12, 0), elbow_L=(-80, 0, 0),
                     shoulder_R=(-50 + 6 * t, 12, 0), elbow_R=(-80 + 8 * t, 0, 0))

    spread = with_(s, chest=(-10, 0, 0), head=(-10, 0, 0),
                   kb=(0, 0, 0, 0.0),
                   shoulder_L=(-80, -70, 0), elbow_L=(-10, 0, 0),
                   shoulder_R=(-80, 70, 0), elbow_R=(-10, 0, 0))
    slam = with_(s, hips=(-20, 0, 0), abdomen=(20, 0, 0), chest=(24, 0, 0),
                 shoulder_R=(-10, 10, 0), elbow_R=(-40, 0, 0),
                 hip_L=(-70, -8, 0), knee_L=(110, 0, 0), ankle_L=(-30, 0, 0),
                 hip_R=(-70, 8, 0), knee_R=(110, 0, 0), ankle_R=(-30, 0, 0))

    return [
        Clip("idle", 8, 8, True, idle, overlap=0.02),
        h.stride(run, K, fps=12),
        h.shot("attack", 8, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.4, dive_in, o(0, 0.06, -0.12)),
            (0.7, plunge, o(0, -0.40, 0.10), "out"),
            (1.0, plunge, o(0, -0.55, -0.05))]),
        h.shot("secondary", 8, 16, [(i / 8, hammer((-1) ** i), o(0, 0, 0), "linear")
                                    for i in range(9)], overlap=0.0),
        h.shot("special", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.35, spread, o(0, 0, 0.02), "out"),
            (0.75, spread, o(0, 0, 0.02)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("mobility", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.4, slam, o(0, 0, -0.30), "in"),
            (0.7, slam, o(0, 0, -0.38)),
            (1.0, with_(slam, kb=(0, 0, 0, 0.0)), o(0, 0, -0.9), "in")]),
    ] + h.hit_and_death(stand, K)
