"""Wade, the Old Head. Modelled from assets/turnarounds/Wade.png.

White swept-back hair and grey stubble, orange skin, a tiger-stripe tattoo
sleeve on his left arm, wrapped fists and ankles, baggy khaki trousers with
a dark sash. A boxer: he lives in his guard.
"""

import math

from forge import Clip, Key, Model

SKIN = (222, 122, 58)
HAIR = (232, 228, 220)
STUBBLE = (150, 140, 132)
PANTS = (190, 160, 108)
SASH = (104, 66, 38)
WRAP = (236, 228, 206)
INK = (58, 30, 26)
EYE = (34, 22, 24)


def build():
    m = Model("Wade")
    skin = m.material("skin", SKIN)
    hair = m.material("hair", HAIR)
    stubble = m.material("stubble", STUBBLE)
    pants = m.material("pants", PANTS)
    sash = m.material("sash", SASH)
    wrap = m.material("wrap", WRAP)
    ink = m.material("ink", INK)
    eye = m.material("eye", EYE)

    # ---- skeleton -------------------------------------------------------
    m.joint("hips", None, (0, 0, 0.98))
    m.joint("chest", "hips", (0, 0, 1.10))
    m.joint("neck", "chest", (0, 0, 1.50))
    for side, x in (("L", 1), ("R", -1)):
        m.joint(f"shoulder_{side}", "chest", (0.27 * x, 0, 1.45))
        m.joint(f"elbow_{side}", f"shoulder_{side}", (0.30 * x, 0, 1.17))
        m.joint(f"wrist_{side}", f"elbow_{side}", (0.31 * x, 0, 0.93))
        m.joint(f"hip_{side}", "hips", (0.12 * x, 0, 0.94))
        m.joint(f"knee_{side}", f"hip_{side}", (0.13 * x, 0, 0.53))
        m.joint(f"ankle_{side}", f"knee_{side}", (0.13 * x, 0, 0.11))

    # ---- pelvis + sash --------------------------------------------------
    m.box("hips", pants, (0, 0, 0.92), (0.42, 0.28, 0.22), taper=1.05)
    m.box("hips", sash, (0, 0, 1.04), (0.40, 0.27, 0.09))
    m.box("hips", sash, (0.06, -0.15, 1.01), (0.09, 0.05, 0.07))
    m.box("hips", sash, (0.08, -0.15, 0.91), (0.05, 0.03, 0.16), rot=(0, 10, 0))
    m.box("hips", sash, (0.03, -0.15, 0.92), (0.04, 0.03, 0.14), rot=(0, -8, 0))

    # ---- torso: V-shaped and heavy through the chest --------------------
    # Faceted blobs rather than boxes: the flat facets shade like muscle.
    m.blob("chest", skin, (0, -0.01, 1.16), (0.31, 0.22, 0.24), segments=8,
           rings=6)                                                  # abdomen
    m.blob("chest", skin, (0, 0.015, 1.345), (0.47, 0.27, 0.30), segments=10,
           rings=7)                                                  # rib cage
    for x in (1, -1):
        m.blob("chest", skin, (0.10 * x, -0.105, 1.375), (0.21, 0.09, 0.15),
               segments=8, rings=5)                                  # pecs
        m.blob("chest", skin, (0.15 * x, 0.04, 1.47), (0.19, 0.17, 0.10))  # traps
        m.box("chest", ink, (0.05 * x, -0.118, 1.18), (0.035, 0.012, 0.03))  # abs
    m.box("chest", ink, (0, -0.125, 1.29), (0.012, 0.012, 0.11))
    m.limb("neck", skin, (0, 0, 1.47), (0, -0.01, 1.60), 0.075, 0.07)

    # ---- head -----------------------------------------------------------
    m.blob("neck", skin, (0, -0.02, 1.685), (0.23, 0.24, 0.27), segments=10,
           rings=8)
    m.box("neck", skin, (0, -0.04, 1.60), (0.19, 0.18, 0.09), taper=1.15)
    m.box("neck", stubble, (0, -0.10, 1.585), (0.15, 0.06, 0.07), taper=1.2)
    m.box("neck", stubble, (0, -0.125, 1.625), (0.10, 0.03, 0.025))  # moustache
    # Swept-back white hair: low at the brow, fuller at the crown and back.
    m.blob("neck", hair, (0, 0.04, 1.785), (0.22, 0.24, 0.10), rot=(-12, 0, 0))
    m.blob("neck", hair, (0, 0.085, 1.715), (0.225, 0.13, 0.17))
    m.box("neck", hair, (0, -0.07, 1.775), (0.17, 0.08, 0.04), rot=(-20, 0, 0))
    for x in (1, -1):
        m.box("neck", eye, (0.052 * x, -0.128, 1.692), (0.05, 0.02, 0.032))
        m.box("neck", hair, (0.058 * x, -0.13, 1.727), (0.075, 0.025, 0.022))
        m.blob("neck", skin, (0.118 * x, -0.01, 1.68), (0.04, 0.06, 0.08))  # ears

    # ---- arms -----------------------------------------------------------
    for side, x in (("L", 1), ("R", -1)):
        sh, el, wr = f"shoulder_{side}", f"elbow_{side}", f"wrist_{side}"
        m.blob(sh, skin, (0.27 * x, 0, 1.44), (0.19, 0.19, 0.18))  # deltoid
        m.limb(sh, skin, (0.28 * x, 0, 1.42), (0.30 * x, 0, 1.17), 0.088, 0.072)
        m.limb(el, skin, (0.30 * x, 0, 1.17), (0.31 * x, 0, 0.95), 0.072, 0.060)
        m.limb(el, wrap, (0.307 * x, 0, 1.02), (0.31 * x, 0, 0.93), 0.068, 0.066)
        m.box(wr, skin, (0.31 * x, -0.005, 0.865), (0.12, 0.13, 0.12), bevel=0.02)
        m.box(wr, wrap, (0.31 * x, -0.06, 0.865), (0.125, 0.03, 0.085))
        if side == "L":
            # Tiger-stripe tattoo sleeve: dark bands around the upper arm
            for i, z in enumerate((1.38, 1.31, 1.24)):
                m.limb(sh, ink, (0.285 * x, 0, z), (0.29 * x, 0, z - 0.025),
                       0.091 - i * 0.004, 0.090 - i * 0.004, cap=False)
            m.blob(sh, ink, (0.29 * x, -0.02, 1.47), (0.16, 0.12, 0.08))

    # ---- legs: baggy trousers tied at the ankle --------------------------
    for side, x in (("L", 1), ("R", -1)):
        hp, kn, an = f"hip_{side}", f"knee_{side}", f"ankle_{side}"
        m.limb(hp, pants, (0.12 * x, 0, 0.96), (0.13 * x, 0, 0.53), 0.14, 0.12)
        m.limb(kn, pants, (0.13 * x, 0, 0.55), (0.13 * x, 0, 0.17), 0.125, 0.085)
        m.limb(kn, wrap, (0.13 * x, 0, 0.19), (0.13 * x, 0, 0.09), 0.07, 0.065)
        m.box(an, skin, (0.13 * x, -0.05, 0.04), (0.11, 0.23, 0.08), bevel=0.02)

    base = guard()
    return m, base


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

def guard(bounce=0.0):
    """Orthodox boxing guard: left foot and left hand lead."""
    return {
        "hips": (0, 0, 12),
        "chest": (8, 0, -10),
        "neck": (2, 0, -2),
        "shoulder_L": (-48, 8, -10),
        "elbow_L": (-112, 0, 0),
        "wrist_L": (0, 0, 0),
        "shoulder_R": (-38, -10, 10),
        "elbow_R": (-128, 0, 0),
        "hip_L": (-18 - bounce, 0, -8),
        "knee_L": (22 + bounce * 2, 0, 0),
        "ankle_L": (-4 - bounce, 0, 0),
        "hip_R": (14 - bounce, 0, 10),
        "knee_R": (24 + bounce * 2, 0, 0),
        "ankle_R": (-14 - bounce, 0, 0),
    }


def mirror(pose):
    """Swap left / right: lets one authored punch serve both hands."""
    out = {}
    for name, (rx, ry, rz) in pose.items():
        if name.endswith("_L"):
            name = name[:-2] + "_R"
        elif name.endswith("_R"):
            name = name[:-2] + "_L"
        out[name] = (rx, -ry, -rz)
    return out


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(phase):
    """Jog with the guard held high. phase 0..1."""
    s = math.sin(phase * 2 * math.pi)
    c = math.cos(phase * 2 * math.pi)

    def leg(sign):
        swing = 34 * s * sign                       # + = leg back
        lift = max(0.0, -c * sign) * 0              # unused, kept simple
        knee = 18 + max(0.0, s * sign) * 62         # back leg folds
        return (swing - 8, 0, 0), (knee + lift, 0, 0), (-10 - 8 * s * sign, 0, 0)

    hl, kl, al = leg(1)
    hr, kr, ar = leg(-1)
    return {
        "hips": (0, 0, 6 * s),
        "chest": (16, 0, -8 * s),
        "neck": (-8, 0, 4 * s),
        "shoulder_L": (-40 - 14 * s, 10, -8),
        "elbow_L": (-118, 0, 0),
        "shoulder_R": (-40 + 14 * s, -10, 8),
        "elbow_R": (-118, 0, 0),
        "hip_L": hl, "knee_L": kl, "ankle_L": al,
        "hip_R": hr, "knee_R": kr, "ankle_R": ar,
    }


def jab_poses():
    g = guard()
    load = with_(g, chest=(8, 0, -4), shoulder_L=(-52, 8, -6),
                 elbow_L=(-120, 0, 0))
    hit = with_(g, chest=(10, 0, -30), hips=(0, 0, 4), neck=(2, 0, 20),
                shoulder_L=(-92, 0, -14), elbow_L=(-4, 0, 0),
                hip_L=(-24, 0, -8), knee_L=(16, 0, 0))
    return g, load, hit


def cross_poses():
    g = guard()
    load = with_(g, chest=(6, 0, -18), hips=(0, 0, 18))
    hit = with_(g, chest=(12, 0, 34), hips=(0, 0, -12), neck=(2, 0, -30),
                shoulder_R=(-94, 0, 16), elbow_R=(-3, 0, 0),
                shoulder_L=(-30, 12, -12), elbow_L=(-130, 0, 0),
                hip_R=(6, 0, -10), knee_R=(14, 0, 0), ankle_R=(-30, 0, 0))
    return g, load, hit


def uppercut_poses():
    """Right uppercut. The left one is the mirror image."""
    g = guard()
    dip = with_(g, chest=(18, -10, 14), hips=(0, 0, 10),
                shoulder_R=(-10, -12, 10), elbow_R=(-95, 0, 0),
                hip_L=(-28, 0, -8), knee_L=(44, 0, 0), ankle_L=(-14, 0, 0),
                hip_R=(4, 0, 10), knee_R=(48, 0, 0), ankle_R=(-30, 0, 0))
    rise = with_(g, chest=(-12, 8, 26), hips=(0, 0, -6), neck=(-14, 0, -10),
                 shoulder_R=(-150, -4, 22), elbow_R=(-62, 0, 0),
                 hip_L=(-14, 0, -8), knee_L=(10, 0, 0),
                 hip_R=(10, 0, 10), knee_R=(8, 0, 0), ankle_R=(-6, 0, 0))
    return g, dip, rise


def slip_poses():
    g = guard()
    duck = with_(g, chest=(30, 26, -18), neck=(-10, 10, 0),
                 hip_L=(-36, 0, -10), knee_L=(62, 0, 0), ankle_L=(-24, 0, 0),
                 hip_R=(-4, 0, 14), knee_R=(58, 0, 0), ankle_R=(-46, 0, 0))
    return g, duck


def tuck():
    return {
        "hips": (0, 0, 0),
        "chest": (52, 0, 0),
        "neck": (30, 0, 0),
        "shoulder_L": (-70, 20, 0), "elbow_L": (-130, 0, 0),
        "shoulder_R": (-70, -20, 0), "elbow_R": (-130, 0, 0),
        "hip_L": (-110, 0, 0), "knee_L": (135, 0, 0), "ankle_L": (-20, 0, 0),
        "hip_R": (-104, 0, 0), "knee_R": (130, 0, 0), "ankle_R": (-20, 0, 0),
    }


def roll_keys(frames):
    """Forward roll in place (the server moves him; the sprite only spins).
    Each frame rotates the tucked body about its own centre."""
    keys = []
    centre = 0.88       # model-space height of the tucked body's centre
    rolling_height = 0.56  # where that centre should sit while rolling
    for f in range(frames + 1):
        t = f / frames
        angle = 360.0 * t
        # Rotate about the body's centre, not the feet: R(a) maps the centre
        # (0, 0, c) to (0, -c sin a, c cos a); offset it back into place.
        a = math.radians(angle)
        tucked = 0.08 < t < 0.92
        target = rolling_height if tucked else centre - 0.05
        off = (0.0, centre * math.sin(a), target - centre * math.cos(a))
        pose = tuck() if tucked else with_(guard(), chest=(30, 0, 0))
        keys.append(Key(t, pose, off, (angle, 0, 0), "linear"))
    return keys


def clips():
    g = guard()
    j0, j1, j2 = jab_poses()
    c0, c1, c2 = cross_poses()
    u0, u1, u2 = uppercut_poses()
    s0, s1 = slip_poses()
    stance = (0, 0, -0.05)
    lunge = (0, -0.06, -0.06)

    idle_keys = []
    for f in range(9):
        b = 4 * math.sin(f / 8 * 2 * math.pi)
        idle_keys.append(Key(f / 8, guard(b), (0, 0, -0.05 - 0.008 * b)))

    run_keys = []
    for f in range(9):
        p = f / 8
        bounce = 0.035 * abs(math.cos(p * 2 * math.pi))
        run_keys.append(Key(p, run_pose(p), (0, -0.02, -0.04 + bounce),
                            ease="linear"))

    hit_back = with_(g, chest=(-16, -6, 14), neck=(-22, 0, 10),
                     shoulder_L=(-20, 30, -10), elbow_L=(-60, 0, 0),
                     shoulder_R=(-20, -30, 10), elbow_R=(-70, 0, 0))

    kneel = with_(g, chest=(26, 0, 0), neck=(30, 0, 0),
                  shoulder_L=(-10, 20, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-10, -20, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-70, 0, -6), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 0, 8), knee_R=(130, 0, 0), ankle_R=(-40, 0, 0))
    lying = {
        "hips": (0, 0, 0), "chest": (-6, 0, 8), "neck": (-10, 0, 20),
        "shoulder_L": (-150, 50, 0), "elbow_L": (-20, 0, 0),
        "shoulder_R": (-40, -70, 0), "elbow_R": (-30, 0, 0),
        "hip_L": (-50, 0, -10), "knee_L": (80, 0, 0), "ankle_L": (-20, 0, 0),
        "hip_R": (-20, 0, 14), "knee_R": (30, 0, 0), "ankle_R": (-10, 0, 0),
    }

    return [
        Clip("idle", 8, 8, True, idle_keys),
        Clip("run", 8, 13, True, run_keys),
        Clip("jab", 5, 20, False, [
            Key(0.0, j0, stance), Key(0.25, j1, stance),
            Key(0.5, j2, lunge, ease="out"), Key(1.0, j0, stance)]),
        Clip("cross", 6, 20, False, [
            Key(0.0, c0, stance), Key(0.2, c1, stance),
            Key(0.45, c2, lunge, ease="out"), Key(0.7, c2, lunge),
            Key(1.0, c0, stance)]),
        Clip("uppercut_R", 6, 22, False, [
            Key(0.0, u0, stance), Key(0.3, u1, (0, -0.02, -0.12)),
            Key(0.6, u2, (0, -0.04, 0.0), ease="out"), Key(1.0, u0, stance)]),
        Clip("uppercut_L", 6, 22, False, [
            Key(0.0, u0, stance), Key(0.3, mirror(u1), (0, -0.02, -0.12)),
            Key(0.6, mirror(u2), (0, -0.04, 0.0), ease="out"),
            Key(1.0, u0, stance)]),
        Clip("slip", 6, 16, False, [
            Key(0.0, s0, stance), Key(0.3, s1, (0.10, 0.04, -0.16), ease="out"),
            Key(0.75, s1, (0.10, 0.04, -0.16)), Key(1.0, s0, stance)]),
        Clip("roll", 8, 18, False, roll_keys(8)),
        Clip("hit", 4, 16, False, [
            Key(0.0, g, stance), Key(0.3, hit_back, (0, 0.07, -0.03), ease="out"),
            Key(1.0, g, stance)]),
        Clip("death", 9, 12, False, [
            Key(0.0, hit_back, (0, 0.04, -0.03)),
            Key(0.35, kneel, (0, 0.06, -0.40)),
            Key(0.7, lying, (0, 0.10, 0.12), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, 0.10, 0.12), rot=(-90, 0, 0))]),
    ]
