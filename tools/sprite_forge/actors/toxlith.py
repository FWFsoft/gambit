"""Toxlith: the Venoxarid swarmer.

Knee-high, fast and stupid: a squat, warty toad-lizard in acid green with
a crest of magenta spines along its back, venom beading on the tips, red
eyes and a long pink tongue lolling out of a wide mouth.

Shape brief: low and wide, the opposite of every hero. The spiky crest is
the top of its outline from any side; the tongue hangs out front.
"""

import math

from forge import Clip, Key, Model

CELL = (96, 88, 72)   # cell w, h, feet line

SKIN = (122, 204, 58)
WART = (66, 138, 44)
BELLY = (196, 222, 112)
CREST = (206, 36, 112)
ORB = (255, 96, 176)
TONGUE = (236, 104, 148)
EYE = (240, 46, 56)
CLAW = (226, 214, 176)
MOUTH = (72, 18, 42)


def build():
    m = Model("Toxlith")
    skin = m.material("skin", SKIN)
    wart = m.material("wart", WART)
    belly = m.material("belly", BELLY)
    crest = m.material("crest", CREST)
    orb = m.material("orb", ORB, glow=True)
    tongue = m.material("tongue", TONGUE)
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

    # ---- body: a squat, lumpy barrel --------------------------------------
    m.blob("pelvis", skin, (0, 0.06, 0.42), (0.34, 0.30, 0.22), segments=9,
           rings=6)
    m.blob("spine", skin, (0, -0.06, 0.55), (0.40, 0.40, 0.27), rot=(-14, 0, 0),
           segments=10, rings=7)
    m.blob("spine", belly, (0, -0.17, 0.47), (0.28, 0.20, 0.18), segments=8,
           rings=5)
    # Warts: darker lumps scattered over the back and flanks.
    warts = ((0.12, -0.10, 0.70, 0.07), (-0.13, -0.02, 0.70, 0.06),
             (0.19, 0.05, 0.62, 0.06), (-0.20, -0.12, 0.60, 0.07),
             (0.06, 0.10, 0.68, 0.05), (-0.07, -0.20, 0.67, 0.05),
             (0.22, -0.16, 0.54, 0.05), (-0.23, 0.06, 0.53, 0.05),
             (0.16, 0.16, 0.50, 0.05), (-0.15, 0.17, 0.52, 0.05))
    for x, y, z, r in warts:
        m.blob("spine", wart, (x, y, z), (r, r, r * 0.8), segments=6, rings=4)
    # The crest: magenta spines down the back, venom beading on the tips.
    spines = ((-0.20, 0.70, 0.16, -12), (-0.08, 0.76, 0.22, -4),
              (0.04, 0.76, 0.20, 6), (0.15, 0.70, 0.16, 14),
              (0.25, 0.62, 0.12, 22))
    for k, (y, z, h, lean) in enumerate(spines):
        tip_y = y + h * math.sin(math.radians(lean))
        tip_z = z + h * math.cos(math.radians(lean))
        m.limb("spine", crest, (0, y, z - 0.04), (0, tip_y, tip_z), 0.05, 0.008,
               sides=5)
        for x in (1, -1):
            m.limb("spine", crest, (0.05 * x, y, z - 0.05),
                   (0.12 * x, tip_y, tip_z - 0.05), 0.03, 0.006, sides=4)
        m.blob("spine", orb, (0, tip_y, tip_z + 0.02),
               (0.065, 0.065, 0.065) if k % 2 == 0 else (0.045, 0.045, 0.045),
               smooth=True)

    # ---- head: a wide toad head, mouth open, tongue out --------------------
    m.blob("head", skin, (0, -0.31, 0.66), (0.30, 0.28, 0.16), segments=9,
           rings=6)
    for x in (1, -1):
        m.blob("head", skin, (0.10 * x, -0.30, 0.76), (0.10, 0.10, 0.09))
        m.blob("head", eye, (0.12 * x, -0.36, 0.80), (0.075, 0.06, 0.065),
               smooth=True)
        m.blob("head", wart, (0.15 * x, -0.24, 0.72), (0.05, 0.05, 0.04),
               segments=6, rings=4)
    m.box("head", mouth, (0, -0.42, 0.63), (0.24, 0.06, 0.05))
    m.box("jaw", belly, (0, -0.36, 0.56), (0.28, 0.20, 0.06))
    m.box("jaw", mouth, (0, -0.40, 0.595), (0.22, 0.12, 0.02))
    # The tongue lolls out over the lower jaw and hangs, with a venom drop.
    m.limb("jaw", tongue, (0, -0.40, 0.60), (0.02, -0.50, 0.55), 0.045, 0.04,
           sides=6)
    m.limb("jaw", tongue, (0.02, -0.50, 0.55), (0.03, -0.53, 0.40), 0.04, 0.03,
           sides=6)
    m.blob("jaw", orb, (0.03, -0.53, 0.37), (0.035, 0.035, 0.04), smooth=True)
    for x in (1, -1):
        m.box("jaw", claw, (0.09 * x, -0.45, 0.61), (0.025, 0.02, 0.04))

    # ---- tail: a short, thick club --------------------------------------------
    m.limb("tail1", skin, (0, 0.16, 0.42), (0, 0.34, 0.31), 0.11, 0.08)
    m.limb("tail2", skin, (0, 0.34, 0.31), (0, 0.48, 0.22), 0.08, 0.03,
           sides=6)
    m.blob("tail2", wart, (0, 0.30, 0.38), (0.05, 0.05, 0.04), segments=6,
           rings=4)

    # ---- arms: thick and short, splayed, with hooked claws -------------------
    for side, x in (("L", 1), ("R", -1)):
        a, f = f"arm_{side}", f"fore_{side}"
        m.blob(a, skin, (0.17 * x, -0.14, 0.60), (0.14, 0.14, 0.13))
        m.limb(a, skin, (0.17 * x, -0.14, 0.60), (0.22 * x, -0.20, 0.47),
               0.075, 0.06, sides=6)
        m.limb(f, belly, (0.22 * x, -0.20, 0.47), (0.23 * x, -0.27, 0.34),
               0.06, 0.05, sides=6)
        for k in (-1, 0, 1):
            m.limb(f, claw, (0.23 * x + 0.025 * k, -0.27, 0.34),
                   (0.23 * x + 0.04 * k, -0.35, 0.27), 0.018, 0.004, sides=4)

    # ---- legs: frog-thick haunches ----------------------------------------------
    for side, x in (("L", 1), ("R", -1)):
        t, sh, ft = f"thigh_{side}", f"shin_{side}", f"foot_{side}"
        m.blob(t, skin, (0.17 * x, 0.04, 0.38), (0.15, 0.17, 0.14))
        m.limb(t, skin, (0.13 * x, 0.04, 0.44), (0.15 * x, -0.10, 0.26),
               0.10, 0.07, sides=6)
        m.limb(sh, belly, (0.15 * x, -0.10, 0.26), (0.15 * x, 0.06, 0.10),
               0.06, 0.05, sides=6)
        m.limb(ft, skin, (0.15 * x, 0.06, 0.10), (0.15 * x, -0.03, 0.02),
               0.05, 0.04, sides=5)
        for k in (-1, 1):
            m.limb(ft, claw, (0.15 * x + 0.03 * k, -0.03, 0.02),
                   (0.15 * x + 0.05 * k, -0.12, 0.01), 0.016, 0.004, sides=4)

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
