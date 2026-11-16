"""Nyx, the Hive Mother: body from her generated model, the CRT as a prop.

The body is art/models/Nyx.glb (Meshy, from the A-pose concept art), on
the joints a human placed in the fit editor (art/models/Nyx.fit.json).
Poses that put a hand somewhere exact (carry, heave) were solved for that
fit; re-solve them if it changes. Nothing here changes her proportions.

The CRT is its own model, art/models/NyxCRT.glb, bound rigidly to a joint
under her right hand so it can leave her: she carries it on her right
shoulder, sets it on the floor before she dissolves into a swarm ("drop"),
and lifts it back up when she reforms ("lift"). In the game the CRT on the
floor is drawn from its own sheet (NyxCRT), and her pixel dissolve, the
swarm and the cloud are runtime effects (src/ArenaView.cpp).

Who she is in motion: unhurried. She is the one person in the arena who is
not in a hurry; the swarm does the work. Weight on one hip, the free hand
loose, the CRT never wobbles.

Preview with preview.py nyx idle out.png 0,4.
"""

import math
import os

import numpy as np

from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Nyx"
MODEL = "art/models/Nyx.glb"
CRT_MODEL = "art/models/NyxCRT.glb"
HEIGHT = 1.85                        # forge units, to the top of the hair
CRT_HEIGHT = 0.36                    # the set is about her shoulders' width
YAW = 0.0
STYLE = "ink"
RIM = (236, 62, 163)                 # coat magenta; the screen is the cyan
CELL = (176, 156, 128)
FOLLOW = {}

FIT = skin.load_fit(os.path.join(REPO, MODEL))

# The CRT has two mounts, one shown at a time (pose scale 0 hides the
# other): "crt" rides the chest, sitting on her right shoulder beside her
# head; "crt_hand" hangs from her right hand for setting it down and lifting
# it. Positions are from each joint's rest position, in forge units.
CRT_ON_SHOULDER = (-0.30, 0.13, 0.24)    # from the chest joint
CRT_IN_HAND = (0.0, -0.02, -0.12)        # from the wrist, arm hanging
SHOW, HIDE = 1.0, 0.0


def attach_prop(model, path, joint, height, offset, material="crt"):
    """A second model file as a rigid part of `joint`: every vertex weighted
    to that joint alone. Textured like the body."""
    obj, v = skin.load_mesh(path, height)
    v = v - np.array([0.0, 0.0, height / 2.0])     # centre it on its joint
    rest = model.rest_world(joint)
    v = v + np.array(rest) + np.array(offset)
    model.material(material, (200, 196, 180))
    model.materials[material].textured = True
    weights = np.ones((len(v), 1))
    s = skin.Skin(obj, v, [joint], {joint: np.array(rest)}, weights,
                  model.materials[material].bmat)
    s.obj.name = f"{model.name}_{material}"
    skin._set_coords(s.obj.data, v)
    model.skins.append(s)
    return s


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    crt = os.path.join(REPO, CRT_MODEL)
    m.joint("crt", "chest", m.rest_world("chest"))
    attach_prop(m, crt, "crt", CRT_HEIGHT, CRT_ON_SHOULDER)
    m.joint("crt_hand", "wrist_R", m.rest_world("wrist_R"))
    attach_prop(m, crt, "crt_hand", CRT_HEIGHT, CRT_IN_HAND,
                material="crt_hand")
    return m, stand()


# ---------------------------------------------------------------------------
# Poses, as joint angles (degrees). x: her left; -y: forward; z: up.
# A hanging limb swings forward with negative x; elbows bend with negative x;
# the spine leans forward with positive x; z twists toward her left.
# ---------------------------------------------------------------------------

def carry():
    """The right arm up, elbow out at chest height, the hand by her ear
    steadying the set on her shoulder (solved for her fitted arm: the wrist
    lands 0.27 out and level with her eyes, the elbow 0.46 out at shoulder
    height; re-solve with the arm if the fit changes)."""
    return {
        "shoulder_R": (-96, -4, -100),
        "elbow_R": (-146, 0, 0),
        "wrist_R": (0, 0, 0),
        "crt": (0, 12, 0, SHOW),
        "crt_hand": (0, 0, 0, HIDE),
    }


def in_hands(pitch):
    """The set held in her right hand, levelled against the arm's `pitch`
    (the sum of the x rotations from the hips to the elbow)."""
    return {"crt": (0, 0, 0, HIDE), "crt_hand": (-pitch, 0, 0, SHOW)}


def stand(breath=0.0):
    """Hip cocked to her left, free hand loose, the set on her shoulder."""
    pose = {
        "hips": (0, 4, 6),
        "abdomen": (2 + 1.5 * breath, -3, -3),
        "chest": (2 + 2 * breath, -3, -4),
        "neck": (0, 2, 2),
        "head": (-3, 4, 4),
        "shoulder_L": (2, 6, 0), "elbow_L": (-14, 0, 0), "wrist_L": (0, 0, 0),
        "hip_L": (-2, -4, -4), "knee_L": (4, 0, 0), "ankle_L": (-2, 4, 0),
        "hip_R": (4, 6, 8), "knee_R": (10, 0, 0), "ankle_R": (-12, -6, 0),
    }
    pose.update(carry())
    return pose


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def walk(p):
    """Her walk: a long, loose stride; the CRT shoulder stays level."""
    s = math.sin(p * 2 * math.pi)
    c = math.cos(p * 2 * math.pi)
    pose = stand()
    pose.update({
        "hips": (4, 0, 8 * s),
        "abdomen": (4, 0, -3 * s),
        "chest": (4, 0, -6 * s),
        "head": (-4, 0, 4 * s),
        "shoulder_L": (30 * s, 8, 0), "elbow_L": (-20 - 12 * max(0.0, s), 0, 0),
        "hip_L": (-34 * s - 4, 0, 0), "knee_L": (12 + 52 * max(0.0, c), 0, 0),
        "ankle_L": (-8 + 14 * s, 0, 0),
        "hip_R": (34 * s - 4, 0, 0), "knee_R": (12 + 52 * max(0.0, -c), 0, 0),
        "ankle_R": (-8 - 14 * s, 0, 0),
    })
    return pose


# How deep she crouches to set the set on the floor, and how far the root
# drops so her feet stay on it (measured for the fit: re-measure if it
# changes). At DEEP the set's bottom is 5 cm off the floor.
DEEP = 1.35
DEEP_ROOT = -0.585
HALF_ROOT = -0.134                   # for crouch(0.6)


def heave():
    """The set in her right hand in front of her right shoulder, on its way
    to or from the floor (solved: the wrist 0.28 in front of the shoulder,
    at the height of her collarbone)."""
    pose = {"shoulder_R": (-22, 45, 8), "elbow_R": (-130, 0, 0),
            "wrist_R": (0, 0, 0)}
    pose.update(in_hands(-22 - 130 + 12))
    return pose


def crouch(reach=1.0):
    """Knees down, back bent, both hands at the floor with the set."""
    pitch = 50 * reach - 55 * reach - 20
    pose = with_(
        stand(),
        hips=(0, 0, 0), abdomen=(24 * reach, 0, 0), chest=(26 * reach, 0, 0),
        neck=(-14, 0, 0), head=(-18, 0, 0),
        shoulder_L=(-50 * reach, 10, 0), elbow_L=(-30, 0, 0),
        shoulder_R=(-55 * reach, -10, 0), elbow_R=(-20, 0, 0),
        wrist_R=(0, 0, 0),
        hip_L=(-80 * reach, -6, -8), knee_L=(120 * reach, 0, 0),
        ankle_L=(-40 * reach, 0, 0),
        hip_R=(-60 * reach, 6, 8), knee_R=(110 * reach, 0, 0),
        ankle_R=(-50 * reach, 0, 0))
    pose.update(in_hands(pitch))
    return pose


def clips():
    base = stand()
    low = (0, 0, 0)
    out = []

    idle = [Key(f / 8, stand(0.5 - 0.5 * math.cos(2 * math.pi * f / 8)),
                (0, 0, -0.01), ease="linear") for f in range(9)]
    out.append(Clip("idle", 8, 8, True, idle, overlap=0.03))

    run = []
    for f in range(9):
        p = f / 8
        s = math.sin(p * 2 * math.pi)
        run.append(Key(p, walk(p), (0, 0, -0.03 + 0.03 * abs(s)), ease="linear"))
    out.append(Clip("run", 8, 12, True, run, overlap=0.02))

    # LMB: Nanite Swarm. The free hand flicks out at the target, palm open,
    # as if shooing the swarm off her fingers.
    flick = with_(base, chest=(0, 0, 16), abdomen=(0, 0, 6),
                  shoulder_L=(-70, 30, -10), elbow_L=(-8, 0, 0),
                  wrist_L=(0, 0, -20))
    out.append(Clip("swarm", 6, 16, False, [
        Key(0.0, base, low),
        Key(0.3, with_(flick, shoulder_L=(-40, 10, 20), elbow_L=(-70, 0, 0)),
            low, ease="out"),
        Key(0.5, flick, low, ease="out"),
        Key(1.0, base, low, ease="in")], overlap=0.03))

    # RMB (hold): Plague Bringer. Gathering: the free arm sweeps in to her
    # chest, she draws in. Release: the arm throws out wide, the wave goes.
    gather = with_(base, abdomen=(8, 0, -10), chest=(10, 0, -18),
                   head=(4, 0, -8), shoulder_L=(-50, -30, 30),
                   elbow_L=(-110, 0, 0))
    out.append(Clip("plague_charge", 6, 10, True, [
        Key(0.0, gather, (0, 0, -0.04), ease="linear"),
        Key(0.5, with_(gather, chest=(12, 0, -20)), (0, 0, -0.05), ease="linear"),
        Key(1.0, gather, (0, 0, -0.04), ease="linear")]))
    sweep = with_(base, abdomen=(-4, 0, 10), chest=(-8, 0, 22),
                  head=(-6, 0, 10), shoulder_L=(-20, 95, 0),
                  elbow_L=(-6, 0, 0))
    out.append(Clip("plague", 8, 16, False, [
        Key(0.0, gather, (0, 0, -0.04)),
        Key(0.25, sweep, (0, 0, 0.0), ease="out"),
        Key(0.6, with_(sweep, shoulder_L=(-15, 100, 0)), low),
        Key(1.0, base, low, ease="in")], overlap=0.04))

    # Q: Nanite Cloud. A snap of the fingers overhead.
    snap = with_(base, chest=(-4, 0, 6), head=(-8, 0, 0),
                 shoulder_L=(-150, 20, 0), elbow_L=(-30, 0, 0))
    out.append(Clip("cloud", 6, 14, False, [
        Key(0.0, base, low),
        Key(0.35, snap, (0, 0, 0.02), ease="out"),
        Key(0.55, with_(snap, wrist_L=(0, 0, 25)), (0, 0, 0.02)),
        Key(1.0, base, low, ease="in")], overlap=0.03))

    # Shift: Become the Swarm. She swings the set down off her shoulder and
    # sets it on the floor in front of her; the game dissolves her into the
    # swarm as the clip ends. The set changes mounts (shoulder to hand) in
    # one step at the heave, where the two nearly meet, so no frame shows
    # both copies.
    def swap(t, before, after, offset, ease="smooth"):
        return [Key(t, before, offset, ease=ease),
                Key(t + 0.001, after, offset, ease="linear")]

    heave_carry = with_(base, abdomen=(6, 0, -6), chest=(6, 0, -10))
    heave_hand = with_(heave_carry, **heave())
    out.append(Clip("drop", 7, 24, False, [
        Key(0.0, base, low),
        *swap(0.18, heave_carry, heave_hand, (0, 0, -0.04), ease="out"),
        Key(0.5, crouch(0.6), (0, 0, HALF_ROOT), ease="in"),
        Key(0.8, crouch(DEEP), (0, 0, DEEP_ROOT), ease="out"),
        Key(1.0, crouch(DEEP), (0, 0, DEEP_ROOT))]))
    # ...and lifts it back onto her shoulder when she reforms: the same
    # moves backwards, with a little dip as she takes the weight.
    out.append(Clip("lift", 11, 24, False, [
        Key(0.0, crouch(DEEP), (0, 0, DEEP_ROOT)),
        Key(0.2, crouch(DEEP), (0, 0, DEEP_ROOT - 0.015), ease="out"),
        Key(0.5, crouch(0.6), (0, 0, HALF_ROOT), ease="out"),
        *swap(0.72, heave_hand, heave_carry, (0, 0, -0.04), ease="out"),
        Key(0.85, with_(base, chest=(-3, 0, 2)), (0, 0, 0.01), ease="out"),
        Key(1.0, base, low, ease="in")]))

    # Getting hit, and going down (the CRT falls with her).
    hurt = with_(base, abdomen=(-8, 0, 6), chest=(-18, -4, 12),
                 head=(-24, 0, 14), shoulder_L=(-24, 40, -6),
                 elbow_L=(-50, 0, 0))
    out.append(Clip("hit", 6, 20, False, [
        Key(0.0, base, low), Key(0.22, hurt, (0, 0.10, -0.06), ease="out"),
        Key(1.0, base, low)], overlap=0.025))
    lying = with_(crouch(1.0), hips=(0, 0, 0), abdomen=(-4, 0, 0),
                  chest=(-6, 0, 8), head=(-10, 0, 20),
                  hip_L=(-30, 0, -10), knee_L=(40, 0, 0),
                  hip_R=(-10, 0, 14), knee_R=(20, 0, 0))
    out.append(Clip("death", 12, 14, False, [
        Key(0.0, hurt, (0, 0.06, -0.06)),
        Key(0.4, crouch(DEEP), (0, 0.10, DEEP_ROOT), ease="out"),
        Key(0.75, lying, (0, 0.10, 0.12), rot=(-88, 0, 0), ease="in"),
        Key(1.0, lying, (0, 0.10, 0.12), rot=(-90, 0, 0))], overlap=0.02))
    return out
