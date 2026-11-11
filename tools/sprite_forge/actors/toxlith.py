"""Toxlith: the Venoxarid swarmer.

Knee-high, fast and stupid. A hunched, digitigrade lizard-insect in olive
chitin with a venom-sac hump that glows sickly green, a heavy underbite and
two cyan eyes. Built to read at ~40px: a big head, long claws and a glow
that pops against the ground, so a pack of them is readable at a glance.
"""

import math

from forge import Clip, Key, Model

CELL = (96, 80, 66)   # cell w, h, feet line

CHITIN = (150, 150, 52)
BELLY = (92, 98, 44)
PLATE = (186, 176, 74)
SAC = (140, 236, 84)
EYE = (110, 250, 236)
CLAW = (226, 214, 176)
MOUTH = (60, 24, 40)


def build():
    m = Model("Toxlith")
    chitin = m.material("chitin", CHITIN)
    belly = m.material("belly", BELLY)
    plate = m.material("plate", PLATE)
    sac = m.material("sac", SAC, glow=True)
    eye = m.material("eye", EYE, glow=True)
    claw = m.material("claw", CLAW)
    mouth = m.material("mouth", MOUTH)

    # ---- skeleton (faces -Y) ---------------------------------------------
    m.joint("pelvis", None, (0, 0.04, 0.44))
    m.joint("spine", "pelvis", (0, -0.02, 0.50))
    m.joint("head", "spine", (0, -0.24, 0.66))
    m.joint("jaw", "head", (0, -0.27, 0.62))
    m.joint("tail1", "pelvis", (0, 0.16, 0.42))
    m.joint("tail2", "tail1", (0, 0.34, 0.30))
    for side, x in (("L", 1), ("R", -1)):
        m.joint(f"arm_{side}", "spine", (0.15 * x, -0.14, 0.62))
        m.joint(f"fore_{side}", f"arm_{side}", (0.20 * x, -0.20, 0.47))
        m.joint(f"thigh_{side}", "pelvis", (0.11 * x, 0.04, 0.42))
        m.joint(f"shin_{side}", f"thigh_{side}", (0.13 * x, -0.10, 0.26))
        m.joint(f"foot_{side}", f"shin_{side}", (0.13 * x, 0.06, 0.10))

    # ---- body: a hunched barrel with a glowing venom hump -----------------
    m.blob("pelvis", belly, (0, 0.05, 0.43), (0.26, 0.26, 0.20), segments=8,
           rings=6)
    m.blob("spine", chitin, (0, -0.07, 0.56), (0.30, 0.34, 0.24), rot=(-20, 0, 0),
           segments=9, rings=6)
    m.blob("spine", belly, (0, -0.15, 0.51), (0.22, 0.18, 0.16), segments=8,
           rings=5)
    m.blob("spine", sac, (0, 0.03, 0.67), (0.22, 0.22, 0.16), segments=8,
           rings=6, smooth=True)
    for x in (1, -1):
        m.blob("spine", sac, (0.10 * x, 0.10, 0.60), (0.12, 0.12, 0.10),
               segments=7, rings=5, smooth=True)
    for i, (y, z) in enumerate(((-0.12, 0.68), (-0.02, 0.71), (0.09, 0.70))):
        m.box("spine", plate, (0, y, z + 0.02), (0.13 - i * 0.02, 0.07, 0.04),
              rot=(-20, 0, 0))

    # ---- head: wedge skull, underbite, cyan eyes ---------------------------
    m.blob("head", chitin, (0, -0.31, 0.68), (0.22, 0.26, 0.17), segments=8,
           rings=6)
    m.box("head", plate, (0, -0.31, 0.755), (0.16, 0.20, 0.04), rot=(-8, 0, 0))
    for x in (1, -1):
        m.box("head", eye, (0.075 * x, -0.42, 0.705), (0.05, 0.03, 0.035))
        m.limb("head", plate, (0.08 * x, -0.25, 0.75), (0.13 * x, -0.12, 0.82),
               0.025, 0.008, sides=5)                         # horns
    m.box("jaw", belly, (0, -0.37, 0.60), (0.18, 0.18, 0.06))
    m.box("jaw", mouth, (0, -0.40, 0.635), (0.13, 0.10, 0.02))
    for x in (1, -1):
        m.box("jaw", claw, (0.05 * x, -0.45, 0.645), (0.025, 0.02, 0.04))

    # ---- tail ---------------------------------------------------------------
    m.limb("tail1", chitin, (0, 0.14, 0.43), (0, 0.34, 0.31), 0.08, 0.055)
    m.limb("tail2", chitin, (0, 0.34, 0.31), (0, 0.50, 0.20), 0.055, 0.02,
           sides=6)
    m.blob("tail2", sac, (0, 0.47, 0.22), (0.07, 0.07, 0.07), smooth=True)

    # ---- arms: long, ending in big hooked claws ------------------------------
    for side, x in (("L", 1), ("R", -1)):
        a, f = f"arm_{side}", f"fore_{side}"
        m.blob(a, chitin, (0.15 * x, -0.14, 0.62), (0.11, 0.11, 0.11))
        m.limb(a, chitin, (0.15 * x, -0.14, 0.62), (0.20 * x, -0.20, 0.47),
               0.05, 0.04, sides=6)
        m.limb(f, belly, (0.20 * x, -0.20, 0.47), (0.21 * x, -0.29, 0.36),
               0.04, 0.03, sides=6)
        for k in (-1, 0, 1):
            m.limb(f, claw, (0.21 * x + 0.02 * k, -0.29, 0.36),
                   (0.21 * x + 0.035 * k, -0.37, 0.27), 0.016, 0.004, sides=4)

    # ---- legs: digitigrade ---------------------------------------------------
    for side, x in (("L", 1), ("R", -1)):
        t, s, ft = f"thigh_{side}", f"shin_{side}", f"foot_{side}"
        m.limb(t, chitin, (0.11 * x, 0.04, 0.44), (0.13 * x, -0.10, 0.26),
               0.07, 0.05, sides=6)
        m.limb(s, belly, (0.13 * x, -0.10, 0.26), (0.13 * x, 0.06, 0.10),
               0.045, 0.035, sides=6)
        m.limb(ft, chitin, (0.13 * x, 0.06, 0.10), (0.13 * x, -0.02, 0.02),
               0.035, 0.03, sides=5)
        for k in (-1, 1):
            m.limb(ft, claw, (0.13 * x + 0.025 * k, -0.02, 0.02),
                   (0.13 * x + 0.04 * k, -0.11, 0.01), 0.014, 0.004, sides=4)

    return m, rest()


def rest(breath=0.0):
    return {
        "pelvis": (0, 0, 0),
        "spine": (4 + breath * 3, 0, 0),
        "head": (-6 - breath * 4, 0, 0),
        "jaw": (breath * 4, 0, 0),
        "arm_L": (-10, 0, 6), "fore_L": (-10, 0, 0),
        "arm_R": (-10, 0, -6), "fore_R": (-10, 0, 0),
        "thigh_L": (0, 0, -4), "shin_L": (0, 0, 0), "foot_L": (0, 0, 0),
        "thigh_R": (0, 0, 4), "shin_R": (0, 0, 0), "foot_R": (0, 0, 0),
        "tail1": (-6 + breath * 6, 0, breath * 8), "tail2": (10, 0, breath * 10),
    }


def run_pose(p):
    s = math.sin(p * 2 * math.pi)
    c = math.cos(p * 2 * math.pi)
    pose = rest()
    pose.update({
        "pelvis": (0, 0, 8 * s),
        "spine": (16, 0, -6 * s),
        "head": (-14, 0, 6 * s),
        "jaw": (10, 0, 0),
        "arm_L": (-30 + 30 * s, 0, 10), "fore_L": (-30, 0, 0),
        "arm_R": (-30 - 30 * s, 0, -10), "fore_R": (-30, 0, 0),
        "thigh_L": (-34 * s, 0, -4), "shin_L": (max(0, c) * -30, 0, 0),
        "foot_L": (20 * s, 0, 0),
        "thigh_R": (34 * s, 0, 4), "shin_R": (max(0, -c) * -30, 0, 0),
        "foot_R": (-20 * s, 0, 0),
        "tail1": (-10, 0, -14 * s), "tail2": (16, 0, -18 * s),
    })
    return pose


def clips():
    idle = []
    for f in range(7):
        b = math.sin(f / 6 * 2 * math.pi)
        idle.append(Key(f / 6, rest(b), (0, 0, 0.006 * b)))

    run = []
    for f in range(7):
        p = f / 6
        run.append(Key(p, run_pose(p),
                       (0, 0, 0.03 * abs(math.cos(p * 2 * math.pi))),
                       ease="linear"))

    r = rest()
    coil = dict(r)
    coil.update({"spine": (-12, 0, 0), "head": (-26, 0, 0), "jaw": (34, 0, 0),
                 "arm_L": (-110, 0, 30), "fore_L": (-50, 0, 0),
                 "arm_R": (-110, 0, -30), "fore_R": (-50, 0, 0),
                 "thigh_L": (14, 0, -4), "shin_L": (-30, 0, 0),
                 "thigh_R": (14, 0, 4), "shin_R": (-30, 0, 0),
                 "tail1": (-24, 0, 0), "tail2": (-10, 0, 0)})
    strike = dict(r)
    strike.update({"spine": (36, 0, 0), "head": (8, 0, 0), "jaw": (40, 0, 0),
                   "arm_L": (-80, 0, -12), "fore_L": (40, 0, 0),
                   "arm_R": (-80, 0, 12), "fore_R": (40, 0, 0),
                   "thigh_L": (-30, 0, -4), "thigh_R": (-24, 0, 4),
                   "shin_L": (10, 0, 0), "tail1": (16, 0, 0)})
    hurt = dict(r)
    hurt.update({"spine": (-24, 0, 10), "head": (-30, 0, 14), "jaw": (30, 0, 0),
                 "arm_L": (-40, 0, 40), "arm_R": (-40, 0, -40),
                 "tail1": (14, 0, 10)})
    dead = dict(r)
    dead.update({"spine": (10, 0, 0), "head": (20, 0, 20), "jaw": (36, 0, 0),
                 "arm_L": (-60, 0, 60), "fore_L": (40, 0, 0),
                 "arm_R": (-20, 0, -50), "fore_R": (30, 0, 0),
                 "thigh_L": (-80, 0, -20), "shin_L": (-60, 0, 0),
                 "thigh_R": (-60, 0, 20), "shin_R": (-40, 0, 0),
                 "tail1": (10, 0, 20), "tail2": (0, 0, 30)})

    return [
        Clip("idle", 6, 8, True, idle),
        Clip("run", 6, 14, True, run),
        # The windup is the first half: the client holds it while the
        # server's telegraph runs, then plays the strike.
        Clip("attack", 7, 14, False, [
            Key(0.0, r), Key(0.45, coil, (0, 0.06, -0.02)),
            Key(0.6, strike, (0, -0.12, 0.02), ease="out"),
            Key(0.8, strike, (0, -0.12, 0.0)), Key(1.0, r)]),
        Clip("hit", 3, 16, False, [
            Key(0.0, r), Key(0.4, hurt, (0, 0.06, 0), ease="out"), Key(1.0, r)]),
        Clip("death", 7, 12, False, [
            Key(0.0, hurt, (0, 0.04, 0)),
            Key(0.5, dead, (0, 0.06, 0.10), rot=(-60, 0, 20), ease="in"),
            Key(1.0, dead, (0, 0.08, 0.16), rot=(-92, 0, 30))]),
    ]
