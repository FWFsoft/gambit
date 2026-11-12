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


def stretch(along=1.35, fist=1.25):
    """Smear frame helpers: a forearm that lengthens and a fist that swells."""
    return (0, 0, 0, 1.0, 1.0, along), (0, 0, 0, fist, fist, fist)


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

    # ---- jab (lead hand) ------------------------------------------------------
    stretch_el, swell = stretch(1.35, 1.28)
    hold_el, hold_fist = stretch(1.12, 1.12)
    j_load = with_(g, hips=(0, 0, 22), chest=(6, 0, 2),
                   shoulder_L=(-44, -24, -6), elbow_L=(-124, 0, 0))
    j_hit = with_(g, hips=(0, 0, 4), abdomen=(8, 0, -10), chest=(12, 0, -26),
                  head=(10, 0, 12), shoulder_L=(-92, -2, -14),
                  elbow_L=(-4,) + stretch_el[1:], wrist_L=swell,
                  hip_L=(-26, -4, -8), knee_L=(14, 0, 0))
    j_hold = with_(j_hit, elbow_L=(-6,) + hold_el[1:], wrist_L=hold_fist)
    j_back = with_(g, shoulder_L=(-64, -20, -8), elbow_L=(-106, 0, 0))

    # ---- cross (rear hand): the hips whip, the heel turns ---------------------
    # Rear-hand power: the torso turns toward his left (+z) to bring the
    # right shoulder through; the load winds the other way first. Twists
    # add up down the spine (hips + abdomen + chest), so keep the sum near
    # -25 for the load and +35 for the hit.
    c_load = with_(g, hips=(0, 0, -4), abdomen=(4, 0, -8), chest=(6, 0, -12),
                   shoulder_R=(-40, 22, 12), elbow_R=(-136, 0, 0),
                   knee_R=(30, 0, 0))
    c_hit = with_(g, hips=(0, 0, 18), abdomen=(8, 0, 8), chest=(14, 0, 10),
                  head=(8, 0, -10), shoulder_R=(-94, 4, 10),
                  elbow_R=(-4, 0, 0, 1.0, 1.0, 1.42), wrist_R=(0, 0, 0, 1.32, 1.32, 1.32),
                  shoulder_L=(-50, -28, -6), elbow_L=(-132, 0, 0),
                  hip_R=(6, 4, -12), knee_R=(16, 0, 0), ankle_R=(-36, 0, 0),
                  hip_L=(-24, -4, -8), knee_L=(22, 0, 0))
    c_hold = with_(c_hit, elbow_R=(-6, 0, 0, 1.0, 1.0, 1.15),
                   wrist_R=(0, 0, 0, 1.12, 1.12, 1.12))

    # ---- uppercut (rear hand): sink, then explode upward ---------------------
    u_dip = with_(g, hips=(0, 0, -6), abdomen=(18, -6, -8), chest=(22, -10, -12),
                  head=(10, 0, 6), shoulder_R=(-12, 10, 12), elbow_R=(-96, 0, 0),
                  hip_L=(-34, -4, -8), knee_L=(56, 0, 0), ankle_L=(-18, 0, 0),
                  hip_R=(-2, 4, 12), knee_R=(54, 0, 0), ankle_R=(-30, 0, 0))
    u_rise = with_(g, hips=(0, 0, 16), abdomen=(-6, 6, 10), chest=(-16, 8, 12),
                   head=(-14, 0, -6), shoulder_R=(-136, -4, 24),
                   elbow_R=(-74, 0, 0, 1.0, 1.0, 1.28), wrist_R=(0, 0, 0, 1.3, 1.3, 1.3),
                   hip_L=(-12, -4, -8), knee_L=(6, 0, 0),
                   hip_R=(8, 4, 12), knee_R=(6, 0, 0), ankle_R=(-22, 0, 0))
    u_hold = with_(u_rise, elbow_R=(-76, 0, 0, 1.0, 1.0, 1.1),
                   wrist_R=(0, 0, 0, 1.12, 1.12, 1.12))

    # ---- slip: bob and weave off the line -------------------------------------
    s_duck = with_(g, abdomen=(14, 12, 0), chest=(30, 22, -18), head=(-8, 10, 0),
                   hip_L=(-36, -4, -10), knee_L=(62, 0, 0), ankle_L=(-24, 0, 0),
                   hip_R=(-4, 4, 14), knee_R=(58, 0, 0), ankle_R=(-44, 0, 0))

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

    tuck = with_(g, hips=(0, 0, 0), abdomen=(30, 0, 0), chest=(40, 0, 0),
                 head=(30, 0, 0), shoulder_L=(-70, -20, 0), elbow_L=(-130, 0, 0),
                 shoulder_R=(-70, 20, 0), elbow_R=(-130, 0, 0),
                 hip_L=(-110, 0, 0), knee_L=(135, 0, 0), ankle_L=(-20, 0, 0),
                 hip_R=(-104, 0, 0), knee_R=(130, 0, 0), ankle_R=(-20, 0, 0))

    def roll_keys(frames):
        keys = []
        centre, rolling = 0.92, 0.58
        for f in range(frames + 1):
            t = f / frames
            a = math.radians(360.0 * t)
            tucked = 0.08 < t < 0.92
            target = rolling if tucked else centre - 0.05
            off = (0.0, centre * math.sin(a), target - centre * math.cos(a))
            keys.append(Key(t, tuck if tucked else with_(g, chest=(30, 0, 0)),
                            off, (360.0 * t, 0, 0), "linear"))
        return keys

    return [
        Clip("idle", 12, 12, True, idle, overlap=0.035),
        Clip("run", 10, 15, True, run, overlap=0.02),
        Clip("jab", 8, 24, False, [
            Key(0.0, g, low), Key(0.14, j_load, (0, 0.02, -0.04), ease="out"),
            Key(0.32, j_hit, (0, -0.12, -0.04), ease="out"),
            Key(0.52, j_hold, (0, -0.11, -0.04)),
            Key(0.78, j_back, (0, -0.04, -0.03)), Key(1.0, g, low)],
            overlap=0.008),
        Clip("cross", 9, 24, False, [
            Key(0.0, g, low), Key(0.16, c_load, (0, 0.03, -0.05), ease="out"),
            Key(0.36, c_hit, (0, -0.15, -0.05), ease="out"),
            Key(0.56, c_hold, (0, -0.14, -0.05)),
            Key(0.8, with_(g, chest=(10, 0, 10)), (0, -0.05, -0.03)),
            Key(1.0, g, low)], overlap=0.008),
        Clip("uppercut_R", 10, 24, False, [
            Key(0.0, g, low), Key(0.24, u_dip, (0, 0.0, -0.16), ease="out"),
            Key(0.44, u_rise, (0, -0.06, 0.06), ease="out"),
            Key(0.62, u_hold, (0, -0.06, 0.05)),
            Key(1.0, g, low)], overlap=0.01),
        Clip("uppercut_L", 10, 24, False, [
            Key(0.0, g, low), Key(0.24, mirror(u_dip), (0, 0.0, -0.16), ease="out"),
            Key(0.44, mirror(u_rise), (0, -0.06, 0.06), ease="out"),
            Key(0.62, mirror(u_hold), (0, -0.06, 0.05)),
            Key(1.0, g, low)], overlap=0.01),
        Clip("slip", 8, 20, False, [
            Key(0.0, g, low), Key(0.3, s_duck, (0.10, 0.04, -0.17), ease="out"),
            Key(0.7, s_duck, (0.10, 0.04, -0.17)), Key(1.0, g, low)],
            overlap=0.015),
        Clip("roll", 10, 20, False, roll_keys(10)),
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
