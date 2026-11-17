"""Gravon, the Undying Blade: the host's body from his generated model, the
blade as a prop.

Gravon is the sword. The man is only its host: his body is
art/models/Gravon.glb (Meshy, from the A-pose concept art) on the joints a
human placed in the fit editor (art/models/Gravon.fit.json). The blade is
its own model, art/models/GravonBlade.glb, bound rigidly to two mounts, one
shown at a time: "back" wears it down his spine, "sword" puts it in his
right fist. In the game the blade alone (stuck in the floor where the host
fell, or on an ally's back) is drawn from its own sheet, GravonBlade.

Shape brief: the blade. At rest it runs straight down his spine, pommel
over his head, crossguard across his shoulders, so the whole silhouette
reads as a sword with a man hung on it. The host slumps beneath it, head
bowed, arms hanging: a vessel the blade is wearing. It is drawn only to
swing, and every swing is heavy: the body winds up, the blade arrives late
and stops hard.

Preview with preview.py gravon idle out.png 0,3,5.
"""

import math
import os

import numpy as np

from forge import Clip, Key, Model
import skin
from nyx import attach_prop

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Gravon"
MODEL = "art/models/Gravon.glb"
BLADE_MODEL = "art/models/GravonBlade.glb"
HEIGHT = 2.0                         # forge units, to the top of the hair
BLADE_HEIGHT = 2.4                   # pommel to point: taller than he is
YAW = 0.0
STYLE = "ink"
RIM = (176, 92, 255)                 # the violet of the blade's will
PIXEL_SCALE = 1.55                   # a big cell for a big blade: see the sheet size
# Room below the feet for the slam, whose blade lands far out toward the camera.
CELL = (320, 285, 178)
FOLLOW = {}

FIT = skin.load_fit(os.path.join(REPO, MODEL))

# The blade model stands point down, pommel up, its flat facing the camera.
# GRIP is how far up it the middle of the grip is (measured off its preview).
GRIP = 0.875
# On his back, as in the splash art: a diagonal from the pommel over his
# left shoulder to the point beside his right calf. The mount's joint sits
# at the blade's centre, so a pose turns the blade about its middle.
BACK_CENTRE = (-0.10, 0.21, 1.31)        # world, at rest
BACK_TILT = 30.0                         # degrees off vertical, pommel left
SHOW, HIDE = 1.0, 0.0


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    blade = os.path.join(REPO, BLADE_MODEL)
    m.joint("back", "chest", BACK_CENTRE)
    attach_prop(m, blade, "back", BLADE_HEIGHT, (0.0, 0.0, 0.0),
                material="blade")
    # The fist's mount pivots at the palm, so a pose aims the blade from
    # the grip.
    wrist = m.rest_world("wrist_R")
    hand = m.rest_world("hand_R") if "hand_R" in m.joints else wrist
    palm = tuple((w + h) / 2.0 for w, h in zip(wrist, hand))
    m.joint("sword", "wrist_R", palm)
    attach_prop(m, blade, "sword", BLADE_HEIGHT,
                (0.0, 0.0, -(GRIP - 0.5) * BLADE_HEIGHT),
                material="blade_hand")
    for name, at in REST.items():
        got = m.rest_world(name)
        assert max(abs(a - b) for a, b in zip(at, got)) < 2e-3, (
            f"{name} moved ({got} vs {at}): the fit changed; re-measure REST")
    return m, stance()


# ---------------------------------------------------------------------------
# Poses, as joint angles (degrees). x: his left; -y: forward; z: up.
# A hanging limb swings forward with negative x; elbows bend with negative x;
# the spine leans forward with positive x, and toward his left with positive
# y; z twists toward his left. On the right arm, +y raises it out to his
# right. (Measured with a probe of the fitted model, not assumed.)
#
# Poses with the blade in hand say where the hands go and where the blade
# points; two-bone IK solves the arms, so in a two-handed grip the left hand
# really is on the grip, in every pose and every direction.
# ---------------------------------------------------------------------------

# The fitted skeleton at rest (model space), for the IK. build() checks it
# against the model: if the fit changes, re-measure (probe rest_world).
REST = {
    "hips": (0.000, -0.038, 1.086), "abdomen": (0.000, -0.061, 1.257),
    "chest": (0.000, -0.080, 1.520), "neck": (0.000, 0.027, 1.665),
    "head": (0.000, -0.008, 1.864),
    "shoulder_L": (0.257, 0.043, 1.558), "elbow_L": (0.257, 0.043, 1.307),
    "wrist_L": (0.257, 0.043, 1.005),
    "shoulder_R": (-0.257, 0.043, 1.558), "elbow_R": (-0.257, 0.043, 1.307),
    "wrist_R": (-0.257, 0.043, 1.005),
}
PARENT = {"hips": None, "abdomen": "hips", "chest": "abdomen", "neck": "chest",
          "head": "neck", "shoulder_L": "chest", "elbow_L": "shoulder_L",
          "wrist_L": "elbow_L", "shoulder_R": "chest", "elbow_R": "shoulder_R",
          "wrist_R": "elbow_R"}
POMMEL_HAND = 0.15        # the left hand sits this far up the grip


def _rot(a):
    """Blender's XYZ Euler (degrees) as a matrix: X first, then Y, then Z."""
    x, y, z = (math.radians(v) for v in a[:3])
    cx, sx, cy, sy, cz, sz = (math.cos(x), math.sin(x), math.cos(y),
                              math.sin(y), math.cos(z), math.sin(z))
    rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return rz @ ry @ rx


def _euler(r):
    y = math.asin(max(-1.0, min(1.0, -r[2, 0])))
    x = math.atan2(r[2, 1], r[2, 2])
    z = math.atan2(r[1, 0], r[0, 0])
    return tuple(round(math.degrees(v), 2) for v in (x, y, z))


def _unit(v):
    v = np.array(v, dtype=float)
    return v / np.linalg.norm(v)


def fk(pose, name):
    """(rotation, position) of a joint in model space."""
    chain = []
    j = name
    while j is not None:
        chain.append(j)
        j = PARENT[j]
    r, p = np.eye(3), np.array(REST[chain[-1]])
    for k, j in enumerate(reversed(chain)):
        if k > 0:
            parent = chain[len(chain) - k]
            p = p + r @ (np.array(REST[j]) - np.array(REST[parent]))
        r = r @ _rot(pose.get(j, (0, 0, 0)))
    return r, p


def ik_arm(pose, side, target, pole):
    """Set shoulder and elbow so the wrist reaches `target` (model space),
    the elbow toward `pole`. Out of reach: the arm points at it, straight."""
    sh, el, wr = f"shoulder_{side}", f"elbow_{side}", f"wrist_{side}"
    l1 = np.linalg.norm(np.array(REST[el]) - np.array(REST[sh]))
    l2 = np.linalg.norm(np.array(REST[wr]) - np.array(REST[el]))
    r_chest, _ = fk(pose, "chest")
    _, p_sh = fk(dict(pose, **{sh: (0, 0, 0)}), sh)
    t = r_chest.T @ (np.array(target, dtype=float) - p_sh)
    d = float(np.clip(np.linalg.norm(t), 0.08, l1 + l2 - 1e-3))
    inner = math.acos(max(-1.0, min(1.0, (l1 * l1 + l2 * l2 - d * d) / (2 * l1 * l2))))
    theta = -(math.pi - inner)                    # elbow flex (negative x)
    w_local = np.array([0.0, l2 * math.sin(theta), -l1 - l2 * math.cos(theta)])
    a = _unit(w_local)
    e = _unit(np.array([0.0, 0.0, -1.0]) - a * float(a @ np.array([0.0, 0.0, -1.0])))
    b = _unit(t)
    pc = r_chest.T @ np.array(pole, dtype=float)
    q = _unit(pc - b * float(pc @ b))
    src = np.column_stack([a, e, np.cross(a, e)])
    dst = np.column_stack([b, q, np.cross(b, q)])
    p = dict(pose)
    p[sh] = _euler(dst @ src.T)
    p[el] = (round(math.degrees(theta), 2), 0, 0)
    p[wr] = (0, 0, 0)
    return p


def aim_sword(pose, point, flat, root_rot=(0, 0, 0)):
    """The blade in his right fist toward `point` (world, after the root's
    turn), its flat facing `flat`. Hides the back mount."""
    rr = _rot(root_rot)
    d = _unit(rr.T @ _unit(point))
    n = _unit(rr.T @ _unit(flat))
    n = _unit(n - d * float(n @ d))
    want = np.column_stack([np.cross(n, d), -n, -d])
    r_wrist, _ = fk(pose, "wrist_R")
    p = dict(pose)
    p["sword"] = _euler(r_wrist.T @ want) + (SHOW,)
    p["back"] = (0, 0, 0, HIDE)
    return p, d


def grip(pose, hand, point, flat=(0, 0, 1), two=True, root_rot=(0, 0, 0),
         pole_r=(-1.0, 0.4, -0.6), pole_l=(1.0, 0.4, -0.6)):
    """The blade in hand: right wrist at `hand` (model space; pulled in to
    arm's length if it is out of reach), the blade toward `point`, and with
    `two` the left hand on the grip above where the right hand really is."""
    p = ik_arm(pose, "R", hand, pole_r)
    p, d = aim_sword(p, point, flat, root_rot)
    if two:
        _, wrist = fk(p, "wrist_R")
        p = ik_arm(p, "L", wrist - d * POMMEL_HAND, pole_l)
    return p


def reach_error(pose):
    """How far the left hand is from the grip (0 for a one-handed pose)."""
    if pose.get("sword", (0, 0, 0, 0))[3] == HIDE:
        return 0.0
    r, wrist = fk(pose, "wrist_R")
    d = r @ _rot(pose["sword"]) @ np.array([0.0, 0.0, -1.0])
    _, left = fk(pose, "wrist_L")
    return float(np.linalg.norm(left - (wrist - d * POMMEL_HAND)))


def swing_dir(az, drop=-0.08):
    """A horizontal direction `az` degrees round from straight ahead
    (positive: toward his left), dipping by `drop`."""
    a = math.radians(az)
    return (math.sin(a), -math.cos(a), drop)


def sheathed(sway=0.0):
    return {"back": (-sway, BACK_TILT, 0, SHOW), "sword": (0, 0, 0, HIDE)}


def with_(base, *more, **changes):
    pose = dict(base)
    for extra in more:
        pose.update(extra)
    pose.update(changes)
    return pose


def stance(breath=0.0):
    """The host at rest, the blade on his back: a power stance. Feet wide,
    knees bent, weight low and even, chest up under the weight on his back,
    fists heavy at his sides. Only his breathing moves. The head is bowed a
    little: the violet face is how you know him."""
    pose = {
        "hips": (2, 0, 0),
        "abdomen": (4 - 1.5 * breath, 0, 0),
        "chest": (4 - 2.5 * breath, 0, 0),
        "neck": (4, 0, 0),
        "head": (6 + 1.0 * breath, 0, 0),
        "shoulder_L": (4, -10 - 2 * breath, 0), "elbow_L": (-24, 0, 0),
        "wrist_L": (0, 0, 0),
        "shoulder_R": (4, 10 + 2 * breath, 0), "elbow_R": (-24, 0, 0),
        "wrist_R": (0, 0, 0),
        "hip_L": (-14, -14, -10), "knee_L": (26, 0, 0), "ankle_L": (-12, 14, 0),
        "hip_R": (-14, 14, 10), "knee_R": (26, 0, 0), "ankle_R": (-12, -14, 0),
    }
    pose.update(sheathed(0.0))
    return pose


def run_pose(p):
    """Running with the blade on his back: the same hard, leaning run as
    the drag, both arms pumping clear of his body. The model's feet point
    out (a good stance standing); running, the legs turn in at the hip so
    the toes point where he is going."""
    s = math.sin(p * 2 * math.pi)
    c = math.cos(p * 2 * math.pi)
    pose = stance()
    pose.update({
        "hips": (14, 0, 8 * s), "abdomen": (14, 0, -4 * s),
        "chest": (16, 0, -8 * s), "neck": (-16, 0, 4 * s),
        "head": (-10, 0, 4 * s),
        "shoulder_L": (44 * s - 8, -16, 0), "elbow_L": (-64 + 18 * s, 0, 0),
        "shoulder_R": (-44 * s - 8, 16, 0), "elbow_R": (-64 - 18 * s, 0, 0),
        "hip_L": (-40 * s - 16, -2, -TOE_IN), "knee_L": (28 + 64 * max(0.0, c), 0, 0),
        "ankle_L": (-10 + 14 * s, 0, 0),
        "hip_R": (40 * s - 16, 2, TOE_IN), "knee_R": (28 + 64 * max(0.0, -c), 0, 0),
        "ankle_R": (-10 - 14 * s, 0, 0),
    })
    pose.update(sheathed(0.0))
    return pose


# The model's feet turn out about 17 degrees; running, the legs turn in by
# this much at the hip so the toes point the way he runs.
TOE_IN = 18.0


# ---- the blade out -----------------------------------------------------------
# Feet wide, knees bent, the weight low: everything he does with the blade
# out is braced against it.

LEG_BRACED = {"hip_L": (-24, -14, -8), "knee_L": (46, 0, 0),
              "ankle_L": (-20, 14, 0), "hip_R": (-18, 14, 8),
              "knee_R": (44, 0, 0), "ankle_R": (-24, -14, 0)}
BRACED_ROOT = (0, 0, -0.13)
FLOOR = 0.13              # model-space height of the floor at BRACED_ROOT


def braced(lean=10.0, twist=0.0, side=0.0):
    pose = with_(stance(), LEG_BRACED,
                 hips=(4, side * 0.3, twist * 0.25),
                 abdomen=(lean * 0.5, side * 0.4, twist * 0.3),
                 chest=(lean, side, twist * 0.45),
                 neck=(-lean * 0.4, 0, -twist * 0.3),
                 head=(-lean * 0.3, 0, -twist * 0.25))
    return pose


def tip_down(hand, ahead_az, reach=GRIP * BLADE_HEIGHT, floor=FLOOR):
    """The direction that puts the point on the floor from `hand`,
    `ahead_az` round from straight ahead."""
    drop = floor - hand[2]
    flat = math.sqrt(max(0.0, reach * reach - drop * drop))
    a = math.radians(ahead_az)
    return (math.sin(a) * flat, -math.cos(a) * flat, drop)


GUARD_HAND = (-0.16, -0.36, 0.98)


def guard(breath=0.0, shift=0.0):
    """The blade out, at rest: the same power stance, the blade standing
    point-down in the floor beside him, leaning out to his right front, his
    right hand resting on its grip. Only his breathing moves."""
    del shift
    body = with_(stance(breath), sheathed(0.0))
    body.update(shoulder_L=(4, -12 - 2 * breath, 0), elbow_L=(-30, 0, 0))
    hand = (-0.36, -0.20, 1.06 - 0.01 * breath)
    return grip(body, hand, tip_down(hand, -42, floor=0.06),
                flat=(0.8, -0.6, 0), two=False, pole_r=(-1, 0.3, -0.5))


def drag(p):
    """Running with the blade out: bent hard into it, hauling. The dragging
    arm is pulled straight back by the weight, the shoulder dragged down and
    back, the point scraping along the floor behind him on his right; the
    free arm pumps to keep him going."""
    s = math.sin(p * 2 * math.pi)
    c = math.cos(p * 2 * math.pi)
    pose = with_(stance(),
                 hips=(16, 0, 8 * s - 14), abdomen=(18, -4, -4 * s - 8),
                 chest=(22, -8, -6 * s - 16), neck=(-18, 0, 12),
                 head=(-14, 0, 14 + 3 * s),
                 shoulder_L=(46 * s - 10, -16, 0), elbow_L=(-70 + 20 * s, 0, 0),
                 hip_L=(-40 * s - 18, -4, -TOE_IN),
                 knee_L=(30 + 64 * max(0.0, c), 0, 0),
                 ankle_L=(-10 + 14 * s, 0, 0),
                 hip_R=(40 * s - 18, 4, TOE_IN),
                 knee_R=(30 + 64 * max(0.0, -c), 0, 0),
                 ankle_R=(-10 - 14 * s, 0, 0))
    hand = (-0.46, 0.42 + 0.03 * s, 0.80)
    return grip(pose, hand, tip_down(hand, 158, floor=0.08), flat=(0.9, 0.3, 0),
                two=False, pole_r=(-0.6, 0.6, -0.5))


DRAG_ROOT = (0, 0, -0.06)


# ---- Titan's Slash: one flat half-circle, the blade parallel to the floor ----

def swing(az, body_az=None, two=True, height=1.10, coil=0.0, drive=0.0,
          drop_left=0.0, reach=0.74, pull=0.0):
    """Mid-swing with the blade `az` degrees round (positive: his left), flat
    to the floor. The body moves it, from the ground up: `coil` sinks him
    onto his back leg; `drive` is the back leg straightening and pushing
    off, the front knee taking the weight deep, the hips turned through.
    `body_az` is where hips and chest have turned to, ahead of the blade on
    the way through and dragged past it after. `pull` (-1..1, the way the
    blade is going) crunches his side that way: the lats hauling it round
    and down. The arms are stretched out to their length by the blade."""
    b = max(-100.0, min(100.0, az if body_az is None else body_az))
    lean = 10 + 22 * drive + 4 * coil
    # Lean away from the blade against its weight; crunch into the pull.
    side = (-math.sin(math.radians(az)) * (6 + 6 * coil) + 16 * pull * drive
            + drop_left)
    pose = with_(stance(),
                 hips=(8 + 10 * drive, side * 0.25, b * 0.38),
                 abdomen=(lean * 0.45, side * 0.4, b * 0.26),
                 chest=(lean, side, b * 0.5),
                 neck=(-lean * 0.55, 0, -b * 0.38),
                 head=(-lean * 0.35, 0, -b * 0.3),
                 hip_L=(-24 - 40 * drive - 18 * coil, -14, -8),
                 knee_L=(46 + 40 * drive + 28 * coil, 0, 0),
                 ankle_L=(-20 - 8 * drive - 12 * coil, 14, 0),
                 hip_R=(-18 + 42 * drive - 20 * coil, 14, 8),
                 knee_R=(44 - 40 * drive + 30 * coil, 0, 0),
                 ankle_R=(-24 - 18 * drive - 10 * coil, -14, 0))
    a = math.radians(az * 0.9)
    hand = (-0.06 + reach * math.sin(a), -0.10 - reach * math.cos(a), height)
    p = grip(pose, hand, swing_dir(az), flat=(0, 0, 1), two=two)
    if not two:
        # The free arm flung out behind, against the weight.
        p = ik_arm(p, "L", (0.60 - 0.35 * math.sin(a), 0.15 + 0.25 * math.cos(a),
                            1.30), (1, 0.4, -0.6))
    return p


def swing_root(coil=0.0, drive=0.0):
    return (0, 0.04 * coil - 0.14 * drive, -0.13 - 0.12 * coil - 0.10 * drive)


# ---- drawing the blade off his back and putting it back -------------------------
# The grip is far above his reach when the blade is on his back, so the swap
# happens with his right hand up by his left ear and the blade, now in his
# fist, hanging behind him on nearly the same diagonal.

def reach(drop_left=0.0, sink=0.0):
    """Right hand up across to his left ear, about to take the blade; with
    `drop_left`, the left shoulder dipped to bring the hilt down to it, and
    with `sink` the knees giving as he takes its weight."""
    return with_(stance(), chest=(4 + 14 * sink, drop_left, 12),
                 abdomen=(4 + 8 * sink, drop_left * 0.5, 6), head=(0, 0, -8),
                 shoulder_R=(-150, -34, 0), elbow_R=(-110, 0, 0),
                 wrist_R=(0, 0, 0),
                 shoulder_L=(6, -10 - drop_left, 0), elbow_L=(-20, 0, 0),
                 hip_L=(-10 - 30 * sink, -8, -6), knee_L=(14 + 50 * sink, 0, 0),
                 ankle_L=(-6 - 22 * sink, 8, 0),
                 hip_R=(-4 - 26 * sink, 8, 6), knee_R=(14 + 46 * sink, 0, 0),
                 ankle_R=(-10 - 20 * sink, -8, 0))


def drawn_over_shoulder(drop_left=0.0, sink=0.0):
    p, _ = aim_sword(reach(drop_left, sink), (-0.45, 0.35, -0.82), (0, 1, 0))
    return p


def lifted():
    """Both hands raising it up across his body on the way to his back."""
    body = braced(lean=0, twist=14)
    hand = (0.05, -0.30, 1.42)
    return grip(body, hand, (-0.35, 0.10, 0.93), flat=(0, -1, 0))


# ---- Ravager's Edge -----------------------------------------------------------

LEG_WIDE = {"hip_L": (-22, -16, -6), "knee_L": (44, 0, 0),
            "ankle_L": (-20, 10, 0), "hip_R": (-22, 16, 6),
            "knee_R": (44, 0, 0), "ankle_R": (-20, -10, 0)}
CHARGE_ROOT = (0, 0.04, -0.17)


def ravager_hold(strain=0.0):
    """Coiled, rooted: both hands over his right shoulder, the blade laid
    back behind him, the body wound down and back."""
    body = with_(stance(), LEG_WIDE, hips=(-4, 0, -22), abdomen=(-6, 0, -12),
                 chest=(-12 - 4 * strain, 0, -24), neck=(4, 0, 10),
                 head=(4 + 6 * strain, 0, 14))
    return grip(body, (-0.30, 0.02, 1.72 - 0.02 * strain), (-0.25, 0.72, -0.64),
                flat=(1, 0, 0.2), pole_r=(-1, 0.2, -0.3), pole_l=(1, 0.0, -0.8))


def ravager_over():
    """The throw starts in the upper back: arched hard, arms high, the blade
    straight up and starting to tip forward over his head, heels coming up."""
    body = with_(stance(), LEG_WIDE, hips=(-6, 0, -4), abdomen=(-12, 0, -2),
                 chest=(-22, 0, -4), neck=(-10, 0, 0), head=(-12, 0, 0),
                 ankle_L=(10, 10, 0), ankle_R=(10, -10, 0))
    return grip(body, (-0.05, -0.05, 2.24), (0.0, -0.25, 0.97), flat=(1, 0, 0),
                pole_r=(-1, 0, 0), pole_l=(1, 0, 0))


def hold_at(body, root, rot, hands, point, flat=(1, 0, 0)):
    """Both hands on a grip that is at `hands` (world), the blade toward
    `point` (world), the root at `root` turned by `rot`."""
    r = _rot(rot)
    hand = r.T @ (np.array(hands) - np.array(root))
    return grip(body, hand, point, flat=flat, root_rot=rot,
                pole_r=(-1, 0, -0.4), pole_l=(1, 0, -0.4))


def tipped(pitch, leg_up, plant_bend=24.0):
    """Pitched forward over a planted left foot: the torso goes down by
    `pitch` at the hips while the left leg stays upright under him, and the
    right leg swings up behind by `leg_up` (degrees past in line with his
    back) to balance the blade's pull."""
    return with_(stance(), sheathed(0.0),
                 hips=(pitch, 0, 0), abdomen=(10, 0, 0), chest=(12, 0, 0),
                 neck=(-30, 0, 0), head=(-22, 0, 0),
                 hip_L=(-pitch - plant_bend * 0.5, -6, -4),
                 knee_L=(plant_bend, 0, 0), ankle_L=(-plant_bend * 0.5, 6, 0),
                 hip_R=(leg_up - pitch * 0.15, 8, 6), knee_R=(14, 0, 0),
                 ankle_R=(28, 0, 0))


# Where the blade lands: flat out in front of him, slammed along the floor
# by its own weight, and where his hands stay on it until it comes out.
SLAM_ROOT = (0, -0.02, -0.10)
SLAM_HANDS = (-0.04, -0.92, 0.60)
SLAM_POINT = tip_down(SLAM_HANDS, 0, floor=0.05)


def ravager_cross():
    """The throw has carried it over and it is coming down in front of him;
    its weight starts to take him with it: he tips over his left foot, the
    right leg coming up."""
    body = tipped(30, 20, plant_bend=16)
    return hold_at(body, (0, 0, -0.06), (0, 0, 0), (-0.05, -0.62, 1.42),
                   (0.0, -0.80, 0.18))


def ravager_impact(sag=0.0):
    """It has hit, and it has him: bent over his planted left foot, head
    below his waist, the right leg high behind, both hands still on it."""
    body = tipped(76 + 4 * sag, 56 - 6 * sag)
    root = (SLAM_ROOT[0], SLAM_ROOT[1], SLAM_ROOT[2] - 0.01 * sag)
    return hold_at(body, root, (0, 0, 0), SLAM_HANDS, SLAM_POINT), root


def ravager_recover():
    """The right foot back down, crouched wide over it, still bent to it."""
    body = with_(braced(lean=40), LEG_WIDE, hips=(24, 0, 0),
                 hip_L=(-46, -16, -6), knee_L=(70, 0, 0), ankle_L=(-24, 10, 0),
                 hip_R=(-46, 16, 6), knee_R=(70, 0, 0), ankle_R=(-24, -10, 0))
    root = (0, 0.02, -0.30)
    return hold_at(body, root, (0, 0, 0), SLAM_HANDS, SLAM_POINT), root


def ravager_heave():
    """Leaning back with his whole weight, legs driving, hauling it up out
    of the floor: the grip has come back and up toward him."""
    body = braced(lean=-22)
    body.update(LEG_WIDE)
    body.update(hip_L=(-40, -16, -6), knee_L=(62, 0, 0))
    root = (0, 0.04, -0.20)
    hands = (-0.05, -0.42, 1.20)
    return hold_at(body, root, (0, 0, 0), hands, (0.0, -0.70, -0.71)), root


def on_shoulder():
    """Up and onto his right shoulder: both hands on the grip in front of
    it, the blade lying back over the shoulder."""
    body = braced(lean=4, twist=-8)
    return grip(body, (-0.18, -0.30, 1.45), (-0.25, 0.82, 0.52),
                flat=(1, 0, 0), pole_r=(-1, 0, -0.5), pole_l=(1, -0.2, -0.6))


# ---- Fury of the Blade: the blade across his shoulders, a roar -----------------

def fury(roar=1.0):
    body = with_(braced(lean=-6 * roar), chest=(-14 * roar, 0, 0),
                 neck=(-10 * roar, 0, 0), head=(-24 * roar, 0, 0))
    hand = (-0.40, 0.08, 1.62)
    d = _unit((1.0, 0.22, 0.04))
    p = ik_arm(body, "R", hand, (-1, 0, -0.5))
    p, d = aim_sword(p, d, (0, 0.3, 1))
    return ik_arm(p, "L", np.array(hand) + d * 0.80, (1, 0, -0.5))


# ---- Blade Rush: slide, then stab ------------------------------------------------

def slide():
    """Sliding in low, both hands on the grip at his right hip, the blade
    trailing behind him, point down."""
    body = with_(stance(), hips=(14, 0, -16), abdomen=(18, 0, -10),
                 chest=(24, 0, -14), neck=(-18, 0, 8), head=(-20, 0, 8),
                 hip_L=(-74, -6, -4), knee_L=(96, 0, 0), ankle_L=(-24, 0, 0),
                 hip_R=(34, 8, 6), knee_R=(12, 0, 0), ankle_R=(-30, 0, 0))
    hand = (-0.28, 0.06, 0.82)
    return grip(body, hand, tip_down(hand, 160, floor=-0.24), flat=(0.9, 0.3, 0),
                pole_r=(-0.8, 0.6, -0.5), pole_l=(1, 0.2, -0.7))


SLIDE_ROOT = (0, -0.05, -0.30)


def chamber():
    """The beat before the stab: sunk into a low, wide stance, the blade
    raised level with his shoulders, both hands by his right cheek, the
    point aimed straight ahead along it. The back and legs are loaded."""
    body = with_(braced(lean=4, twist=-22),
                 neck=(-4, 0, 14), head=(-2, 0, 12),
                 hip_L=(-52, -14, -8), knee_L=(84, 0, 0), ankle_L=(-26, 14, 0),
                 hip_R=(-42, 14, 8), knee_R=(88, 0, 0), ankle_R=(-38, -14, 0))
    return grip(body, (-0.24, -0.10, 1.52), (0.04, -1.0, 0.02), flat=(0, 0, 1),
                pole_r=(-1, 0.2, -0.6), pole_l=(1, 0.2, -0.8))


CHAMBER_ROOT = (0, 0.06, -0.34)


def stab():
    """Driven through by the legs from the low stance: the back leg
    straightening and pushing, the front knee deep, the body turning square
    behind it, both arms shooting out at shoulder height and locking, the
    blade level the whole way."""
    body = with_(braced(lean=20, twist=4), hips=(14, 0, 2),
                 abdomen=(10, 0, 2), chest=(12, 0, 2),
                 neck=(-22, 0, -2), head=(-16, 0, -2),
                 hip_L=(-80, -6, -4), knee_L=(90, 0, 0), ankle_L=(-12, 0, 0),
                 hip_R=(42, 8, 6), knee_R=(2, 0, 0), ankle_R=(-36, 0, 0))
    return grip(body, (-0.05, -0.84, 1.38), (0.0, -1.0, 0.0), flat=(0, 0, 1),
                pole_r=(-1, 0, -0.6), pole_l=(1, 0, -0.6))


# ---- Getting hit, falling, rising ---------------------------------------------------

def hurt():
    return with_(stance(), abdomen=(-8, 0, 6), chest=(-16, -4, 10),
                 neck=(-12, 0, 0), head=(-20, 0, 12),
                 shoulder_L=(-24, 36, 0), elbow_L=(-50, 0, 0),
                 shoulder_R=(-10, 30, 0), elbow_R=(-30, 0, 0))


def hurt_drawn():
    body = with_(braced(lean=-10, twist=-6), head=(-18, 0, 10))
    hand = (GUARD_HAND[0], GUARD_HAND[1] + 0.08, GUARD_HAND[2] + 0.04)
    return grip(body, hand, tip_down(hand, -28), flat=(0.85, -0.5, 0))


def kneel(arms_down=1.0):
    return with_(stance(), hips=(10, 0, 0), abdomen=(16, 0, 0), chest=(24, 0, 0),
                 neck=(20, 0, 0), head=(14, 0, 0),
                 shoulder_L=(-20 * arms_down, 10, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(-20 * arms_down, 10, 0), elbow_R=(-20, 0, 0),
                 hip_L=(-84, -6, 0), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                 hip_R=(-10, 6, 0), knee_R=(118, 0, 0), ankle_R=(-30, 0, 0))


KNEEL_ROOT = (0, 0.02, -0.46)


def lying():
    return with_(stance(), hips=(0, 0, 0), abdomen=(0, 0, 0), chest=(0, 0, 8),
                 neck=(0, 0, 0), head=(-10, 0, 24),
                 shoulder_L=(-150, 50, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(-30, 70, 0), elbow_R=(-30, 0, 0),
                 hip_L=(-30, -8, 0), knee_L=(40, 0, 0),
                 hip_R=(-8, 8, 0), knee_R=(20, 0, 0))


def bare(pose):
    """No blade on him: it lies where he fell, drawn from its own sheet."""
    return with_(pose, back=(0, 0, 0, HIDE), sword=(0, 0, 0, HIDE))


def risen_grip():
    """Rising: kneeling, his right hand on the grip of the blade leaning in
    the floor in front of him."""
    body = with_(kneel(0.0), chest=(10, 0, 0), neck=(4, 0, 0), head=(0, 0, 0))
    hand = (-0.20, -0.45, 1.05)
    return grip(body, hand, tip_down(hand, -5, floor=0.46), flat=(1, 0, 0),
                two=False)


# ---------------------------------------------------------------------------
# Clips. Every clip with the blade out ends in the guard, the sword-out idle;
# the server keeps the blade out until he has not attacked for a while, and
# the game plays "sheathe" then. Timings match the server (GravonKit).
# ---------------------------------------------------------------------------

def clips():
    base = stance()
    low = (0, 0, 0)
    g = guard()
    out = []

    def swap(t, before, after, offset, ease="smooth"):
        """Change mounts between two frames: no frame shows both blades."""
        return [Key(t, before, offset, ease=ease),
                Key(t + 0.001, after, offset, ease="linear")]

    def along(t0, t1, az0, az1, steps, root, **kw):
        """Keys through a swing, small steps so the blade sweeps the arc."""
        keys = []
        for i in range(1, steps + 1):
            u = i / steps
            keys.append(Key(t0 + (t1 - t0) * u, swing(az0 + (az1 - az0) * u, **kw),
                            root, ease="linear"))
        return keys

    # Sheathed: slumped under it, and a slow, heavy trudge.
    idle = [Key(f / 6, stance(0.5 - 0.5 * math.cos(2 * math.pi * f / 6)),
                (0, 0, -0.06 - 0.006 * (0.5 - 0.5 * math.cos(2 * math.pi * f / 6))),
                ease="linear") for f in range(7)]
    out.append(Clip("idle", 6, 5, True, idle))
    run = []
    for f in range(7):
        p = f / 6
        s = abs(math.sin(p * 2 * math.pi))
        # Each stride drives down hard and pushes off.
        run.append(Key(p, run_pose(p), (0, -0.04, -0.12 + 0.07 * s ** 0.6),
                       ease="linear"))
    out.append(Clip("run", 6, 9, True, run, overlap=0.02))

    # Out: the guard, heaving and rocking under the weight, and the drag.
    gi = [Key(f / 6, guard(0.5 - 0.5 * math.cos(2 * math.pi * f / 6)),
              (0, 0, -0.06 - 0.006 * (0.5 - 0.5 * math.cos(2 * math.pi * f / 6))),
              ease="linear") for f in range(7)]
    out.append(Clip("guard", 6, 5, True, gi))
    dr = []
    for f in range(7):
        p = f / 6
        s = abs(math.sin(p * 2 * math.pi))
        # Each stride drives down hard and pushes off.
        dr.append(Key(p, drag(p), (0, -0.04, -0.13 + 0.07 * s ** 0.6),
                      ease="linear"))
    out.append(Clip("drag_run", 6, 9, True, dr))

    # Putting it away: both hands lift it across, the right hand takes it
    # over his left shoulder, onto his back.
    out.append(Clip("sheathe", 6, 9, False, [
        Key(0.0, g, BRACED_ROOT),
        Key(0.30, lifted(), (0, 0, -0.06), ease="out"),
        *swap(0.58, drawn_over_shoulder(0, 0.4), reach(0, 0.4), (0, 0, -0.08)),
        Key(0.80, with_(base, chest=(14, 0, 0)), (0, 0, -0.05), ease="out"),
        Key(1.0, base, low)]))

    def through(t0, t1, frames, az, body, coil, drive, height, two=True,
                drop_left=(0, 0)):
        pull = 1.0 if az[1] > az[0] else -1.0
        """Keys along a swing, one on every frame time of a `frames`-frame
        clip between t0 and t1 (and at t1): every frame is a solved pose,
        never a blend of two blade angles. Each argument but `two` is a
        (from, to)."""
        times = sorted({k / (frames - 1) for k in range(frames)
                        if t0 + 1e-6 < k / (frames - 1) < t1 - 1e-6} | {t1})
        keys = []
        for t in times:
            u = (t - t0) / (t1 - t0)

            def at(v):
                return v[0] + (v[1] - v[0]) * u
            keys.append(Key(t, swing(at(az), at(body), two=two,
                                     height=at(height), coil=at(coil),
                                     drive=at(drive), drop_left=at(drop_left),
                                     pull=pull),
                            swing_root(at(coil), at(drive)), ease="linear"))
        return keys

    # Titan's Slash from the back. He drops his left shoulder and sinks to
    # take the hilt; the blade comes off and its weight hauls him forward
    # and down; then the hips turn and drag it round flat through the whole
    # front, his left to his right (a backhand), the body overrunning it,
    # until the left hand catches it. Lands at 420 ms of 0.73 s (8 frames
    # at 11 fps): t = 0.575.
    out.append(Clip("heave", 8, 11, False, [
        Key(0.0, base, low),
        Key(0.10, reach(18, 0.5), (0, 0, -0.12), ease="out"),
        *swap(0.20, reach(24, 0.8), drawn_over_shoulder(24, 0.8), (0, 0, -0.18)),
        Key(0.36, swing(150, 125, two=False, height=1.18, coil=1.0,
                        drop_left=14), swing_root(1.0, 0.0), ease="in"),
        *through(0.36, 0.45, 8, (150, 75), (125, 30), (1.0, 0.6), (0.0, 0.4),
                 (1.18, 1.10), two=False, drop_left=(14, 6)),
        *through(0.45, 0.575, 8, (75, 0), (30, -40), (0.6, 0.1), (0.4, 1.0),
                 (1.10, 0.96), drop_left=(6, 0)),
        *through(0.575, 0.76, 8, (0, -120), (-40, -100), (0.1, 0.0), (1.0, 1.0),
                 (0.96, 1.0)),
        Key(0.88, swing(-90, -80, height=1.0, drive=0.6), swing_root(0, 0.6),
            ease="out"),
        Key(1.0, g, BRACED_ROOT)]))

    # Out of a run: the drag is the wind-up. He plants, turns his hips, and
    # rips it up from behind him on his right into a flat forehand round to
    # his left. 380 ms of 0.64 s (7 at 11): t = 0.6.
    out.append(Clip("heave_run", 7, 11, False, [
        Key(0.0, drag(0.0), DRAG_ROOT),
        Key(0.18, swing(-160, -125, height=0.98, coil=1.0),
            swing_root(1.0, 0.0), ease="out"),
        *through(0.18, 0.60, 7, (-160, 0), (-125, 40), (1.0, 0.1), (0.0, 1.0),
                 (0.98, 0.96)),
        *through(0.60, 0.80, 7, (0, 115), (40, 100), (0.1, 0.0), (1.0, 1.0),
                 (0.96, 1.02)),
        Key(1.0, g, BRACED_ROOT)]))

    # Two hands, back and forth: coil, then the hips go first and the blade
    # is dragged round after them, and its weight carries him on past.
    # 300 ms of 0.58 s (7 at 12): t = 0.51.
    for name, sign in (("slash_f", -1), ("slash_b", 1)):
        wind, follow = 125 * sign, -112 * sign
        out.append(Clip(name, 7, 12, False, [
            Key(0.0, g, BRACED_ROOT),
            Key(0.30, swing(wind, 95 * sign, height=1.30, coil=1.0),
                swing_root(1.0, 0.0), ease="out"),
            *through(0.30, 0.51, 7, (wind, 0), (95 * sign, -40 * sign),
                     (1.0, 0.15), (0.0, 1.0), (1.30, 0.94)),
            *through(0.51, 0.72, 7, (0, follow), (-40 * sign, -100 * sign),
                     (0.15, 0.0), (1.0, 1.0), (0.94, 1.0)),
            Key(0.86, swing(follow * 0.92, -92 * sign, height=1.0, drive=0.6),
                swing_root(0, 0.6), ease="out"),
            Key(1.0, g, BRACED_ROOT)]))

    # Ravager's Edge: the held coil, then the release. The upper back throws
    # it over; the blade goes out and down and drags him after it, his body
    # swinging up off the floor about the grip; it lands at 300 ms (t = 0.25
    # of 1.2 s). He comes down on his knees behind it, hauls it out leaning
    # back, swings it onto his shoulder, and only then lowers it to the
    # guard (the server roots him for all 1.2 s).
    out.append(Clip("ravager_charge", 3, 8, True, [
        Key(0.0, ravager_hold(0.0), CHARGE_ROOT, ease="linear"),
        Key(0.5, ravager_hold(1.0), (0, 0.04, -0.18), ease="linear"),
        Key(1.0, ravager_hold(0.0), CHARGE_ROOT, ease="linear")]))
    impact, impact_root = ravager_impact()
    sagged, sagged_root = ravager_impact(1.0)
    recover, recover_root = ravager_recover()
    heave, heave_root = ravager_heave()
    # Two motions: the throw, feet planted, the body weight going into it
    # (coil, over the top); then, once it crosses in front of him, the
    # blade's weight takes him: over his planted left foot, head below his
    # waist and right leg high by the impact at 300 ms (t = 0.25). The
    # server moves him only a short lurch forward.
    # Every key on a frame time (12 frames: t = k / 11), so no frame blends
    # two blade angles. The impact frame is t = 3/11 (327 ms, the server's
    # hit at 300 ms between it and the frame before).
    f = [k / 11 for k in range(12)]
    out.append(Clip("ravager", 12, 10, False, [
        Key(f[0], ravager_hold(1.0), CHARGE_ROOT),
        Key(f[1], ravager_over(), (0, 0.0, -0.06), ease="out"),
        Key(f[2], ravager_cross(), (0, 0, -0.06), ease="in"),
        Key(f[3], impact, impact_root, ease="in"),
        Key(f[4], sagged, sagged_root),
        Key(f[5], sagged, sagged_root),
        Key(f[6], recover, recover_root, ease="out"),
        Key(f[7], heave, heave_root, ease="in"),
        Key(f[8], on_shoulder(), (0, 0, -0.10), ease="out"),
        Key(f[9], on_shoulder(), (0, 0, -0.12)),
        Key(f[10], on_shoulder(), (0, 0, -0.11)),
        Key(f[11], g, BRACED_ROOT, ease="in")]))

    # Fury of the Blade: up across his shoulders, a roar, and down again.
    out.append(Clip("fury", 6, 9, False, [
        Key(0.0, g, BRACED_ROOT),
        Key(0.30, fury(0.3), (0, 0, -0.06), ease="out"),
        Key(0.50, fury(1.0), (0, 0.02, -0.02), ease="out"),
        Key(0.75, fury(1.0), (0, 0.02, -0.02)),
        Key(1.0, g, BRACED_ROOT)]))

    # Blade Rush: the slide (170 ms dash and a 90 ms beat, held), then the
    # stab, sent as its own cast.
    # Blade Rush: the slide (the server's 170 ms dash) dragging it, then in
    # the 90 ms beat both hands wind it back to his hip; the stab is sent as
    # its own cast and the thrust lands at once.
    out.append(Clip("rush", 3, 11, False, [
        Key(0.0, g, BRACED_ROOT),
        Key(0.40, slide(), SLIDE_ROOT, ease="out"),
        Key(0.62, slide(), (0, -0.05, -0.31)),
        Key(1.0, chamber(), CHAMBER_ROOT, ease="in")]))
    out.append(Clip("rush_stab", 5, 12, False, [
        Key(0.0, chamber(), CHAMBER_ROOT),
        Key(0.18, stab(), (0, -0.30, -0.24), ease="in"),
        Key(0.55, stab(), (0, -0.31, -0.25)),
        Key(1.0, g, BRACED_ROOT)]))

    out.append(Clip("hit", 3, 14, False, [
        Key(0.0, base, low), Key(0.3, hurt(), (0, 0.06, -0.03), ease="out"),
        Key(1.0, base, low)], overlap=0.025))
    out.append(Clip("hit_drawn", 3, 14, False, [
        Key(0.0, g, BRACED_ROOT),
        Key(0.3, hurt_drawn(), (0, 0.06, -0.08), ease="out"),
        Key(1.0, g, BRACED_ROOT)]))

    # The host falls. The blade leaves him at once: the game draws it on
    # its own, stuck in the floor where he went down.
    out.append(Clip("death", 6, 10, False, [
        Key(0.0, bare(hurt()), (0, 0.04, -0.03)),
        Key(0.35, bare(kneel()), KNEEL_ROOT, ease="out"),
        Key(0.75, bare(lying()), (0, -0.10, 0.12), rot=(86, 0, 0), ease="in"),
        Key(1.0, bare(lying()), (0, -0.10, 0.12), rot=(90, 0, 0))],
        overlap=0.02))

    # The blade raises its host: he is there kneeling with his hand on it,
    # stands, and slings it onto his back.
    out.append(Clip("rise", 6, 9, False, [
        Key(0.0, risen_grip(), KNEEL_ROOT),
        Key(0.25, risen_grip(), (0, 0.02, -0.47)),
        Key(0.55, with_(reach(), chest=(-4, 0, 12)), (0, 0.0, -0.06),
            ease="out"),
        *swap(0.70, drawn_over_shoulder(), base, (0, 0.0, -0.02)),
        Key(1.0, base, low, ease="in")]))
    return out
