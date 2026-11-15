"""Trial: Wade's body from his generated model, on his existing motion.

The body is art/models/Wade.glb (Meshy, from the A-pose concept art), on
the joints a human placed (art/models/Wade.fit.json). Nothing here changes
his proportions: the only scaling is the stretch on a punch's smear frames.

The motion is data (anim.py): controls, which say where the hips, hands and
feet are, and the clips that use them. The starting point is the shipped
Wade's motion (wade.py) solved for this body, from the guard a human posed
on it. Once a human has saved from the pose editor,
art/models/Wade.anim.json replaces the starting point.

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

import bpy

import _rig
import anim
import forge
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
    return m, guard()


_TABLE = None


def joint_table():
    """This body's joints as [(name, parent, rest position)], for the rig.
    The same positions build() gives the model."""
    global _TABLE
    if _TABLE is None:
        data = skin.prepare(os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW, log=lambda *a: None)
        bpy.data.objects.remove(data["obj"], do_unlink=True)
        _TABLE = [(n, p, tuple(float(c) for c in data["rest_pos"][n]))
                  for n, p in skin.HUMANOID]
    return _TABLE


def rig():
    return _rig.Rig(joint_table())


def current():
    """The saved animation if there is one, else the starting point."""
    data = anim.load(os.path.join(REPO, MODEL)) or animation()
    anim.check(data)
    return data


def clips():
    return anim.bake(current(), rig(), guard())


# ---------------------------------------------------------------------------
# The starting point
# ---------------------------------------------------------------------------

# The guard as a human posed it on this body in the pose editor.
GUARD = {
    "hips": (0, 0, 10), "abdomen": (13, -1.3, -3.2), "chest": (6.6, 1.5, -14.7),
    "neck": (7, 0, -2), "head": (8, 0, -2),
    "shoulder_L": (-75.8, -3.5, 8.3), "elbow_L": (-123.1, -0.5, 0),
    "wrist_L": (-6, 0, 0),
    "shoulder_R": (-78.6, 2.5, -1.9), "elbow_R": (-115.5, -0.3, -1),
    "wrist_R": (-6, 0, 0),
    "hip_L": (-24, -6, -6), "knee_L": (28, 0, 0), "ankle_L": (-6, 6, 0),
    "hip_R": (12, 6, 10), "knee_R": (32, 0, 0), "ankle_R": (-20, -6, 0),
    "hair": (0, 0, 0), "sash": (0, 0, 0),
}


def guard(bounce=0.0, roll=0.0):
    """The guard, with the idle's motion on top: `bounce` 0..1 sinks into
    the knees, `roll` -1..1 weaves the head and shoulders."""
    add = {
        "hips": (0, 0, 3 * roll), "abdomen": (2 * bounce, 0, 0),
        "chest": (3 * bounce, 0, -4 * roll), "head": (0, 0, 4 * roll),
        "shoulder_L": (-2 * roll, 0, 0), "shoulder_R": (2 * roll, 0, 0),
        "hip_L": (-6 * bounce, 0, 0), "knee_L": (14 * bounce, 0, 0),
        "ankle_L": (-8 * bounce, 0, 0),
        "hip_R": (-6 * bounce, 0, 0), "knee_R": (14 * bounce, 0, 0),
        "ankle_R": (-8 * bounce, 0, 0),
    }
    return {j: tuple(r[i] + add.get(j, (0, 0, 0))[i] for i in range(3)) + tuple(r[3:])
            for j, r in GUARD.items()}


class _Recorder(_rig.Rig):
    """A rig that keeps the control keys of every clip it is asked for."""

    def __init__(self, table):
        super().__init__(table)
        self.recorded = {}

    def clip(self, name, frames, fps, keys, base=None):
        self.recorded[name] = [(k[0], k[1], k[2] if len(k) > 2 else "smooth") for k in keys]
        return super().clip(name, frames, fps, keys, base)


def animation():
    """The shipped Wade's clips, solved for this body. wade.py authors its
    punches as controls built from its guard, so it is run here with this
    body's rig and guard in place of its own."""
    recorder = _Recorder(joint_table())
    theirs = wade.RIG, wade.guard
    wade.RIG, wade.guard = recorder, guard
    try:
        source = wade.clips()
        home = wade.stance()
    finally:
        wade.RIG, wade.guard = theirs
    base = guard()
    out = anim.Builder()
    # wade.py's punches start and end in its own stance, whose spine is not
    # quite the guard posed on this body. They start and end in that guard.
    posed = recorder.control(base, wade.LOW)
    out.control(posed, "guard")
    for c in source:
        if c.name in recorder.recorded:
            keys = [(f, posed if ctl == home else ctl, how, (0, 0, 0))
                    for f, ctl, how in recorder.recorded[c.name]]
        else:
            # Authored as joint angles: one control per frame reproduces it.
            keys = []
            for f in range(c.frames):
                t = f / c.frames if c.loop else f / max(1, c.frames - 1)
                pose, off, rot = forge.sample(c, t, base)
                keys.append((f, recorder.control(pose, off), "linear", rot))
            if c.loop:
                keys.append((c.frames, keys[0][1], "linear", keys[0][3]))
        out.clip(c.name, c.frames, c.fps, c.loop, c.overlap, keys)
    return out.data()
