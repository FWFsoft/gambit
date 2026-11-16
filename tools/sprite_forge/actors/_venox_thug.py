"""The Venoxarid street thugs: shared motion for the grunt and the elite.

Both bodies are generated models (art/models/VenoxaridGrunt.glb and
VenoxaridElite.glb) on the forge's humanoid skeleton, so one set of poses
drives both. Nothing here changes their proportions; the elite is simply a
bigger model (HEIGHT), and `k` scales the root offsets to match.

Who they are in motion: back-alley muscle, not animals. A slouched,
shoulders-rolled swagger; a heavy jog; and one attack, a big overhand
right. The wind-up (the first 45% of `attack`, which the client holds for
the whole server telegraph) rears the fist far back so the hit is readable
before it lands.

Pose values are joint angles in degrees (pitch, roll, yaw): a negative
shoulder or hip pitch swings the limb forward, a negative elbow bends it,
a positive knee bends it, and + yaw turns him to his left.
"""

import math
import os

from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))


def build(name, model, height, fit, yaw=0.0, head_turn=0.0):
    m = Model(name)
    skin.attach(m, os.path.join(REPO, model), height, fit, yaw=yaw)
    return m, straighten(stand(), head_turn)


def straighten(pose, head_turn):
    """Undo the head turn the model was generated with.

    Both models look a little to their own left in the A-pose. The fit
    cannot fix that (it places joints, it does not turn the mesh), so every
    pose turns the head back by `head_turn` degrees. With it, a head yaw of
    0 in a pose means "looking straight ahead"."""
    rx, ry, rz = pose.get("head", (0, 0, 0))[:3]
    return with_(pose, head=(rx, ry, rz - head_turn))


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def stand(breath=0.0):
    """The slouch: weight on the back leg, chin down, hands loose and a
    little forward, ready to shove someone."""
    return {
        "hips": (0, 0, 10),
        "abdomen": (6 + 2 * breath, 0, -3),
        "chest": (9 + 3 * breath, 0, -5),
        "neck": (-4, 0, 0),
        "head": (6, 0, -2),
        "shoulder_L": (-10, -14 - 2 * breath, 0), "elbow_L": (-34, 0, 0),
        "shoulder_R": (-4, 14 + 2 * breath, 0), "elbow_R": (-40, 0, 0),
        "hip_L": (-12, -10, -8), "knee_L": (16, 0, 0), "ankle_L": (-4, 8, 0),
        "hip_R": (6, 10, 10), "knee_R": (12, 0, 0), "ankle_R": (-14, -8, 0),
    }


def run_pose(phase):
    s = math.sin(phase * 2 * math.pi)
    return with_(stand(), **{
        "hips": (0, 0, 7 * s),
        "abdomen": (8, 0, -3 * s),
        "chest": (12, 0, -9 * s),
        "neck": (-6, 0, 2 * s),
        "head": (-4, 0, 0),
        "shoulder_L": (-12 - 34 * s, -16, 0), "elbow_L": (-58, 0, 0),
        "shoulder_R": (-12 + 34 * s, 16, 0), "elbow_R": (-58, 0, 0),
        "hip_L": (32 * s - 8, -6, 0), "knee_L": (14 + max(0.0, s) * 58, 0, 0),
        "ankle_L": (-8, 0, 0),
        "hip_R": (-32 * s - 8, 6, 0), "knee_R": (14 + max(0.0, -s) * 58, 0, 0),
        "ankle_R": (-8, 0, 0),
    })


def clips(k=1.0, head_turn=0.0):
    """k: the body's size relative to the grunt, for root offsets.
    head_turn: see straighten()."""
    out = _clips(k)
    for clip in out:
        for key in clip.keys:
            key.pose = straighten(key.pose, head_turn)
    return out


def _clips(k):
    def off(x, y, z):
        return (x * k, y * k, z * k)

    idle = []
    for f in range(9):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / 8)
        idle.append(Key(f / 8, stand(b), off(0, 0, -0.02 - 0.012 * b),
                        ease="linear"))

    run = []
    for f in range(9):
        p = f / 8
        run.append(Key(p, run_pose(p),
                       off(0, 0, -0.03 + 0.04 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    s = stand()
    # Wind-up: turn away from the target, right fist cocked high behind the
    # head, left hand out front to measure the distance.
    coil = with_(s, hips=(0, 0, -8), abdomen=(-4, 0, -8), chest=(-10, 0, -16),
                 neck=(0, 0, 10), head=(4, 0, 18),
                 shoulder_R=(34, 62, 0), elbow_R=(-112, 0, 0),
                 shoulder_L=(-62, -12, 0), elbow_L=(-36, 0, 0),
                 hip_L=(-24, -10, -8), knee_L=(26, 0, 0),
                 hip_R=(16, 10, 10), knee_R=(30, 0, 0))
    # The hit: hips and chest whip round, the right arm comes over the top.
    strike = with_(s, hips=(0, 0, 10), abdomen=(12, 0, 6), chest=(22, 0, 14),
                   neck=(0, 0, -10), head=(4, 0, -16),
                   shoulder_R=(-104, 8, 0), elbow_R=(-12, 0, 0),
                   shoulder_L=(28, -22, 0), elbow_L=(-78, 0, 0),
                   hip_L=(-34, -10, -8), knee_L=(44, 0, 0),
                   hip_R=(26, 10, 10), knee_R=(12, 0, 0))
    follow = with_(strike, chest=(28, 0, 22), shoulder_R=(-70, -10, 0),
                   elbow_R=(-40, 0, 0))
    hurt = with_(s, abdomen=(-12, 0, 6), chest=(-20, 0, 10), neck=(-10, 0, 0),
                 head=(-22, 0, 12),
                 shoulder_L=(-30, -44, 0), elbow_L=(-50, 0, 0),
                 shoulder_R=(-24, 46, 0), elbow_R=(-56, 0, 0),
                 hip_L=(4, -10, -8), knee_L=(24, 0, 0))
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
        Clip("run", 8, 12, True, run),
        Clip("attack", 9, 16, False, [
            Key(0.0, s, off(0, 0, -0.02)),
            Key(0.45, coil, off(0, 0.08, -0.05)),
            Key(0.6, strike, off(0, -0.16, -0.08), ease="out"),
            Key(0.8, follow, off(0, -0.17, -0.09)),
            Key(1.0, s, off(0, 0, -0.02))], overlap=0.012),
        Clip("hit", 4, 16, False, [
            Key(0.0, s, off(0, 0, -0.02)),
            Key(0.3, hurt, off(0, 0.08, -0.03), ease="out"),
            Key(1.0, s, off(0, 0, -0.02))]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, off(0, 0.05, -0.03)),
            Key(0.35, buckle, off(0, 0.10, -0.22)),
            # Flat on his back. The root turns about the feet, so slide him
            # forward to keep the body inside the cell: his legs go out
            # from under him.
            Key(0.7, lying, off(0, -1.15, 0.20), rot=(-86, 0, 8), ease="in"),
            Key(1.0, lying, off(0, -1.15, 0.20), rot=(-90, 0, 8))]),
    ]
