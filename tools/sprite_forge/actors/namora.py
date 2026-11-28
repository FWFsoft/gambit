"""Namora, the Dragon Monk (Defender). Body from art/models/Namora.glb;
her tail is art/models/NamoraTail.glb on its own chain of joints
(_tail.py straightens the curled model and binds it to tail_0..tail_7).

Shape brief: a dragon monk: long dark hair, a gold horned crown, red
wraps and sash over bare arms, baggy white trousers, clawed green dragon
feet. The tail is the signature piece: green-scaled with a ridge of
spines and an arrow fin, it sweeps out behind her and along the floor,
so from every side she trails a long curve nobody else has. She stands
in a fighting stance: feet wide, one open palm forward, the other fist
chambered at the hip.

Kit (Character Concepts doc):
  attack     Expel Soul: a straight palm strike.
  secondary  Dragon Breath (hold): a deep inhale, chest up and arms
             drawn in, then a two-handed push of wind.
  special    Rebuke the Infirm: she spins and the tail sweeps a full
             circle around her.
  mobility   Ward the Innocent: arms open wide, palms out, radiating
             her Guard to an ally.
"""

import math
import os

from forge import Model
import _hero as h
import _tail
from _hero import with_
import skin

NAME = "Namora"
MODEL = "art/models/Namora.glb"
TAIL_MODEL = "art/models/NamoraTail.glb"
HEIGHT = 1.85
YAW = 0.0
STYLE = "ink"
RIM = (0, 200, 200)                  # her cyan
CELL = (248, 196, 150)               # wide and deep: the tail
K = HEIGHT / 1.95
TAIL_LENGTH = 1.8 * K
TAIL_JOINTS = 8

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    hips = m.rest_world("hips")
    # From the small of her back, straight down and back; poses lift it.
    _tail.attach(m, TAIL_MODEL, "hips", (hips[0], hips[1] + 0.10 * K, hips[2] - 0.04 * K),
                 (0.0, 0.35, -1.0), TAIL_LENGTH, joints=TAIL_JOINTS)
    return m, stand()


def tail(lift=15.0, drop=5.0, bend=45.0, flick=8.0, sway=0.0, swing=0.0):
    """The tail as joint angles. Rest is straight down and a little back.
    `lift` swings the whole tail back from the base; joints 1-3 `drop` it
    toward the floor, joint 4 `bend`s it level along the floor, and the
    tip `flick`s up. `sway` snakes it side to side, `swing` turns the whole
    tail round her (yaw at the base). Positive pitch lifts it backward."""
    pitch = [lift, drop, drop, drop, bend, 0.0, 0.0, flick]
    p = {}
    for i in range(TAIL_JOINTS):
        yaw = (swing if i == 0 else 0.0) + (sway * math.sin(i * 0.9) if i >= 4 else 0.0)
        p[f"tail_{i}"] = (pitch[i], 0, yaw)
    return p


def stand(breath=0.0, sway=0.0):
    """A low monk's stance: feet wide, left palm forward, right fist
    chambered at the hip."""
    p = {
        "hips": (-4, 0, -14),
        "abdomen": (2 + 1 * breath, 0, 6),
        "chest": (2 + 2 * breath, 0, 8),
        "neck": (0, 0, 0),
        "head": (-4, 0, 10),
        "shoulder_L": (-60, -10, 0), "elbow_L": (-30, 0, 0),
        "shoulder_R": (20, 24, 0), "elbow_R": (-110, 0, 0),
        "hip_L": (-24, -16, -10), "knee_L": (30, 0, 0), "ankle_L": (-6, 10, 0),
        "hip_R": (8, 16, 10), "knee_R": (26, 0, 0), "ankle_R": (-20, -10, 0),
    }
    p.update(tail(sway=sway + 6 * breath))
    return p


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    from forge import Clip, Key
    s = stand()
    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        idle.append(Key(f / 8, stand(b, 8 * math.sin(2 * math.pi * f / 8)),
                        o(0, 0, -0.01 * b), ease="linear"))

    base_run = h.run_pose(stand, lean=12, arm=34, leg=36, knee=66)

    def run(phase):
        return with_(base_run(phase), **tail(lift=30, bend=30,
                                             sway=14 * math.sin(phase * 2 * math.pi)))

    palm = with_(s, hips=(-4, 0, 14), chest=(6, 0, -10),
                 shoulder_L=(20, -20, 0), elbow_L=(-110, 0, 0),
                 shoulder_R=(-90, 4, 0), elbow_R=(-4, 0, 0),
                 hip_L=(-36, -16, 0), knee_L=(40, 0, 0))

    inhale = with_(s, chest=(-16, 0, 0), head=(-20, 0, 0),
                   shoulder_L=(-40, -40, 0), elbow_L=(-120, 0, 0),
                   shoulder_R=(-40, 40, 0), elbow_R=(-120, 0, 0),
                   **tail(lift=25, flick=30))
    exhale = with_(s, chest=(14, 0, 0), head=(6, 0, 0),
                   shoulder_L=(-88, -8, 0), elbow_L=(-4, 0, 0),
                   shoulder_R=(-88, 8, 0), elbow_R=(-4, 0, 0),
                   hip_L=(-40, -16, 0), knee_L=(44, 0, 0))

    def spin(t):
        """t 0..1: the body turns a full circle, the tail out flat."""
        a = 360 * t
        return with_(s, hips=(-4, 0, a), chest=(4, 0, -10),
                     shoulder_L=(-30, -60, 0), shoulder_R=(-30, 60, 0),
                     elbow_L=(-20, 0, 0), elbow_R=(-20, 0, 0),
                     **tail(lift=70, drop=0, bend=0, flick=0))

    ward = with_(s, chest=(-8, 0, 0), head=(-10, 0, 0),
                 shoulder_L=(-80, -70, 0), elbow_L=(-6, 0, 0),
                 shoulder_R=(-80, 70, 0), elbow_R=(-6, 0, 0),
                 hip_L=(-10, -14, 0), hip_R=(-10, 14, 0))

    return [
        Clip("idle", 8, 8, True, idle, overlap=0.03),
        h.stride(run, K, fps=12),
        h.shot("attack", 7, 16, [
            (0.0, s, o(0, 0, 0)),
            (0.5, palm, o(0, -0.08, -0.03), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.45, inhale, o(0, 0.04, 0.02)),
            (0.62, exhale, o(0, -0.06, -0.03), "out"),
            (0.85, exhale, o(0, -0.06, -0.03)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 9, 14, [(i / 8, spin(i / 8), o(0, 0, -0.02), "linear")
                                  for i in range(9)], overlap=0.0),
        h.shot("mobility", 8, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.4, ward, o(0, 0, 0.02), "out"),
            (0.75, ward, o(0, 0, 0.02)),
            (1.0, s, o(0, 0, 0))]),
    ] + h.hit_and_death(stand, K)
