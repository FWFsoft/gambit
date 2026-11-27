"""Shared pieces for the heroes whose bodies are generated models and whose
motion is still a first pass (MILES, Mina, Stitches, Eliana, Lilith,
Valthor): props on joints, and the clip shapes every hero needs.

Each hero's own file holds who they are: the idle stance, the run, and one
clip per ability, named as the game asks for them when it has no mapping
of its own (src/ArenaView.cpp, actionClip: "attack" LMB, "secondary" RMB,
"special" Q, "mobility" LSHIFT). Kits are in the Character Concepts doc.

Pose values are joint angles in degrees (pitch, roll, yaw), as everywhere
in the forge: a negative shoulder or hip pitch swings the limb forward, a
negative elbow bends it, a positive knee bends it, the spine leans forward
with positive pitch, and + yaw turns toward the character's left. On the
left arm a negative roll raises it out to the side; on the right, positive.
A fourth value scales the joint: 0 hides it and everything on it.
"""

import math
import os

import numpy as np

from forge import Clip, Key
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
SHOW, HIDE = 1.0, 0.0


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def _rotation(rx, ry, rz):
    """Degrees about x, then y, then z (world axes)."""
    a, b, c = (math.radians(d) for d in (rx, ry, rz))
    x = np.array([[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]])
    y = np.array([[math.cos(b), 0, math.sin(b)], [0, 1, 0], [-math.sin(b), 0, math.cos(b)]])
    z = np.array([[math.cos(c), -math.sin(c), 0], [math.sin(c), math.cos(c), 0], [0, 0, 1]])
    return z @ y @ x


def prop(model, path, joint, parent, at, size, turn=(0, 0, 0), material=None):
    """A second model file as a rigid part on a new joint.

    The joint is made under `parent` at `at` (world, rest pose). The prop's
    longest side is scaled to `size`, turned by `turn` (degrees about x, y,
    z) and centred on the joint, so a pose turns it about its middle."""
    material = material or joint
    obj, v = skin.load_mesh(os.path.join(REPO, path), 1.0)
    v = v - (v.max(0) + v.min(0)) / 2.0
    v = v * (size / float((v.max(0) - v.min(0)).max()))
    v = v @ _rotation(*turn).T
    model.joint(joint, parent, tuple(float(c) for c in at))
    rest = np.array(model.rest_world(joint))
    v = v + rest
    model.material(material, (200, 200, 200))
    model.materials[material].textured = True
    s = skin.Skin(obj, v, [joint], {joint: rest}, np.ones((len(v), 1)),
                  model.materials[material].bmat)
    s.obj.name = f"{model.name}_{material}"
    skin._set_coords(s.obj.data, v)
    model.skins.append(s)
    return s


def palm(model, side="R"):
    """Where an object held in the hand sits, in the rest pose: a little
    below the wrist, along the hanging arm."""
    w = np.array(model.rest_world(f"wrist_{side}"))
    e = np.array(model.rest_world(f"elbow_{side}"))
    return tuple(w + (w - e) * 0.35)


def back(model, depth, drop=0.0):
    """A point on the character's back at chest height, `depth` behind the
    chest joint (+y is backward) and `drop` below it."""
    c = np.array(model.rest_world("chest"))
    return tuple(c + np.array((0.0, depth, -drop)))


# ---- clip shapes -----------------------------------------------------------

def breathe(stand, k, frames=8, fps=8, sink=0.012):
    keys = []
    for f in range(frames + 1):
        b = 0.5 - 0.5 * math.cos(2 * math.pi * f / frames)
        keys.append(Key(f / frames, stand(b), (0, 0, -sink * k * b), ease="linear"))
    return Clip("idle", frames, fps, True, keys, overlap=0.03)


def stride(pose, k, name="run", frames=8, fps=12, bounce=0.04, low=0.0):
    keys = []
    for f in range(frames + 1):
        p = f / frames
        keys.append(Key(p, pose(p),
                        (0, 0, k * (-low + bounce * abs(math.cos(p * 2 * math.pi)))),
                        ease="linear"))
    return Clip(name, frames, fps, True, keys)


def shot(name, frames, fps, beats, overlap=0.012):
    """A one-shot clip from (t, pose, offset[, ease]) beats."""
    keys = [Key(b[0], b[1], b[2], ease=b[3] if len(b) > 3 else "smooth")
            for b in beats]
    return Clip(name, frames, fps, False, keys, overlap=overlap)


def run_pose(stand, lean=8, arm=34, leg=34, knee=60, elbow=-62, twist=8):
    """A plain hero run on top of `stand`: arms and legs in opposition."""
    def pose(phase):
        s = math.sin(phase * 2 * math.pi)
        base = stand(0.0)
        return with_(base, **{
            "hips": (0, 0, twist * s),
            "abdomen": (lean * 0.5, 0, -twist * 0.4 * s),
            "chest": (lean, 0, -twist * s),
            "shoulder_L": (-arm * s, base["shoulder_L"][1], 0) + tuple(base["shoulder_L"][3:]),
            "elbow_L": (elbow, 0, 0),
            "shoulder_R": (arm * s, base["shoulder_R"][1], 0) + tuple(base["shoulder_R"][3:]),
            "elbow_R": (elbow, 0, 0),
            "hip_L": (leg * s - 8, -4, 0), "knee_L": (12 + max(0.0, s) * knee, 0, 0),
            "ankle_L": (-8, 0, 0),
            "hip_R": (-leg * s - 8, 4, 0), "knee_R": (12 + max(0.0, -s) * knee, 0, 0),
            "ankle_R": (-8, 0, 0),
        })
    return pose


def hit_and_death(stand, k, name_hit="hit"):
    """The cast's shared reaction and fall: jolted back, then knees go and
    the body falls flat on its back (slid forward so it stays in the cell)."""
    s = stand(0.0)
    hurt = with_(s, abdomen=(-10, 0, 6), chest=(-18, 0, 10), neck=(-8, 0, 0),
                 head=(-20, 0, 12),
                 shoulder_L=(-24, -40, 0), elbow_L=(-50, 0, 0),
                 shoulder_R=(-20, 40, 0), elbow_R=(-50, 0, 0))
    buckle = with_(hurt, abdomen=(-18, 0, 0), chest=(-26, 0, 0), head=(-30, 0, 0),
                   hip_L=(-30, -12, 0), knee_L=(70, 0, 0),
                   hip_R=(-20, 12, 0), knee_R=(64, 0, 0))
    lying = with_(s, hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 0),
                  neck=(0, 0, 0), head=(-8, 0, 24),
                  shoulder_L=(-10, -40, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-6, 30, 0), elbow_R=(-64, 0, 0),
                  hip_L=(-8, -16, 0), knee_L=(10, 0, 0), ankle_L=(30, 0, 0),
                  hip_R=(-34, 20, 0), knee_R=(62, 0, 0), ankle_R=(30, 0, 0))
    o = lambda x, y, z: (x * k, y * k, z * k)  # noqa: E731
    return lay_down([
        shot(name_hit, 4, 16, [(0.0, s, o(0, 0, 0)),
                               (0.3, hurt, o(0, 0.07, -0.03), "out"),
                               (1.0, s, o(0, 0, 0))], overlap=0.0),
        shot("death", 9, 12, [(0.0, hurt, o(0, 0.05, -0.03)),
                              (0.35, buckle, o(0, 0.10, -0.22)),
                              (0.7, lying, o(0, -1.15, 0.20), "in"),
                              (1.0, lying, o(0, -1.15, 0.20))], overlap=0.0),
    ])


def lay_down(clips):
    """The fall's last two keys turn the root onto its back."""
    for c in clips:
        if c.name == "death":
            c.keys[2].rot = (-86, 0, 8)
            c.keys[3].rot = (-90, 0, 8)
    return clips
