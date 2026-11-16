"""Nolan: body from his generated model, motion as editable data.

The body is art/models/Nolan.glb (Meshy, from the A-pose concept art), on
the joints a human placed (art/models/Nolan.fit.json). Nothing here changes
his proportions.

The motion is data (anim.py): controls, which say where the hips, hands and
feet are, and the clips that use them. Once a human has saved from the pose
editor, art/models/Nolan.anim.json replaces the starting point below.

Who he is in motion: the hype anime kicker, the foil to Wade's plain
boxing. The reference is firebending as Zuko does it: low, wide, grounded
stances, a hand planted on the floor, and the legs drawing big arcs. Every
attack is a kick; the hands only balance and plant.

Preview with preview.py nolan sweep out.png 0,2,4.
"""

import math
import os

import bpy

import _rig
import anim
import forge
from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Nolan"
MODEL = "art/models/Nolan.glb"
HEIGHT = 1.9                         # forge units, to the top of the hair
YAW = 0.0
STYLE = "ink"
RIM = (64, 200, 255)                 # electric blue: the hair and lightning
CELL = (176, 156, 128)
FOLLOW = {}

# Saved from the joint editor (fit.py --editor) as art/models/Nolan.fit.json.
FIT = skin.load_fit(os.path.join(REPO, MODEL))


def build():
    base = guard()
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    return m, base


_TABLE = None
_RIG = None


def joint_table():
    """This body's joints as [(name, parent, rest position)], for the rig."""
    global _TABLE
    if _TABLE is None:
        data = skin.prepare(os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW, log=lambda *a: None)
        bpy.data.objects.remove(data["obj"], do_unlink=True)
        _TABLE = [(n, p, tuple(float(c) for c in data["rest_pos"][n]))
                  for n, p in skin.HUMANOID]
    return _TABLE


def rig():
    global _RIG
    if _RIG is None:
        _RIG = _rig.Rig(joint_table())
    return _RIG


def current():
    """The saved animation if there is one, else the starting point."""
    data = anim.load(os.path.join(REPO, MODEL)) or animation()
    anim.check(data)
    return data


def clips():
    return anim.bake(current(), rig(), guard())


# ---------------------------------------------------------------------------
# The starting point, by position
# ---------------------------------------------------------------------------
# He is left foot forward. x is his left, -y is forward, z is up.

FLOOR = 0.168                        # ankle height with the foot flat

# The stance's planted feet: lead foot forward, rear foot turned out.
LEAD = dict(at=(0.20, -0.30, FLOOR), rot=(0, 0, 8), knee=(0.25, -1, 0))
REAR = dict(at=(-0.24, 0.30, FLOOR), rot=(0, 0, -55), knee=(-1, -0.15, 0))
# The stance's hands: lead palm out in front, rear hand chambered at the ribs.
PALM = dict(at=(0.06, -0.40, -0.26), elbow=(0.5, 0, -1))
CHAMBER = dict(at=(-0.20, 0.02, -0.52), elbow=(-0.2, 1, -0.2))


def ctl(off=(0, 0.04, -0.17), twist=(-22, -6, -10), lean=(4, 6), side=(0, 0),
        tilt=0.0, nod=-10.0, look=0.9, L=PALM, R=CHAMBER, fl=LEAD, fr=REAR):
    """A control.

    off    hips, from standing straight (x his left, -y forward, z up)
    twist  yaw of (hips, abdomen, chest); + turns him to his left
    lean   forward bend of (abdomen, chest); side: side bend of the same
    tilt   hips pitch; nod: extra head pitch (+ chin down)
    look   how much the head turns back to face the target
    L, R   dict(at=, elbow=): the wrist goes to `at`, measured from the head
           joint (so a hand travels with the body); dict(floor=) puts it at
           a point in the world instead (a planted hand). The elbow sticks
           out along `elbow`.
    fl, fr dict(at=, rot=(pitch, roll, yaw), knee=): the ankle in the world,
           the foot's rotation, and the way the knee points.
    """
    r = rig()
    c = {"offset": tuple(off)}
    c["hips"] = (tilt, 0, twist[0])
    c["abdomen"] = (lean[0], side[0], twist[1])
    c["chest"] = (lean[1], side[1], twist[2])
    yaw, bend, roll = sum(twist), lean[0] + lean[1] + tilt, side[0] + side[1]
    c["neck"] = (-0.3 * bend, -0.3 * roll, -0.35 * look * yaw)
    c["head"] = (-0.4 * bend + nod, -0.4 * roll, -0.65 * look * yaw)
    head = r.joint_at(c, "head")
    for s, arm in (("L", L), ("R", R)):
        sh = r.joint_at(c, f"shoulder_{s}")
        if "floor" in arm:
            c[f"hand_{s}"] = tuple(arm["floor"])
        else:
            c[f"hand_{s}"] = tuple(head[i] + arm["at"][i] for i in range(3))
        c[f"elbow_{s}"] = _rig.aim(sh, arm.get("elbow", (0, 0, -1)))
        c[f"wrist_{s}"] = (0, 0, 0)
        c[f"fore_{s}"] = (1.0, 1.0, 1.0)
    for s, foot in (("L", fl), ("R", fr)):
        c[f"foot_{s}"] = tuple(foot["at"])
        c[f"footrot_{s}"] = tuple(foot.get("rot", (0, 0, 0)))
        c[f"knee_{s}"] = _rig.aim(r.joint_at(c, f"hip_{s}"), foot.get("knee", (0, -1, 0)))
    return c


_GUARD = None


def guard():
    """The stance as joint angles: what the model stands in and what fills
    any joint a clip leaves alone."""
    global _GUARD
    if _GUARD is None:
        _GUARD = rig().solve(ctl())[0]
    return dict(_GUARD)


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(p):
    """The anime run: chest thrown forward, arms trailing straight back."""
    s = math.sin(p * 2 * math.pi)

    def leg(sign):
        swing = -40 * s * sign - 10         # negative = forward
        fold = 14 + 84 * max(0.0, s * sign * -1)
        return (swing, 0, 0), (fold, 0, 0), (-8 - 10 * s * sign, 0, 0)

    hl, kl, al = leg(1)
    hr, kr, ar = leg(-1)
    return {
        "hips": (10, 0, 8 * s), "abdomen": (14, 0, -3 * s),
        "chest": (20, 0, -8 * s), "neck": (-14, 0, 2 * s), "head": (-20, 0, 3 * s),
        "shoulder_L": (48 + 5 * s, -16, 0), "elbow_L": (-10, 0, 0), "wrist_L": (0, 0, 0),
        "shoulder_R": (48 - 5 * s, 16, 0), "elbow_R": (-10, 0, 0), "wrist_R": (0, 0, 0),
        "hip_L": hl, "knee_L": kl, "ankle_L": al,
        "hip_R": hr, "knee_R": kr, "ankle_R": ar,
    }


def animation():
    r = rig()
    out = anim.Builder()
    G = ctl()
    out.control(G, "stance")

    def clip(name, frames, fps, keys, loop=False, overlap=0.0):
        full = []
        for k in keys:
            how = k[2] if len(k) > 2 else "smooth"
            rot = k[3] if len(k) > 3 else (0, 0, 0)
            full.append((k[0], k[1], how, rot))
        out.clip(name, frames, fps, loop, overlap, full)

    def angles(name, frames, fps, loop, keys, overlap=0.0):
        """A clip authored as joint angles: one control per frame."""
        src = Clip(name, frames, fps, loop, keys)
        base, full = guard(), []
        for f in range(frames):
            t = f / frames if loop else f / max(1, frames - 1)
            pose, off, rot = forge.sample(src, t, base)
            full.append((f, r.control(pose, off), "linear", rot))
        if loop:
            full.append((frames, full[0][1], "linear", full[0][3]))
        out.clip(name, frames, fps, loop, overlap, full)

    # ---- idle: the stance breathes, low and coiled ---------------------------
    sunk = ctl(off=(0, 0.05, -0.22), lean=(6, 8),
               L=dict(at=(0.07, -0.38, -0.29), elbow=(0.5, 0, -1)),
               R=dict(at=(-0.21, 0.03, -0.54), elbow=(-0.2, 1, -0.2)))
    clip("idle", 12, 10, [(0, G), (6, sunk), (12, G)], loop=True, overlap=0.03)

    run = []
    for f in range(11):
        p = f / 10
        s = math.sin(p * 2 * math.pi)
        run.append(Key(p, run_pose(p), (0, -0.05, -0.10 + 0.06 * (1 - abs(s))),
                       ease="linear"))
    angles("run", 10, 15, True, run, overlap=0.02)

    # Damage lands when the button is pressed, so every kick hits early: one
    # frame of load, the kick on frame 2, a held frame, then the way out.

    # ---- LMB: the sweep. Drop onto the lead hand, the rear leg scythes the
    # floor from his right, across the front, to his left; then a hop back ----
    plant = dict(floor=(0.42, -0.40, 0.12), elbow=(1, 0, 0.3))
    squat = dict(at=(0.16, -0.18, 0.22), rot=(25, 0, 10), knee=(0.6, -1, 0))
    clip("sweep", 14, 24, [
        (0, G),
        (1, ctl(off=(0.05, -0.02, -0.45), twist=(-40, -10, -15), lean=(20, 25),
                L=dict(floor=(0.38, -0.38, 0.30), elbow=(1, 0, 0.3)),
                R=dict(at=(-0.40, 0.25, -0.25), elbow=(-1, 0.5, 0)),
                fl=squat,
                fr=dict(at=(-0.75, 0.25, 0.20), rot=(0, 0, -80), knee=(-1, 0, 0.4))),
         "out"),
        (2, ctl(off=(0.08, -0.05, -0.62), twist=(10, 5, 5), lean=(28, 30),
                L=plant, R=dict(at=(-0.45, 0.20, -0.10), elbow=(-1, 0.5, 0.2)),
                fl=squat,
                fr=dict(at=(0.05, -0.95, 0.14), rot=(-10, 0, 60), knee=(0.3, -0.3, 1))),
         "snap"),
        (3, ctl(off=(0.08, -0.05, -0.62), twist=(35, 12, 12), lean=(28, 30),
                L=plant, R=dict(at=(-0.45, 0.25, -0.05), elbow=(-1, 0.5, 0.2)),
                fl=squat,
                fr=dict(at=(0.65, -0.65, 0.14), rot=(-10, 0, 100), knee=(0.3, -0.3, 1)))),
        (4, ctl(off=(0.08, -0.04, -0.60), twist=(42, 14, 14), lean=(26, 28),
                L=plant, R=dict(at=(-0.42, 0.28, -0.05), elbow=(-1, 0.5, 0.2)),
                fl=squat,
                fr=dict(at=(0.75, -0.45, 0.14), rot=(-10, 0, 110), knee=(0.3, -0.3, 1)))),
        (6, ctl(off=(0, 0.0, -0.36), twist=(0, 0, 0), lean=(14, 16),
                L=dict(at=(0.22, -0.25, -0.45), elbow=(0.6, 0.3, -1)),
                R=dict(at=(-0.24, -0.05, -0.45), elbow=(-0.6, 0.6, -1)),
                fl=dict(at=(0.18, -0.10, FLOOR), rot=(0, 0, 10), knee=(0.4, -1, 0)),
                fr=dict(at=(-0.20, 0.18, FLOOR), rot=(0, 0, -30), knee=(-0.6, -1, 0))),
         "in"),
        (8, ctl(off=(0, 0.16, 0.12), twist=(-14, -4, -6), lean=(6, 8),
                L=dict(at=(0.10, -0.34, -0.22), elbow=(0.5, 0, -1)),
                R=dict(at=(-0.26, 0.05, -0.40), elbow=(-0.4, 1, -0.2)),
                fl=dict(at=(0.14, 0.02, 0.52), rot=(30, 0, 8), knee=(0.2, -1, 0.1)),
                fr=dict(at=(-0.16, 0.34, 0.56), rot=(35, 0, -30), knee=(-0.6, -1, 0))),
         "out"),
        (11, ctl(off=(0, 0.06, -0.28), lean=(8, 10)), "in"),
        (13, G),
    ])

    # ---- RMB (hold): the coil. Wound to his right, ready to let go ----------
    def coil(drop):
        return ctl(off=(0, 0.08, -0.34 - drop), twist=(-50, -15, -20), lean=(10, 14),
                   L=dict(at=(-0.12, -0.26, -0.34), elbow=(0.6, -0.4, -0.6)),
                   R=dict(at=(-0.32, 0.24, -0.44), elbow=(-0.6, 1, 0)),
                   fl=dict(at=(0.20, -0.30, FLOOR), rot=(0, 0, -5), knee=(-0.1, -1, 0)),
                   fr=dict(at=(-0.24, 0.30, FLOOR), rot=(0, 0, -70), knee=(-1, 0, 0)))

    C = coil(0.0)
    clip("lotus_charge", 8, 12, [(0, C), (4, coil(0.04)), (8, C)], loop=True)

    # ---- RMB (release): the lotus. A full turn in the air, to his left, the
    # rear leg out flat the whole way round ------------------------------------
    def wheel(z, reach=1.0):
        return ctl(off=(0, 0, z), twist=(20, 10, 15), lean=(-5, -8),
                   L=dict(at=(0.50, 0.08, -0.15), elbow=(0.3, 1, -0.3)),
                   R=dict(at=(-0.44, 0.26, -0.22), elbow=(-0.3, 1, -0.3)),
                   fl=dict(at=(0.12, 0.05, 0.62 + z), rot=(30, 0, 0), knee=(0.4, -1, 0)),
                   fr=dict(at=(-0.30 * reach, -0.82 * reach, 1.00 + z),
                           rot=(-60, 0, 0), knee=(-0.4, -0.3, 1)))

    clip("lotus", 12, 24, [
        (0, C),
        (1, ctl(off=(0, 0.02, -0.12), twist=(-24, -6, -8), lean=(4, 6),
                L=dict(at=(0.30, -0.12, -0.25), elbow=(0.6, 0.6, -0.6)),
                R=dict(at=(-0.40, 0.22, -0.30), elbow=(-0.6, 1, 0)),
                fr=dict(at=(-0.50, 0.10, 0.50), rot=(10, 0, -40), knee=(-1, -0.6, 0.3))),
         "out"),
        (2, wheel(0.18), "snap", (0, 0, 100)),
        (3, wheel(0.22), "linear", (0, 0, 190)),
        (4, wheel(0.16), "linear", (0, 0, 280)),
        (6, ctl(off=(0, 0.04, -0.32), lean=(10, 12)), "linear", (0, 0, 360)),
        (9, ctl(off=(0, 0.04, -0.20)), "smooth", (0, 0, 360)),
        (11, G, "smooth", (0, 0, 360)),
    ])

    # ---- Q: the black-out. An axe kick: the leg goes straight up, then the
    # heel comes down through the target and he lands in a crouch -------------
    clip("blackout", 12, 24, [
        (0, G),
        (1, ctl(off=(0, 0.02, 0.20), twist=(0, 0, 0), lean=(-8, -10),
                L=dict(at=(0.16, -0.34, -0.18), elbow=(0.6, 0, -1)),
                R=dict(at=(-0.40, 0.26, -0.08), elbow=(-0.6, 1, 0)),
                fl=dict(at=(0.14, 0.12, 0.74), rot=(40, 0, 0), knee=(0.2, -1, 0)),
                fr=dict(at=(-0.10, -0.42, 2.00), rot=(-120, 0, 0), knee=(-0.2, -1, 0.3))),
         "out"),
        (2, ctl(off=(0, -0.08, 0.26), twist=(14, 4, 6), lean=(24, 28),
                L=dict(at=(0.30, -0.08, -0.52), elbow=(0.8, 0.4, -0.4)),
                R=dict(at=(-0.36, 0.36, 0.02), elbow=(-0.6, 1, 0.3)),
                fl=dict(at=(0.14, 0.14, 0.84), rot=(45, 0, 0), knee=(0.2, -1, 0)),
                fr=dict(at=(-0.08, -0.85, 0.72), rot=(-30, 0, 0), knee=(-0.1, -0.3, 1))),
         "snap"),
        (3, ctl(off=(0, -0.08, 0.20), twist=(16, 5, 7), lean=(26, 30),
                L=dict(at=(0.30, -0.06, -0.52), elbow=(0.8, 0.4, -0.4)),
                R=dict(at=(-0.36, 0.36, 0.04), elbow=(-0.6, 1, 0.3)),
                fl=dict(at=(0.14, 0.14, 0.78), rot=(45, 0, 0), knee=(0.2, -1, 0)),
                fr=dict(at=(-0.08, -0.80, 0.52), rot=(-25, 0, 0), knee=(-0.1, -0.3, 1)))),
        (5, ctl(off=(0, -0.10, -0.42), twist=(10, 3, 4), lean=(22, 24),
                L=dict(at=(0.28, -0.22, -0.62), elbow=(0.8, 0.3, -0.4)),
                R=dict(at=(-0.38, 0.30, -0.20), elbow=(-0.6, 1, 0.2)),
                fl=dict(at=(0.16, 0.26, 0.24), rot=(45, 0, 0), knee=(0.3, -1, -0.3)),
                fr=dict(at=(-0.10, -0.46, FLOOR), rot=(0, 0, -8), knee=(-0.2, -1, 0))),
         "in"),
        (8, ctl(off=(0, 0.0, -0.28), lean=(10, 12)), "smooth"),
        (11, G),
    ])

    # ---- Shift: the blink. Fold up small, then arrive mid-air, knee first ----
    fly = dict(L=dict(at=(0.38, 0.34, -0.36), elbow=(0.5, 1, 0)),
               R=dict(at=(-0.38, 0.34, -0.36), elbow=(-0.5, 1, 0)),
               fl=dict(at=(0.12, -0.28, 0.80), rot=(20, 0, 0), knee=(0, -1, 0.6)),
               fr=dict(at=(-0.14, 0.48, 0.62), rot=(65, 0, 0), knee=(-0.1, -1, -0.4)))
    clip("blink", 8, 24, [
        (0, G),
        (1, ctl(off=(0, 0.05, -0.42), twist=(-10, -3, -4), lean=(20, 22), nod=10,
                L=dict(at=(-0.06, -0.24, -0.30), elbow=(0.6, -0.2, -1)),
                R=dict(at=(0.06, -0.24, -0.38), elbow=(-0.6, -0.2, -1))), "out"),
        (3, ctl(off=(0, -0.06, 0.20), twist=(-8, -2, -4), lean=(12, 14), **fly), "snap"),
        (5, ctl(off=(0, -0.04, 0.16), twist=(-10, -3, -5), lean=(12, 14), **fly)),
        (7, G, "in"),
    ])

    # ---- getting hit, and going down ----------------------------------------
    g, low = guard(), tuple(G["offset"])
    hurt = with_(g, abdomen=(-8, 0, 6), chest=(-22, -6, 14), neck=(-12, 0, 6),
                 head=(-30, 0, 16), shoulder_L=(-24, -40, -6), elbow_L=(-60, 0, 0),
                 shoulder_R=(-20, 42, 8), elbow_R=(-70, 0, 0))
    stagger = with_(hurt, chest=(-30, -10, 24), head=(-20, 0, 20),
                    hip_L=(-30, -4, -8), knee_L=(40, 0, 0))
    kneel = with_(g, abdomen=(16, 0, 0), chest=(24, 0, 0), head=(30, 0, 0),
                  shoulder_L=(-10, -20, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-10, 20, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-70, 0, -6), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 0, 8), knee_R=(130, 0, 0), ankle_R=(-40, 0, 0))
    lying = with_(g, hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 8),
                  head=(-10, 0, 20), shoulder_L=(-150, 50, 0), elbow_L=(-20, 0, 0),
                  shoulder_R=(-40, -70, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-50, 0, -10), knee_L=(80, 0, 0), ankle_L=(-20, 0, 0),
                  hip_R=(-20, 0, 14), knee_R=(30, 0, 0), ankle_R=(-10, 0, 0))
    angles("hit", 6, 20, False, [
        Key(0.0, g, low), Key(0.22, hurt, (0, 0.13, -0.16), ease="out"),
        Key(0.55, with_(hurt, head=(-16, 0, 8)), (0, 0.11, -0.17)),
        Key(1.0, g, low)], overlap=0.025)
    angles("death", 12, 14, False, [
        Key(0.0, hurt, (0, 0.08, -0.16)),
        Key(0.2, stagger, (0, 0.18, -0.20), ease="out"),
        Key(0.45, kneel, (0, 0.14, -0.50)),
        Key(0.75, lying, (0, 0.10, 0.16), rot=(-88, 0, 0), ease="in"),
        Key(1.0, lying, (0, 0.10, 0.16), rot=(-90, 0, 0))], overlap=0.02)
    return out.data()
