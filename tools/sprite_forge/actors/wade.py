"""Wade, the Old Head. Built on the heroic body kit (_body.py).

A tight, short white crop and a full dark beard, orange skin, a tiger-stripe tattoo
sleeve on his left arm, big wrapped fists, baggy khaki trousers with a dark
sash. A boxer who lives in his guard.

Proportions: a veteran heavyweight in the 3rd Strike mould: broad, sloping
shoulders and a deep chest over a narrow waist, heavy forearms and
oversized wrapped mitts, baggy trousers on planted legs.

Motion: fighting-game timing. Every punch has an anticipation, a snappy
extension where the forearm stretches and the fist swells (a smear), a held
impact and a recoil. The hips lead and the fist arrives last (overlap), and
his hair and sash tail swing on springs (FOLLOW).
"""

import math

from forge import Clip, Key, Model
import _body
import _rig

STYLE = "toon"
RIM = (255, 176, 72)          # tiger orange
PIXEL_SCALE = 1.0
CELL = (176, 156, 128)

SKIN = (214, 112, 52)
HAIR = (238, 234, 226)
BEARD = (44, 30, 26)
PANTS = (200, 166, 104)
SASH = (150, 80, 38)
WAIST = (40, 34, 36)
WRAP = (240, 234, 214)
LEGWRAP = (150, 146, 112)
INK = (24, 16, 18)

# 3rd Strike proportions (think Ken, a little heavier and older): about
# seven heads tall, long legs in a wide stance, squared athletic shoulders
# and a real waist, long arms with fists that read but are not mitts.
BUILD = _body.Build(
    head=0.29, shoulders=0.33, chest=(0.66, 0.27, 0.33), waist=0.29,
    hip_width=0.14, upper_arm=(0.10, 0.125, 0.08), forearm=(0.085, 0.108, 0.075),
    hand=0.16, thigh=(0.17, 0.19, 0.17), calf=(0.16, 0.17, 0.12),
    foot=(0.11, 0.26, 0.08), arm_length=1.12, leg_length=1.10, neck=0.08,
    deltoid=2.2, pecs=0.85, square=0.11)

# Poses by position (fists, feet) instead of by angle: see _rig.py.
RIG = _rig.Rig(_body.joint_table(BUILD))

# Secondary motion: joint -> (drive joint, gain, stiffness, damping, drag)
FOLLOW = {
    "sash": ("hips", 1.0, 80.0, 7.0, 1.0),
}


def build():
    b = BUILD
    m = Model("Wade")
    skin = m.material("skin", SKIN)
    hair = m.material("hair", HAIR)
    beard = m.material("beard", BEARD)
    pants = m.material("pants", PANTS)
    sash = m.material("sash", SASH)
    waist = m.material("waist", WAIST)
    legwrap = m.material("legwrap", LEGWRAP)
    wrap = m.material("wrap", WRAP)
    ink = m.material("ink", INK)

    _body.skeleton(m, b)
    hz = _body.skeleton_heights(m)["hips"]
    m.joint("hair", "head", (0, 0.06, hz + 0.88))
    m.joint("sash", "hips", (0.08, -0.15, hz - 0.02))

    # ---- body ----------------------------------------------------------
    _body.torso(m, b, skin)
    _body.head(m, b, skin)
    _body.arms(m, b, skin, skin, wrap)
    _body.legs(m, b, pants, pants, skin)

    # ---- face: a short, full beard and a stern brow ---------------------------
    w = b.head
    # Features sit on the face surface (the skull's front is about y=-0.17).
    m.blob("head", beard, (0, -0.105, hz + 0.70), (w * 0.84, w * 0.62, w * 0.52),
           segments=14, rings=8, smooth=True)                        # full beard
    for x in (1, -1):
        m.tube("head", beard, [(0.12 * x, -0.06, hz + 0.80), (0.11 * x, -0.12, hz + 0.72)],
               [0.03, 0.04], sides=6)                                # sideburns
    m.tube("head", beard, [(-0.07, -0.178, hz + 0.765), (0, -0.19, hz + 0.772),
                           (0.07, -0.178, hz + 0.765)], [0.022, 0.026, 0.022],
           sides=6)                                                  # moustache
    for x in (1, -1):
        m.box("head", ink, (0.06 * x, -0.17, hz + 0.818),
              (0.055, 0.02, 0.026))                                  # eyes
        m.tube("head", beard, [(0.02 * x, -0.18, hz + 0.84), (0.11 * x, -0.16, hz + 0.856)],
               [0.024, 0.016], sides=6)                              # hard brows
        m.blob("head", skin, (0.14 * x, -0.01, hz + 0.80), (0.05, 0.07, 0.09),
               segments=8, rings=6, smooth=True)                     # ears

    # ---- hair: a tight, short white crop, swept back -------------------------
    # Sits on the crown and back of the skull; the hairline starts well
    # above the brow so the face and beard read.
    m.blob("head", hair, (0, 0.045, hz + 0.905), (w * 0.98, w * 0.98, w * 0.46),
           segments=16, rings=10, smooth=True)
    m.blob("head", hair, (0, -0.035, hz + 0.945), (w * 0.70, w * 0.34, w * 0.22),
           segments=12, rings=8, smooth=True)                        # short front lift

    # ---- tiger-stripe sleeve: shoulder to wrist on the left arm ---------------
    def stripes(joint, a, bpt, radius, count, phase):
        """Bold, tapering stripes that wrap part way round a limb segment."""
        for k in range(count):
            u = (k + 0.5) / count
            cx = a[0] + (bpt[0] - a[0]) * u
            cz = a[2] + (bpt[2] - a[2]) * u
            r = radius(u) + 0.004
            start = phase + k * 1.3
            span = 3.6 + 0.8 * ((k * 7) % 3) / 2.0
            pts = []
            for t in range(7):
                ang = start + span * t / 6
                # A diagonal sweep, like the splash art's stripes.
                pts.append((cx + math.cos(ang) * r, math.sin(ang) * r,
                            cz + 0.07 * (t - 3) / 3))
            m.tube(joint, ink, pts, [0.004, 0.017, 0.023, 0.025, 0.023, 0.017, 0.004],
                   sides=6, steps=3)

    s_ = m.rest_world("shoulder_L")
    e_ = m.rest_world("elbow_L")
    w_ = m.rest_world("wrist_L")
    r0, r1, r2 = b.upper_arm
    # Below the shoulder cap...
    stripes("shoulder_L", (s_[0], 0, s_[2] - 0.10), e_,
            lambda u: r1 - (r1 - r2) * max(0.0, u - 0.5), 4, 0.3)
    stripes("elbow_L", e_, (w_[0], 0, w_[2] + 0.15),
            lambda u: b.forearm[1], 3, 2.4)
    # ...and over it, wrapping the deltoid.
    cap = r1 * b.deltoid * 0.5
    stripes("shoulder_L", (s_[0], 0, s_[2] + 0.06), (s_[0], 0, s_[2] - 0.08),
            lambda u: cap * (0.85 + 0.15 * (1 - abs(u - 0.5) * 2)), 3, 2.0)
    for k, (x, z, ang) in enumerate(((0.20, hz + 0.53, 0.6), (0.24, hz + 0.47, 0.9),
                                     (0.16, hz + 0.46, 0.3))):
        m.tube("chest", ink, [(x - 0.06, -0.15, z + 0.03), (x, -0.17, z),
                              (x + 0.07, -0.14, z - 0.03 * ang)],
               [0.004, 0.018, 0.004], sides=5, steps=3)              # onto the pec

    # ---- wraps at the wrists and ankles ------------------------------------------
    for side in ("L", "R"):
        w = m.rest_world(f"wrist_{side}")
        # Hand wraps run from the fist well up the forearm.
        m.tube(f"elbow_{side}", wrap, [(w[0], 0, w[2] + 0.13), (w[0], 0, w[2] - 0.01)],
               [0.084, 0.082], sides=10)
        a = m.rest_world(f"ankle_{side}")
        # Bandaged lower legs over the trouser cuffs.
        m.tube(f"knee_{side}", legwrap, [(a[0], 0, a[2] + 0.24), (a[0], 0, a[2] + 0.12),
                                         (a[0], 0, a[2] + 0.02)],
               [0.13, 0.11, 0.09], sides=10)

    # ---- sash: tied at the waist, a tail that swings ------------------------------
    m.tube("hips", waist, [(-0.25, 0.0, hz + 0.10), (0, -0.18, hz + 0.11),
                           (0.25, 0.0, hz + 0.10), (0, 0.18, hz + 0.11),
                           (-0.25, 0.0, hz + 0.10)], [0.05, 0.05, 0.05, 0.05, 0.05],
           sides=6)                                                  # dark waistband
    m.tube("hips", sash, [(-0.25, 0.0, hz + 0.04), (0, -0.18, hz + 0.05),
                          (0.25, 0.0, hz + 0.04), (0, 0.18, hz + 0.05),
                          (-0.25, 0.0, hz + 0.04)], [0.045, 0.045, 0.045, 0.045, 0.045],
           sides=6)                                                  # brown belt
    m.blob("hips", sash, (0.06, -0.18, hz + 0.04), (0.08, 0.05, 0.07),
           segments=8, rings=6, smooth=True)                         # knot
    m.tube("sash", sash, [(0.08, -0.16, hz + 0.04), (0.10, -0.18, hz - 0.12),
                          (0.11, -0.17, hz - 0.30)], [0.05, 0.045, 0.02], sides=6)

    return m, guard()


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

def guard(bounce=0.0, roll=0.0):
    """Peek-a-boo (the Tyson guard): crouched forward from the waist, chin
    tucked behind the gloves, gloves up at the cheeks side by side, forearms
    near vertical, elbows pointing down and tight to the ribs (just outside
    them), knees bent, feet bladed about shoulder width.
    `bounce` 0..1 sinks into the knees, `roll` -1..1 weaves the head.

    The arms hang off the chest joint, so leaning forward tips them back
    with the torso: the shoulders swing well forward (about -84) to put the
    elbows down in front of the lower chest, and the elbows bend about -114
    so the forearms stand upright with the gloves at the cheeks, just in
    front of the face. Measured with joint positions, not guessed."""
    return {
        "hips": (0, 0, 10 + 3 * roll),
        "abdomen": (8 + 2 * bounce, 0, -3),
        "chest": (10 + 3 * bounce, 0, -4 - 4 * roll),
        "neck": (2, 0, -2),
        "head": (8, 0, -2 + 4 * roll),
        "shoulder_L": (-84 - 2 * roll, -8, -27), "elbow_L": (-114, 0, 0),
        "wrist_L": (-6, 0, 0),
        "shoulder_R": (-80 + 2 * roll, 8, 27), "elbow_R": (-116, 0, 0),
        "wrist_R": (-6, 0, 0),
        "hip_L": (-24 - 6 * bounce, -6, -6), "knee_L": (28 + 14 * bounce, 0, 0),
        "ankle_L": (-6 - 8 * bounce, 6, 0),
        "hip_R": (12 - 6 * bounce, 6, 10), "knee_R": (32 + 14 * bounce, 0, 0),
        "ankle_R": (-20 - 8 * bounce, -6, 0),
        "hair": (0, 0, 0), "sash": (0, 0, 0),
    }


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def mirror(pose):
    """Swap left / right: one authored punch serves both hands."""
    out = {}
    for name, r in pose.items():
        if name.endswith("_L"):
            name = name[:-2] + "_R"
        elif name.endswith("_R"):
            name = name[:-2] + "_L"
        out[name] = (r[0], -r[1], -r[2]) + tuple(r[3:])
    return out


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


# ---------------------------------------------------------------------------
# Boxing, by position
# ---------------------------------------------------------------------------
# The punches, the slip and the roll are authored in control space (see
# _rig.py): where the hips, fists and feet are, and how the spine turns.
# The references are the wireframe boxing sheets from the design team
# (jab, cross, hooks, body shots, uppercut loop, slip + chamber, haymaker,
# slip-duck-roll). What they have in common, and what these clips keep:
#   - the feet stay planted and the rear heel turns over on rear-hand power;
#   - the hips turn first and the fist arrives last;
#   - the hand that is not punching stays home at the chin;
#   - the head comes off the centre line with every punch.
# He is orthodox: left (+x) is the lead hand, the one with the tiger sleeve.

LOW = (0, 0, -0.03)                 # the guard sits a little into the knees
SMEAR = (1.0, 1.0, 1.38)            # forearm on the frame the punch lands
SMEAR_BIG = (1.0, 1.0, 1.42)
SETTLE = (1.0, 1.0, 1.10)           # ...and on the held frame after it


def fist(k):
    return (-6, 0, 0, k, k, k)


def stance(off=(0, 0, 0), twist=(10, -3, -4), lean=(8, 10), side=(0, 0),
           tilt=0.0, nod=0.0, look=0.85, L=None, R=None, fl=None, fr=None):
    """A control: the guard, changed.

    off    hips, relative to the guard (x his left, -y forward, z up)
    twist  yaw of (hips, abdomen, chest); + turns him to his left, which
           brings the rear (right) shoulder through
    lean   forward bend of (abdomen, chest)
    side   side bend of (abdomen, chest); + toward his lead side
    tilt   hips pitch; nod: extra head pitch (+ chin down)
    look   how much the head turns back to face the target
    L, R   None: that glove stays home at the cheek, riding the torso.
           dict(at=(x, y, z), elbow=(dx, dy, dz)): the wrist goes to `at`
           (a point past reach means "straight at it"), the elbow sticks
           out along `elbow`. Optional fore= / fist= for smears.
    fl, fr dict(at=, rot=(pitch, roll, yaw), knee=(dx, dy, dz)) to move a
           foot; default is the guard's planted feet.
    """
    g = guard()
    c = RIG.control(g, LOW)
    c["offset"] = (LOW[0] + off[0], LOW[1] + off[1], LOW[2] + off[2])
    c["hips"] = (tilt, 0, twist[0])
    c["abdomen"] = (lean[0], side[0], twist[1])
    c["chest"] = (lean[1], side[1], twist[2])
    # The head stays on the target while the shoulders turn under it.
    yaw = sum(twist)
    bend = lean[0] + lean[1] + tilt - 18
    roll = side[0] + side[1]
    c["neck"] = (2 - 0.2 * bend, -0.3 * roll, -2 - 0.35 * look * (yaw - 3))
    c["head"] = (8 - 0.35 * bend + nod, -0.4 * roll, -2 - 0.65 * look * (yaw - 3))
    c = RIG.ride(c, g)
    for s, arm in (("L", L), ("R", R)):
        if arm is None:
            continue
        sh = RIG.joint_at(c, f"shoulder_{s}")
        c[f"hand_{s}"] = tuple(arm["at"])
        c[f"elbow_{s}"] = _rig.aim(sh, arm.get("elbow", (0, 0, -1)))
        c[f"fore_{s}"] = arm.get("fore", (1.0, 1.0, 1.0))
        c[f"wrist_{s}"] = fist(arm["fist"]) if "fist" in arm else (-6, 0, 0)
    for s, foot in (("L", fl), ("R", fr)):
        if foot is None:
            continue
        if "at" in foot:
            c[f"foot_{s}"] = tuple(foot["at"])
        if "rot" in foot:
            c[f"footrot_{s}"] = tuple(foot["rot"])
        if "knee" in foot:
            c[f"knee_{s}"] = _rig.aim(RIG.joint_at(c, f"hip_{s}"), foot["knee"])
    return c


# The rear foot turned over on its ball: heel high, knee driving in.
PIVOT_R = dict(at=(-0.36, 0.30, 0.31), rot=(52, 0, -6), knee=(0.25, -1, -0.1))
PIVOT_R_BIG = dict(at=(-0.34, 0.26, 0.36), rot=(68, 0, -18), knee=(0.35, -1, -0.2))
# The lead foot turned in on a lead hook.
PIVOT_L = dict(at=(0.255, -0.132, 0.17), rot=(18, 0, -34), knee=(-0.45, -1, 0))


def boxing_clips(base):
    """Wade's punches, slip and roll. `base` fills hair and sash."""
    G = stance()
    out = []

    def clip(name, frames, fps, keys):
        out.append(RIG.clip(name, frames, fps, keys, base))

    # Damage lands when the button is pressed, so every punch hits early:
    # one frame of load, the punch on frame 2, a held frame, then home.

    # ---- 1: jab. A snap off the lead shoulder --------------------------------
    jab_at = (0.05, -1.7, 1.60)
    clip("jab", 8, 24, [
        (0, G),
        (1, stance((0, 0.02, -0.03), twist=(16, -1, 2),
                   L=dict(at=(0.30, -0.30, 1.62), elbow=(0.5, 0.3, -1))), "out"),
        (2, stance((0.02, -0.15, -0.05), twist=(0, -10, -26), lean=(9, 12),
                   L=dict(at=jab_at, elbow=(0.5, 0, -1), fore=SMEAR, fist=1.3)),
         "snap"),
        (3, stance((0.02, -0.14, -0.05), twist=(1, -10, -24), lean=(9, 12),
                   L=dict(at=jab_at, elbow=(0.5, 0, -1), fore=SETTLE, fist=1.12))),
        (4, stance((0.02, -0.12, -0.04), twist=(3, -8, -18), lean=(9, 11),
                   L=dict(at=(0.10, -0.80, 1.62), elbow=(0.5, 0, -1)))),
        (7, G),
    ])

    # ---- 2: cross. The hips whip, the rear heel turns over -------------------
    cross_at = (0.0, -1.8, 1.58)
    clip("cross", 9, 24, [
        (0, G),
        (1, stance((-0.03, 0.04, -0.05), twist=(2, -8, -12), lean=(6, 8),
                   R=dict(at=(-0.36, -0.14, 1.60), elbow=(-0.5, 0.6, -1))), "out"),
        (2, stance((0.05, -0.20, -0.08), twist=(28, 10, 14), lean=(10, 14),
                   side=(3, 4),
                   R=dict(at=cross_at, elbow=(-0.4, 0, -1), fore=SMEAR, fist=1.32),
                   fr=PIVOT_R), "snap"),
        (3, stance((0.05, -0.19, -0.08), twist=(27, 10, 13), lean=(10, 14),
                   side=(3, 4),
                   R=dict(at=cross_at, elbow=(-0.4, 0, -1), fore=SETTLE, fist=1.12),
                   fr=PIVOT_R)),
        (4, stance((0.05, -0.18, -0.07), twist=(25, 9, 11), lean=(10, 13),
                   side=(2, 3),
                   R=dict(at=(-0.05, -1.5, 1.58), elbow=(-0.4, 0, -1)),
                   fr=PIVOT_R)),
        (6, stance((0.02, -0.07, -0.03), twist=(16, 2, 2), lean=(8, 11),
                   R=dict(at=(-0.20, -0.52, 1.66), elbow=(-0.3, 0.2, -1)))),
        (8, G),
    ])

    # ---- 3: lead hook. Elbow up, the whole left side turns through -----------
    clip("hook_L", 9, 24, [
        (0, G),
        (1, stance((0.05, 0.0, -0.06), twist=(22, 4, 8), lean=(8, 8),
                   L=dict(at=(0.50, -0.30, 1.52), elbow=(1, 0.6, 0.0))),
         "out"),
        (2, stance((0.0, -0.08, -0.06), twist=(-8, -14, -26), lean=(8, 9),
                   side=(-2, -3),
                   L=dict(at=(-0.04, -0.72, 1.57), elbow=(1, -0.25, 0.05),
                          fore=SMEAR, fist=1.32), fl=PIVOT_L), "snap"),
        (3, stance((0.0, -0.08, -0.06), twist=(-12, -16, -30), lean=(8, 9),
                   side=(-2, -3),
                   L=dict(at=(-0.20, -0.52, 1.57), elbow=(1, -0.6, 0.05),
                          fore=SETTLE, fist=1.12), fl=PIVOT_L)),
        (4, stance((0.0, -0.07, -0.06), twist=(-13, -16, -30), lean=(8, 9),
                   side=(-2, -3),
                   L=dict(at=(-0.26, -0.46, 1.57), elbow=(1, -0.8, 0.0)),
                   fl=PIVOT_L)),
        (6, stance((0.0, -0.03, -0.03), twist=(2, -8, -14), lean=(8, 10),
                   L=dict(at=(0.08, -0.40, 1.66), elbow=(0.8, -0.2, -0.7)))),
        (8, G),
    ])

    # ---- 4: rear hook. Wound right back, then all the way round --------------
    clip("hook_R", 10, 24, [
        (0, G),
        (1, stance((-0.05, 0.05, -0.07), twist=(-4, -10, -16), lean=(6, 6),
                   R=dict(at=(-0.64, 0.04, 1.52), elbow=(-1, 0.9, 0.0))), "out"),
        (2, stance((0.04, -0.14, -0.08), twist=(30, 12, 18), lean=(9, 11),
                   side=(3, 4),
                   R=dict(at=(0.04, -0.80, 1.57), elbow=(-1, -0.2, 0.08),
                          fore=SMEAR, fist=1.34), fr=PIVOT_R), "snap"),
        (3, stance((0.04, -0.14, -0.08), twist=(34, 14, 22), lean=(9, 11),
                   side=(3, 4),
                   R=dict(at=(0.22, -0.56, 1.56), elbow=(-1, -0.6, 0.08),
                          fore=SETTLE, fist=1.12), fr=PIVOT_R)),
        (4, stance((0.04, -0.13, -0.08), twist=(36, 14, 23), lean=(9, 11),
                   side=(3, 4),
                   R=dict(at=(0.30, -0.48, 1.55), elbow=(-1, -0.8, 0.0)),
                   fr=PIVOT_R)),
        (7, stance((0.02, -0.05, -0.03), twist=(18, 3, 4), lean=(8, 10),
                   R=dict(at=(-0.12, -0.42, 1.66), elbow=(-0.8, -0.1, -0.7)))),
        (9, G),
    ])

    # ---- 2 to the body: change levels, same straight line ---------------------
    body_at = (0.0, -1.8, 1.02)
    low_r = dict(at=(-0.36, 0.30, 0.31), rot=(52, 0, -6), knee=(0.2, -1, -0.3))
    clip("cross_body", 9, 24, [
        (0, G),
        (1, stance((-0.03, 0.02, -0.16), twist=(2, -8, -12), lean=(12, 12),
                   R=dict(at=(-0.36, -0.10, 1.40), elbow=(-0.5, 0.7, -1))), "out"),
        (2, stance((0.06, -0.22, -0.27), twist=(28, 10, 14), lean=(20, 20),
                   side=(4, 5),
                   R=dict(at=body_at, elbow=(-0.5, 0, -0.6), fore=SMEAR, fist=1.32),
                   fr=low_r), "snap"),
        (3, stance((0.06, -0.21, -0.27), twist=(27, 10, 13), lean=(20, 20),
                   side=(4, 5),
                   R=dict(at=body_at, elbow=(-0.5, 0, -0.6), fore=SETTLE, fist=1.12),
                   fr=low_r)),
        (4, stance((0.06, -0.20, -0.25), twist=(25, 9, 11), lean=(19, 19),
                   side=(3, 4),
                   R=dict(at=(-0.04, -1.5, 1.08), elbow=(-0.5, 0, -0.6)),
                   fr=low_r)),
        (6, stance((0.02, -0.08, -0.12), twist=(16, 2, 2), lean=(12, 13),
                   R=dict(at=(-0.20, -0.50, 1.50), elbow=(-0.3, 0.2, -1)))),
        (8, G),
    ])

    # ---- 3 to the body: dip to the lead side and dig it in --------------------
    # The shovel hook: elbow at the hip, forearm level, the body turns it.
    clip("hook_body", 9, 24, [
        (0, G),
        (1, stance((0.10, 0.0, -0.20), twist=(24, 6, 10), lean=(12, 10),
                   side=(8, 10),
                   L=dict(at=(0.50, -0.06, 1.02), elbow=(0.5, 1, -0.3))), "out"),
        (2, stance((0.06, -0.12, -0.24), twist=(-6, -12, -24), lean=(14, 12),
                   side=(8, 10),
                   L=dict(at=(0.02, -0.62, 1.06), elbow=(0.8, 0.5, -0.7),
                          fore=SMEAR, fist=1.32), fl=PIVOT_L), "snap"),
        (3, stance((0.06, -0.12, -0.24), twist=(-10, -14, -28), lean=(14, 12),
                   side=(8, 10),
                   L=dict(at=(-0.12, -0.58, 1.08), elbow=(0.8, 0.3, -0.7),
                          fore=SETTLE, fist=1.12), fl=PIVOT_L)),
        (4, stance((0.06, -0.11, -0.23), twist=(-11, -14, -28), lean=(14, 12),
                   side=(7, 9),
                   L=dict(at=(-0.16, -0.52, 1.10), elbow=(0.8, 0.2, -0.7)),
                   fl=PIVOT_L)),
        (6, stance((0.03, -0.04, -0.10), twist=(2, -8, -14), lean=(10, 11),
                   side=(3, 4),
                   L=dict(at=(0.14, -0.38, 1.50), elbow=(0.6, 0.1, -1)))),
        (8, G),
    ])

    # ---- 5 and 6: the uppercut loop (hold RMB) --------------------------------
    # Feet planted, sat down in the knees. Each clip starts already coiled
    # on its own side, drives up through the legs, and recovers into the
    # coil for the other hand, so 5-6-5-6 chains with no reset. The tail
    # (only seen on release) stands him back up into the guard.
    def coil(s):
        """Sat low, weight over the punching side, that fist by the hip."""
        if s == "L":
            return stance((0.07, -0.02, -0.25), twist=(22, 6, 10), lean=(14, 14),
                          side=(6, 8),
                          L=dict(at=(0.46, -0.10, 0.98), elbow=(0.4, 1, -0.2)))
        return stance((-0.07, 0.0, -0.25), twist=(-6, -10, -16), lean=(14, 14),
                      side=(-6, -8),
                      R=dict(at=(-0.50, -0.02, 0.98), elbow=(-0.4, 1, -0.2)))

    def uppercut(s):
        other = "R" if s == "L" else "L"
        x = 1 if s == "L" else -1
        turn = (-10, -12, -22) if s == "L" else (30, 10, 16)
        part = (0, -6, -8) if s == "L" else (22, 2, 4)
        heel = dict(fl=dict(at=(0.255, -0.132, 0.16), rot=(14, 0, -20),
                            knee=(-0.3, -1, 0))) if s == "L" else dict(fr=PIVOT_R)
        arm = lambda **kw: {s: dict(**kw)}

        def up(at, elbow, **kw):
            return {s: dict(at=at, elbow=elbow, **kw)}

        return [
            (0, coil(s)),
            (1, stance((0.04 * x, -0.06, -0.17), twist=part, lean=(12, 10),
                       side=(3 * x, 4 * x),
                       **up((0.22 * x, -0.40, 1.16), (0.3 * x, 0.6, -1)), **heel),
             "in"),
            (2, stance((0.0, -0.10, -0.01), twist=turn, lean=(2, -2),
                       side=(-2 * x, -3 * x), nod=-6,
                       **up((0.03 * x, -0.50, 1.66), (0.25 * x, 0.35, -1),
                            fore=SMEAR, fist=1.32), **heel), "snap"),
            (3, stance((0.0, -0.10, -0.02), twist=turn, lean=(3, -1),
                       side=(-2 * x, -3 * x), nod=-6,
                       **up((0.02 * x, -0.50, 1.64), (0.25 * x, 0.35, -1),
                            fore=SETTLE, fist=1.12), **heel)),
            (4, stance((-0.02 * x, -0.07, -0.10), twist=turn, lean=(8, 6),
                       **up((0.06 * x, -0.44, 1.52), (0.3 * x, 0.3, -1)), **heel)),
            (6, coil(other)),
            (7, coil(other)),
            (11, G),
        ]

    clip("uppercut_L", 12, 24, uppercut("L"))
    clip("uppercut_R", 12, 24, uppercut("R"))

    # ---- slip (Q): off the line to the rear side, rear hand chambered --------
    # Held for the 0.5 s window. If something hits him in that window the
    # game cuts to "haymaker", which starts from this exact pose.
    def chamber(sink=0.0):
        return stance((-0.15, 0.10, -0.15 - sink), twist=(-6, -10, -18),
                      lean=(10, 12), side=(-9, -11),
                      R=dict(at=(-0.60, 0.12, 1.20 - sink), elbow=(-0.5, 1, 0.1)),
                      L=dict(at=(0.14, -0.40, 1.52 - sink), elbow=(0.5, 0.2, -1)))

    clip("slip", 13, 20, [
        (0, G),
        (2, chamber(), "snap"),
        (6, chamber(0.03)),
        (10, chamber(0.01)),
        (12, G),
    ])

    # ---- haymaker: the answer to whatever he just slipped --------------------
    # Winds further back from the chamber, then everything he has goes
    # over the top: a long lunge off the rear foot, the fist landing on
    # frame 5 (0.21 s, the game's HAYMAKER_WINDUP_MS), a long hold, and a
    # slow walk back to the guard.
    lunge_l = dict(at=(0.26, -0.42, 0.13), rot=(-2, 0, 4), knee=(0.1, -1, 0))
    drive_r = dict(at=(-0.36, 0.24, 0.36), rot=(70, 0, -10), knee=(0.3, -1, -0.5))
    hay_at = (0.16, -2.2, 1.34)
    hay = dict(off=(0.08, -0.42, -0.26), twist=(38, 16, 26), lean=(18, 20),
               side=(6, 8), fl=lunge_l, fr=drive_r)
    clip("haymaker", 15, 24, [
        (0, chamber()),
        (2, stance((-0.18, 0.14, -0.22), twist=(-12, -14, -24), lean=(8, 6),
                   side=(-10, -12),
                   R=dict(at=(-0.80, 0.42, 1.50), elbow=(-1, 0.8, 0.3)),
                   L=dict(at=(0.16, -0.46, 1.46), elbow=(0.5, 0.2, -1))), "out"),
        (4, stance((-0.02, -0.22, -0.22), twist=(14, 2, 2), lean=(12, 12),
                   R=dict(at=(-0.46, -0.24, 1.74), elbow=(-1, 0.5, 0.4)),
                   L=dict(at=(0.30, -0.30, 1.36), elbow=(0.7, 0.4, -1)),
                   fl=dict(at=(0.26, -0.32, 0.20), rot=(-8, 0, 4),
                           knee=(0.1, -1, 0)), fr=PIVOT_R), "in"),
        (5, stance(R=dict(at=hay_at, elbow=(-0.6, 0, 0.8), fore=SMEAR_BIG,
                          fist=1.5),
                   L=dict(at=(0.24, -0.06, 1.14), elbow=(0.4, 1, -0.3)), **hay),
         "snap"),
        (7, stance(R=dict(at=hay_at, elbow=(-0.6, 0, 0.8), fore=SETTLE,
                          fist=1.15),
                   L=dict(at=(0.24, -0.05, 1.14), elbow=(0.4, 1, -0.3)),
                   **dict(hay, off=(0.08, -0.43, -0.28)))),
        (9, stance((0.08, -0.38, -0.24), twist=(42, 16, 26), lean=(18, 18),
                   side=(4, 6),
                   R=dict(at=(0.34, -1.00, 1.10), elbow=(-0.8, 0, 0.2)),
                   L=dict(at=(0.26, -0.12, 1.20), elbow=(0.4, 1, -0.4)),
                   fl=lunge_l, fr=drive_r)),
        (12, stance((0.04, -0.18, -0.10), twist=(22, 4, 6), lean=(10, 12),
                    R=dict(at=(-0.14, -0.50, 1.56), elbow=(-0.5, 0.1, -1)),
                    fl=dict(at=(0.255, -0.24, 0.15), rot=(-2, 0, 4),
                            knee=(0.1, -1, 0)))),
        (14, G),
    ])

    # ---- roll (Shift): slip, duck, roll under, up the other side --------------
    # A boxing roll, not a tumble: the gloves stay up and the head draws a
    # U under the punch while the dash carries him.
    def weave(x, drop, lean, side, twist):
        return stance((x, -0.03, -drop), twist=twist, lean=lean,
                      side=(side * 0.45, side * 0.55), nod=-4)

    clip("roll", 10, 20, [
        (0, G),
        (1, weave(0.14, 0.10, (10, 12), 16, (16, 0, 2)), "out"),
        (2, weave(0.12, 0.30, (18, 20), 14, (14, 0, 0))),
        (3, weave(0.0, 0.42, (24, 28), 0, (6, -3, -6))),
        (4, weave(-0.12, 0.34, (20, 22), -14, (-2, -6, -12))),
        (6, weave(-0.15, 0.10, (10, 12), -16, (-4, -8, -14))),
        (9, G),
    ])
    return out


def clips():
    g = guard()
    low = (0, 0, -0.03)

    # Idle: the boxer's bounce, two beats a second, shoulders rolling.
    idle = []
    for f in range(13):
        t = f / 12
        bounce = 0.5 - 0.5 * math.cos(2 * 2 * math.pi * t)
        roll = math.sin(2 * math.pi * t)
        idle.append(Key(t, guard(bounce, roll), (0, 0, -0.02 - 0.045 * bounce),
                        ease="linear"))

    run = []
    for f in range(11):
        p = f / 10
        s = math.sin(p * 2 * math.pi)
        run.append(Key(p, run_pose(p), (0, -0.03, -0.04 + 0.06 * (1 - abs(s))),
                       ease="linear"))

    # ---- getting hit: the head snaps, the rest follows -------------------------
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

    return [
        Clip("idle", 12, 12, True, idle, overlap=0.035),
        Clip("run", 10, 15, True, run, overlap=0.02),
        *boxing_clips(g),
        Clip("hit", 6, 20, False, [
            Key(0.0, g, low), Key(0.22, hurt, (0, 0.09, -0.02), ease="out"),
            Key(0.55, with_(hurt, head=(-16, 0, 8)), (0, 0.07, -0.03)),
            Key(1.0, g, low)], overlap=0.025),
        Clip("death", 12, 14, False, [
            Key(0.0, hurt, (0, 0.04, -0.02)),
            Key(0.2, stagger, (0, 0.14, -0.06), ease="out"),
            Key(0.45, kneel, (0, 0.10, -0.42)),
            Key(0.75, lying, (0, 0.10, 0.16), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, 0.10, 0.16), rot=(-90, 0, 0))], overlap=0.02),
    ]
