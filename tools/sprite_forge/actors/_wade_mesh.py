"""Trial: Wade's body from his generated model, on his existing motion.

The body is art/models/Wade.glb (Meshy, from the A-pose concept art), on
the joints a human placed (art/models/Wade.fit.json). Nothing here changes
his proportions: the only scaling is the stretch on a punch's smear frames.

The motion is data (see anim.py): named poses and the clips that use them.
animation() below is the starting point, moved over from wade.py; once a
human has saved from the pose editor, art/models/Wade.anim.json replaces it.
This is a trial, not the shipped Wade: the leading underscore keeps
build.py from baking it over the game's sheet.

    <py> tools/sprite_forge/fit.py _wade_mesh out.png
    <py> tools/sprite_forge/preview.py _wade_mesh idle out.png 0,6
    <py> tools/sprite_forge/pose_editor.py _wade_mesh out.html

To ship it: fix what the previews show, then move build() and FIT into
wade.py (see .claude/skills/character-art/SKILL.md, step 3).
"""

import math
import os

import anim
from forge import Model
import skin
import wade

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Wade"
MODEL = "art/models/Wade.glb"
HEIGHT = 1.95
YAW = 0.0
STYLE = "ink"
RIM = wade.RIM
CELL = wade.CELL
FOLLOW = {}      # the sash is part of the mesh here, so nothing swings yet

# Saved from the joint editor (fit.py --editor) as art/models/Wade.fit.json.
# Until a human saves one this is empty and the automatic guess is used.
FIT = skin.load_fit(os.path.join(REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    # The clips key these two; on the mesh body they carry nothing.
    m.joint("hair", "head", m.rest_world("head"))
    m.joint("sash", "hips", m.rest_world("hips"))
    data = current()
    anim.check(data, m.joints)
    return m, anim.resolve(data["poses"], "guard")


def current():
    """The saved animation if there is one, else the starting point."""
    return anim.load(os.path.join(REPO, MODEL)) or animation()


def clips():
    return anim.to_clips(current())


# ---------------------------------------------------------------------------
# The starting point
# ---------------------------------------------------------------------------

def guard(bounce=0.0, roll=0.0):
    """Peek-a-boo (the Tyson guard): crouched forward from the waist, chin
    tucked behind the gloves, gloves up at the cheeks side by side, forearms
    near vertical, elbows pointing down and tight to the ribs, knees bent,
    feet bladed about shoulder width. `bounce` 0..1 sinks into the knees,
    `roll` -1..1 weaves the head.

    The arm angles are solved for this body, not guessed: the centre of each
    hand sits about 0.135 out from the nose, 0.17 in front of the head joint
    and 0.10 below it. Check a change with measure.py."""
    return {
        "hips": (0, 0, 10 + 3 * roll),
        "abdomen": (8 + 2 * bounce, 0, -3),
        "chest": (10 + 3 * bounce, 0, -4 - 4 * roll),
        "neck": (2, 0, -2),
        "head": (8, 0, -2 + 4 * roll),
        "shoulder_L": (-67 - 2 * roll, -2, -6), "elbow_L": (-106, 0, 0),
        "wrist_L": (-6, 0, 0),
        "shoulder_R": (-64 + 2 * roll, -1, 5), "elbow_R": (-109, 0, 0),
        "wrist_R": (-6, 0, 0),
        "hip_L": (-24 - 6 * bounce, -6, -6), "knee_L": (28 + 14 * bounce, 0, 0),
        "ankle_L": (-6 - 8 * bounce, 6, 0),
        "hip_R": (12 - 6 * bounce, 6, 10), "knee_R": (32 + 14 * bounce, 0, 0),
        "ankle_R": (-20 - 8 * bounce, -6, 0),
        "hair": (0, 0, 0), "sash": (0, 0, 0),
    }


def run_pose(p):
    s = math.sin(p * 2 * math.pi)

    def leg(sign):
        swing = -36 * s * sign - 4          # negative = forward
        fold = 12 + 78 * max(0.0, s * sign * -1)
        return (swing, 0, 0), (fold, 0, 0), (-8 - 10 * s * sign, 0, 0)

    hl, kl, al = leg(1)
    hr, kr, ar = leg(-1)
    return {
        "hips": (0, 0, 10 * s),
        "abdomen": (10, 0, -4 * s),
        "chest": (14, 0, -12 * s),
        "neck": (-2, 0, 4 * s),
        "head": (-4, 0, 6 * s),
        "shoulder_L": (-48 - 28 * s, -18, -8), "elbow_L": (-110, 0, 0),
        "wrist_L": (-10, 0, 0),
        "shoulder_R": (-48 + 28 * s, 18, 10), "elbow_R": (-120, 0, 0),
        "wrist_R": (-10, 0, 0),
        "hip_L": hl, "knee_L": kl, "ankle_L": al,
        "hip_R": hr, "knee_R": kr, "ankle_R": ar,
        "hair": (0, 0, 0), "sash": (0, 0, 0),
    }


def animation():
    poses, clips = {}, []
    g = guard()

    def pose(name, base, **joints):
        assert name not in poses, name
        poses[name] = {"base": base, "joints": {j: list(v) for j, v in joints.items()}}
        return name

    def mirrored(name, source):
        """Swap left / right: one authored punch serves both hands."""
        out = {}
        for joint, r in anim.resolve(poses, source).items():
            if joint.endswith("_L"):
                joint = joint[:-2] + "_R"
            elif joint.endswith("_R"):
                joint = joint[:-2] + "_L"
            out[joint] = (r[0], -r[1], -r[2]) + tuple(r[3:])
        return pose(name, None, **out)

    def key(t, name, offset=(0, 0, 0), rot=(0, 0, 0), ease="smooth", add=None):
        k = {"t": t, "pose": name, "offset": list(offset), "rot": list(rot), "ease": ease}
        if add:
            k["add"] = add
        return k

    def clip(name, frames, fps, loop, keys, overlap):
        clips.append({"name": name, "frames": frames, "fps": fps, "loop": loop,
                      "overlap": overlap, "keys": keys})

    low = (0, 0, -0.03)
    pose("guard", None, **g)

    # Idle: the boxer's bounce, two beats a second, shoulders rolling. Each
    # key is the guard plus the bounce, so editing the guard edits the idle.
    idle = []
    for f in range(13):
        t = f / 12
        bounce = 0.5 - 0.5 * math.cos(2 * 2 * math.pi * t)
        roll = math.sin(2 * math.pi * t)
        moved = guard(bounce, roll)
        add = {j: [round(moved[j][i] - g[j][i], 3) for i in range(3)]
               for j in g if any(abs(moved[j][i] - g[j][i]) > 1e-9 for i in range(3))}
        idle.append(key(t, "guard", (0, 0, -0.02 - 0.045 * bounce), ease="linear", add=add))
    clip("idle", 12, 12, True, idle, 0.035)

    run = []
    for f in range(11):
        p = f / 10
        s = math.sin(p * 2 * math.pi)
        # The cycle closes: the last key is the first pose again.
        name = "run 0" if f == 10 else pose(f"run {f}", None, **run_pose(p))
        run.append(key(p, name, (0, -0.03, -0.04 + 0.06 * (1 - abs(s))), ease="linear"))
    clip("run", 10, 15, True, run, 0.02)

    # ---- jab (lead hand) ----------------------------------------------------
    # Smear frames: the forearm lengthens and the fist swells, on the hit and
    # its hold only.
    pose("jab load", "guard", hips=(0, 0, 22), chest=(6, 0, 2),
         shoulder_L=(-34, -16, 14), elbow_L=(-111, 0, 0))
    pose("jab hit", "guard", hips=(0, 0, 4), abdomen=(8, 0, -10), chest=(12, 0, -26),
         head=(10, 0, 12), shoulder_L=(-92, -2, -14),
         elbow_L=(-4, 0, 0, 1.0, 1.0, 1.35), wrist_L=(0, 0, 0, 1.28, 1.28, 1.28),
         hip_L=(-26, -4, -8), knee_L=(14, 0, 0))
    pose("jab hold", "jab hit", elbow_L=(-6, 0, 0, 1.0, 1.0, 1.12),
         wrist_L=(0, 0, 0, 1.12, 1.12, 1.12))
    pose("jab back", "guard", shoulder_L=(-54, -12, 12), elbow_L=(-93, 0, 0))
    clip("jab", 8, 24, False, [
        key(0.0, "guard", low), key(0.14, "jab load", (0, 0.02, -0.04), ease="out"),
        key(0.32, "jab hit", (0, -0.12, -0.04), ease="out"),
        key(0.52, "jab hold", (0, -0.11, -0.04)),
        key(0.78, "jab back", (0, -0.04, -0.03)), key(1.0, "guard", low)], 0.008)

    # ---- cross (rear hand): the hips whip, the heel turns ---------------------
    # The torso turns toward his left (+z) to bring the right shoulder
    # through; the load winds the other way first.
    pose("cross load", "guard", hips=(0, 0, -4), abdomen=(4, 0, -8), chest=(6, 0, -12),
         shoulder_R=(-31, 11, -10), elbow_R=(-124, 0, 0), knee_R=(30, 0, 0))
    pose("cross hit", "guard", hips=(0, 0, 18), abdomen=(8, 0, 8), chest=(14, 0, 10),
         head=(8, 0, -10), shoulder_R=(-94, 4, 10),
         elbow_R=(-4, 0, 0, 1.0, 1.0, 1.42), wrist_R=(0, 0, 0, 1.32, 1.32, 1.32),
         shoulder_L=(-40, -20, 14), elbow_L=(-119, 0, 0),
         hip_R=(6, 4, -12), knee_R=(16, 0, 0), ankle_R=(-36, 0, 0),
         hip_L=(-24, -4, -8), knee_L=(22, 0, 0))
    pose("cross hold", "cross hit", elbow_R=(-6, 0, 0, 1.0, 1.0, 1.15),
         wrist_R=(0, 0, 0, 1.12, 1.12, 1.12))
    pose("cross back", "guard", chest=(10, 0, 10))
    clip("cross", 9, 24, False, [
        key(0.0, "guard", low), key(0.16, "cross load", (0, 0.03, -0.05), ease="out"),
        key(0.36, "cross hit", (0, -0.15, -0.05), ease="out"),
        key(0.56, "cross hold", (0, -0.14, -0.05)),
        key(0.8, "cross back", (0, -0.05, -0.03)), key(1.0, "guard", low)], 0.008)

    # ---- uppercut (rear hand): sink, then explode upward ---------------------
    pose("uppercut R dip", "guard", hips=(0, 0, -6), abdomen=(18, -6, -8),
         chest=(22, -10, -12), head=(10, 0, 6), shoulder_R=(-12, 10, 12),
         elbow_R=(-96, 0, 0), hip_L=(-34, -4, -8), knee_L=(56, 0, 0),
         ankle_L=(-18, 0, 0), hip_R=(-2, 4, 12), knee_R=(54, 0, 0), ankle_R=(-30, 0, 0))
    pose("uppercut R rise", "guard", hips=(0, 0, 16), abdomen=(-6, 6, 10),
         chest=(-16, 8, 12), head=(-14, 0, -6), shoulder_R=(-136, -4, 24),
         elbow_R=(-74, 0, 0, 1.0, 1.0, 1.28), wrist_R=(0, 0, 0, 1.3, 1.3, 1.3),
         hip_L=(-12, -4, -8), knee_L=(6, 0, 0),
         hip_R=(8, 4, 12), knee_R=(6, 0, 0), ankle_R=(-22, 0, 0))
    pose("uppercut R hold", "uppercut R rise", elbow_R=(-76, 0, 0, 1.0, 1.0, 1.1),
         wrist_R=(0, 0, 0, 1.12, 1.12, 1.12))
    for side, names in (("R", None), ("L", True)):
        if names:
            for part in ("dip", "rise", "hold"):
                mirrored(f"uppercut L {part}", f"uppercut R {part}")
        clip(f"uppercut_{side}", 10, 24, False, [
            key(0.0, "guard", low),
            key(0.24, f"uppercut {side} dip", (0, 0.0, -0.16), ease="out"),
            key(0.44, f"uppercut {side} rise", (0, -0.06, 0.06), ease="out"),
            key(0.62, f"uppercut {side} hold", (0, -0.06, 0.05)),
            key(1.0, "guard", low)], 0.01)

    # ---- slip: bob and weave off the line -------------------------------------
    pose("slip duck", "guard", abdomen=(14, 12, 0), chest=(30, 22, -18), head=(-8, 10, 0),
         hip_L=(-36, -4, -10), knee_L=(62, 0, 0), ankle_L=(-24, 0, 0),
         hip_R=(-4, 4, 14), knee_R=(58, 0, 0), ankle_R=(-44, 0, 0))
    clip("slip", 8, 20, False, [
        key(0.0, "guard", low), key(0.3, "slip duck", (0.10, 0.04, -0.17), ease="out"),
        key(0.7, "slip duck", (0.10, 0.04, -0.17)), key(1.0, "guard", low)], 0.015)

    # ---- roll -------------------------------------------------------------------
    pose("roll tuck", "guard", hips=(0, 0, 0), abdomen=(30, 0, 0), chest=(40, 0, 0),
         head=(30, 0, 0), shoulder_L=(-70, -20, 0), elbow_L=(-130, 0, 0),
         shoulder_R=(-70, 20, 0), elbow_R=(-130, 0, 0),
         hip_L=(-110, 0, 0), knee_L=(135, 0, 0), ankle_L=(-20, 0, 0),
         hip_R=(-104, 0, 0), knee_R=(130, 0, 0), ankle_R=(-20, 0, 0))
    pose("roll dive", "guard", chest=(30, 0, 0))
    roll, centre, rolling = [], 0.92, 0.58
    for f in range(11):
        t = f / 10
        a = math.radians(360.0 * t)
        tucked = 0.08 < t < 0.92
        target = rolling if tucked else centre - 0.05
        roll.append(key(t, "roll tuck" if tucked else "roll dive",
                        (0.0, centre * math.sin(a), target - centre * math.cos(a)),
                        (360.0 * t, 0, 0), "linear"))
    clip("roll", 10, 20, False, roll, 0.0)

    # ---- getting hit: the head snaps, the rest follows -------------------------
    pose("hurt", "guard", abdomen=(-8, 0, 6), chest=(-22, -6, 14), neck=(-12, 0, 6),
         head=(-30, 0, 16), shoulder_L=(-24, -40, -6), elbow_L=(-60, 0, 0),
         shoulder_R=(-20, 42, 8), elbow_R=(-70, 0, 0))
    pose("hurt settle", "hurt", head=(-16, 0, 8))
    clip("hit", 6, 20, False, [
        key(0.0, "guard", low), key(0.22, "hurt", (0, 0.09, -0.02), ease="out"),
        key(0.55, "hurt settle", (0, 0.07, -0.03)), key(1.0, "guard", low)], 0.025)

    pose("stagger", "hurt", chest=(-30, -10, 24), head=(-20, 0, 20),
         hip_L=(-30, -4, -8), knee_L=(40, 0, 0))
    pose("kneel", "guard", abdomen=(16, 0, 0), chest=(24, 0, 0), head=(30, 0, 0),
         shoulder_L=(-10, -20, 0), elbow_L=(-30, 0, 0),
         shoulder_R=(-10, 20, 0), elbow_R=(-30, 0, 0),
         hip_L=(-70, 0, -6), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
         hip_R=(-80, 0, 8), knee_R=(130, 0, 0), ankle_R=(-40, 0, 0))
    pose("lying", "guard", hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 8),
         head=(-10, 0, 20), shoulder_L=(-150, 50, 0), elbow_L=(-20, 0, 0),
         shoulder_R=(-40, -70, 0), elbow_R=(-30, 0, 0),
         hip_L=(-50, 0, -10), knee_L=(80, 0, 0), ankle_L=(-20, 0, 0),
         hip_R=(-20, 0, 14), knee_R=(30, 0, 0), ankle_R=(-10, 0, 0))
    clip("death", 12, 14, False, [
        key(0.0, "hurt", (0, 0.04, -0.02)),
        key(0.2, "stagger", (0, 0.14, -0.06), ease="out"),
        key(0.45, "kneel", (0, 0.10, -0.42)),
        key(0.75, "lying", (0, 0.10, 0.16), rot=(-88, 0, 0), ease="in"),
        key(1.0, "lying", (0, 0.10, 0.16), rot=(-90, 0, 0))], 0.02)

    return {"poses": poses, "clips": clips}
