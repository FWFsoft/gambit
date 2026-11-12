"""Wade, the Old Head. Built on the heroic body kit (_body.py).

White swept-back hair and grey stubble, orange skin, a tiger-stripe tattoo
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

SKIN = (222, 124, 62)
HAIR = (236, 232, 224)
STUBBLE = (150, 140, 132)
PANTS = (196, 166, 112)
SASH = (98, 56, 34)
WRAP = (240, 234, 214)
INK = (58, 30, 26)

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
    "hair": ("head", 0.9, 150.0, 11.0, 0.7),
    "sash": ("hips", 1.0, 80.0, 7.0, 1.0),
}


def build():
    b = BUILD
    m = Model("Wade")
    skin = m.material("skin", SKIN)
    hair = m.material("hair", HAIR)
    stubble = m.material("stubble", STUBBLE)
    pants = m.material("pants", PANTS)
    sash = m.material("sash", SASH)
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

    # ---- face: stubble, moustache, brows, ears -----------------------------
    m.blob("head", stubble, (0, -0.10, hz + 0.72), (0.19, 0.12, 0.10),
           segments=12, rings=8, smooth=True)
    m.tube("head", stubble, [(-0.06, -0.135, hz + 0.765), (0, -0.145, hz + 0.775),
                             (0.06, -0.135, hz + 0.765)], [0.018, 0.022, 0.018],
           sides=6)
    for x in (1, -1):
        m.box("head", ink, (0.055 * x, -0.135, hz + 0.815), (0.045, 0.02, 0.028))
        m.tube("head", hair, [(0.02 * x, -0.135, hz + 0.85), (0.09 * x, -0.12, hz + 0.86)],
               [0.016, 0.012], sides=6)
        m.blob("head", skin, (0.13 * x, -0.01, hz + 0.80), (0.05, 0.07, 0.09),
               segments=8, rings=6, smooth=True)

    # ---- hair: a swept-back white crest that streams behind him --------------
    m.blob("head", hair, (0, 0.04, hz + 0.90), (0.27, 0.27, 0.12), segments=14,
           rings=8, smooth=True)
    for x, lift in ((0.0, 0.04), (0.08, 0.0), (-0.08, 0.0)):
        m.tube("hair", hair, [(x, -0.06, hz + 0.91), (x * 1.2, 0.12, hz + 0.90 + lift),
                              (x * 1.5, 0.30, hz + 0.78 + lift)],
               [0.075, 0.06, 0.012], sides=8)

    # ---- tattoo sleeve on the left arm ---------------------------------------
    s = m.rest_world("shoulder_L")
    e = m.rest_world("elbow_L")
    for i, k in enumerate((0.25, 0.45, 0.65)):
        z = s[2] + (e[2] - s[2]) * k
        x = s[0] + (e[0] - s[0]) * k
        m.tube("shoulder_L", ink, [(x, 0, z + 0.012), (x, 0, z - 0.012)],
               [0.128 - i * 0.004, 0.128 - i * 0.004], sides=12, cap=False)

    # ---- wraps at the wrists and ankles ------------------------------------------
    for side in ("L", "R"):
        w = m.rest_world(f"wrist_{side}")
        m.tube(f"elbow_{side}", wrap, [(w[0], 0, w[2] + 0.07), (w[0], 0, w[2] - 0.01)],
               [0.072, 0.076], sides=10)
        a = m.rest_world(f"ankle_{side}")
        m.tube(f"knee_{side}", wrap, [(a[0], 0, a[2] + 0.10), (a[0], 0, a[2] + 0.02)],
               [0.095, 0.085], sides=10)

    # ---- sash: tied at the waist, a tail that swings ------------------------------
    m.tube("hips", sash, [(-0.24, 0.0, hz + 0.06), (0, -0.17, hz + 0.07),
                          (0.24, 0.0, hz + 0.06), (0, 0.17, hz + 0.07),
                          (-0.24, 0.0, hz + 0.06)], [0.05, 0.05, 0.05, 0.05, 0.05],
           sides=6)
    m.tube("sash", sash, [(0.08, -0.16, hz + 0.04), (0.10, -0.18, hz - 0.12),
                          (0.11, -0.17, hz - 0.30)], [0.05, 0.045, 0.02], sides=6)

    return m, guard()


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

def guard(bounce=0.0, roll=0.0):
    """A 3rd Strike fighting stance: upright and proud, chest out, feet
    planted wide with the knees bent out, fists held in front of the chest
    (lead fist higher and forward). `bounce` 0..1 sinks into the knees,
    `roll` -1..1 rolls the shoulders (the idle's rhythm)."""
    return {
        "hips": (0, 0, 18 + 3 * roll),
        "abdomen": (2 + 2 * bounce, 0, -4),
        "chest": (-2 + 3 * bounce, 0, -8 - 4 * roll),
        "neck": (0, 0, -4),
        "head": (2, 0, -4 + 3 * roll),
        "shoulder_L": (-62 - 4 * roll, -8, -16), "elbow_L": (-92, 0, 0),
        "wrist_L": (-8, 0, 0),
        "shoulder_R": (-34 + 4 * roll, 12, 14), "elbow_R": (-118, 0, 0),
        "wrist_R": (-8, 0, 0),
        "hip_L": (-24 - 6 * bounce, -16, -6), "knee_L": (22 + 16 * bounce, 0, 0),
        "ankle_L": (-4 - 8 * bounce, 14, 0),
        "hip_R": (18 - 6 * bounce, 14, 10), "knee_R": (26 + 16 * bounce, 0, 0),
        "ankle_R": (-14 - 8 * bounce, -12, 0),
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
