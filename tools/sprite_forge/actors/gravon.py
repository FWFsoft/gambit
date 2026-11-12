"""Gravon, the Undying Blade. Modelled from assets/previews/Gravon.png.

A lean, shirtless swordsman with wild dark hair and a face lit violet from
within, baggy torn navy trousers, bare feet, bandaged forearms with spiked
bracers, and a cleaver of a sword longer than he is tall: dark iron, a
blocky crossguard around a red eye, glowing red runes down the flat.

Shape brief: the blade. Gravon *is* the sword; the man is only its host.
At rest the blade runs straight down his spine: pommel and grip above his
head, crossguard across his shoulders, the blade tapering past his legs, so
his whole silhouette reads as a sword. The host slumps beneath it, head
bowed, arms hanging: a vessel the blade is wearing. It is drawn only to
swing.
"""

import math

from forge import Clip, Key, Model

PIXEL_SCALE = 1.35  # chunkier art pixels, same size on screen (see forge.ActorSpec)
NAME = "Gravon"
CELL = (220, 256, 200)

SKIN = (214, 120, 70)
HAIR = (40, 30, 44)
FACE = (176, 70, 255)
PANTS = (58, 62, 112)
RAGS = (200, 150, 104)
WRAP = (224, 210, 184)
SPIKE = (96, 88, 96)
IRON = (74, 62, 78)
EDGE = (150, 120, 150)
RUNE = (255, 70, 110)
CORE = (255, 60, 50)


def build():
    m = Model("Gravon")
    skin = m.material("skin", SKIN)
    hair = m.material("hair", HAIR)
    face = m.material("face", FACE, glow=True)
    pants = m.material("pants", PANTS)
    rags = m.material("rags", RAGS)
    wrap = m.material("wrap", WRAP)
    spike = m.material("spike", SPIKE)
    iron = m.material("iron", IRON)
    edge = m.material("edge", EDGE)
    rune = m.material("rune", RUNE, glow=True)
    core = m.material("core", CORE, glow=True)

    # ---- skeleton: tall and lean -------------------------------------------
    m.joint("hips", None, (0, 0, 1.00))
    m.joint("chest", "hips", (0, 0, 1.12))
    m.joint("neck", "chest", (0, 0, 1.58))
    for side, x in (("L", 1), ("R", -1)):
        m.joint(f"shoulder_{side}", "chest", (0.27 * x, 0, 1.50))
        m.joint(f"elbow_{side}", f"shoulder_{side}", (0.30 * x, 0, 1.20))
        m.joint(f"wrist_{side}", f"elbow_{side}", (0.31 * x, 0, 0.94))
        m.joint(f"hip_{side}", "hips", (0.12 * x, 0, 0.96))
        m.joint(f"knee_{side}", f"hip_{side}", (0.13 * x, 0, 0.53))
        m.joint(f"ankle_{side}", f"knee_{side}", (0.13 * x, 0, 0.10))
    # The sword has its own joint in the right fist, so poses can aim it,
    # and a twin worn down his spine. Poses show one and hide the other
    # (a fourth value of 0 scales a joint away).
    m.joint("sword", "wrist_R", (-0.31, -0.02, 0.86))
    m.joint("back", "chest", (0, 0.20, 1.40))

    # ---- torso: lean, V-shaped, bare ------------------------------------------
    m.blob("chest", skin, (0, 0, 1.18), (0.26, 0.20, 0.22), segments=8, rings=6)
    m.blob("chest", skin, (0, 0.01, 1.37), (0.44, 0.25, 0.26), segments=10,
           rings=7)
    for x in (1, -1):
        m.blob("chest", skin, (0.09 * x, -0.10, 1.40), (0.18, 0.08, 0.13),
               segments=8, rings=5)
        m.blob("chest", skin, (0.14 * x, 0.04, 1.50), (0.16, 0.14, 0.09))

    # ---- head: wild hair, a face lit from within -----------------------------------
    m.limb("neck", skin, (0, 0, 1.52), (0, -0.01, 1.62), 0.07, 0.065)
    m.blob("neck", skin, (0, -0.02, 1.72), (0.21, 0.22, 0.25), segments=9,
           rings=7)
    m.box("neck", face, (0, -0.115, 1.72), (0.15, 0.03, 0.16))
    for i, (x, y, z, rx, rz) in enumerate(((0, 0.02, 1.86, -10, 0),
                                           (0.10, 0.06, 1.83, -30, -30),
                                           (-0.10, 0.06, 1.83, -30, 30),
                                           (0, 0.12, 1.78, -60, 0),
                                           (0.06, -0.08, 1.84, 20, -20))):
        m.limb("neck", hair, (x, y, z - 0.06), (x * 1.8, y * 1.8 + 0.04,
                                                 z + 0.12),
               0.07, 0.01, sides=5)
    m.blob("neck", hair, (0, 0.03, 1.80), (0.23, 0.24, 0.12))

    # ---- arms: bandaged forearms, spiked bracers --------------------------------------
    for side, x in (("L", 1), ("R", -1)):
        sh, el, wr = f"shoulder_{side}", f"elbow_{side}", f"wrist_{side}"
        m.blob(sh, skin, (0.27 * x, 0, 1.49), (0.17, 0.17, 0.16))
        m.limb(sh, skin, (0.28 * x, 0, 1.46), (0.30 * x, 0, 1.20), 0.085, 0.07)
        m.limb(el, wrap, (0.30 * x, 0, 1.20), (0.31 * x, 0, 0.96), 0.075, 0.065)
        m.limb(el, spike, (0.305 * x, 0, 1.06), (0.31 * x, 0, 0.98), 0.085,
               0.085)
        for a in range(4):
            ang = a * math.pi / 2 + 0.4
            m.limb(el, spike, (0.31 * x + 0.06 * math.cos(ang), 0.06 *
                               math.sin(ang), 1.02),
                   (0.31 * x + 0.14 * math.cos(ang), 0.14 * math.sin(ang), 1.03),
                   0.025, 0.004, sides=4)
        m.box(wr, skin, (0.31 * x, -0.005, 0.87), (0.11, 0.12, 0.11), bevel=0.02)

    # ---- legs: baggy, torn navy trousers, bare feet ---------------------------------------
    m.box("hips", pants, (0, 0, 0.96), (0.36, 0.26, 0.20))
    m.box("hips", rags, (0, -0.13, 1.02), (0.30, 0.03, 0.05))
    for side, x in (("L", 1), ("R", -1)):
        hp, kn, an = f"hip_{side}", f"knee_{side}", f"ankle_{side}"
        m.limb(hp, pants, (0.12 * x, 0, 0.96), (0.13 * x, 0, 0.53), 0.15, 0.15)
        m.limb(kn, pants, (0.13 * x, 0, 0.55), (0.13 * x, 0, 0.22), 0.16, 0.18)
        m.limb(kn, rags, (0.13 * x, 0, 0.26), (0.13 * x, 0, 0.17), 0.185, 0.19,
               cap=False)
        m.box(an, skin, (0.13 * x, -0.05, 0.04), (0.11, 0.24, 0.07), bevel=0.02)

    # ---- the sword: along -Y from the fist in the rest pose -----------------------------
    m.limb("sword", iron, (-0.31, 0.16, 0.86), (-0.31, -0.06, 0.86), 0.035,
           0.035, sides=6)                                           # grip
    m.box("sword", iron, (-0.31, 0.20, 0.86), (0.06, 0.05, 0.06))   # pommel
    m.box("sword", iron, (-0.31, -0.10, 0.86), (0.10, 0.08, 0.40))  # guard
    m.box("sword", iron, (-0.31, -0.10, 1.06), (0.10, 0.10, 0.08))
    m.box("sword", iron, (-0.31, -0.10, 0.66), (0.10, 0.10, 0.08))
    m.blob("sword", core, (-0.31, -0.12, 0.86), (0.07, 0.07, 0.07),
           smooth=True)
    # The blade: a slab, wider than his torso is deep, longer than he is tall.
    m.box("sword", iron, (-0.31, -1.10, 0.86), (0.06, 1.80, 0.30))
    m.box("sword", edge, (-0.31, -1.10, 0.71), (0.04, 1.80, 0.03))
    m.box("sword", iron, (-0.31, -2.06, 0.80), (0.06, 0.14, 0.18),
          rot=(30, 0, 0))                                            # clipped tip
    for i in range(5):
        y = -0.40 - i * 0.30
        for x in (0.032, -0.032):
            m.box("sword", rune, (-0.31 + x, y, 0.88 + (0.05 if i % 2 else -0.04)),
                  (0.012, 0.16, 0.05))

    # ---- the same sword worn on his back, point down along the spine ----------
    # The flat faces backward, so from behind (and above his shoulders from the
    # front) it shows its full width.
    by = 0.24
    m.box("back", iron, (0, by, 2.30), (0.07, 0.07, 0.07))            # pommel
    m.limb("back", iron, (0, by, 2.27), (0, by, 1.94), 0.04, 0.04, sides=6)
    m.box("back", iron, (0, by, 1.90), (0.64, 0.12, 0.10))            # guard
    for x in (1, -1):
        m.box("back", iron, (0.30 * x, by, 1.90), (0.08, 0.14, 0.20))  # lugs
    m.blob("back", core, (0, by + 0.07, 1.90), (0.08, 0.06, 0.08), smooth=True)
    m.box("back", iron, (0, by, 1.00), (0.32, 0.06, 1.76))            # blade
    m.box("back", edge, (-0.155, by, 1.00), (0.03, 0.04, 1.76))
    m.box("back", iron, (0.06, by, 0.07), (0.20, 0.06, 0.14), rot=(0, 30, 0))
    for i in range(5):
        z = 1.70 - i * 0.30
        m.box("back", rune, ((0.05 if i % 2 else -0.04), by + 0.032, z),
              (0.05, 0.012, 0.16))

    return m, stance()


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

def stance(sway=0.0):
    """The host at rest: slumped under the blade on his back, head bowed,
    arms hanging, feet close together. The sword is the one standing tall."""
    return {
        "hips": (0, 0, 4),
        "chest": (12 + sway, 0, -3),
        "neck": (24 + sway, 0, 6),
        "back": (-12 - sway, 0, 0, 1),
        "sword": (0, 0, 0, 0),
        "shoulder_R": (-4, 6, 0), "elbow_R": (-12, 0, 0),
        "shoulder_L": (-4, -6, 0), "elbow_L": (-14, 0, 0),
        "hip_L": (-6, -2, -4), "knee_L": (10, 0, 0), "ankle_L": (-4, 0, 0),
        "hip_R": (2, 2, 4), "knee_R": (12, 0, 0), "ankle_R": (-8, 0, 0),
    }


def armed(pose):
    """Sword in hand, back empty."""
    p = dict(pose)
    p["back"] = (0, 0, 0, 0)
    sword = p.get("sword", (0, 0, 0))
    p["sword"] = tuple(sword[:3]) + (1,)
    return p


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(phase):
    s = math.sin(phase * 2 * math.pi)
    p = stance()
    p.update({
        "hips": (0, 0, 8 * s),
        "chest": (14, 0, -8 * s),
        "neck": (10, 0, 0),
        "shoulder_L": (-20 - 35 * s, -10, 0), "elbow_L": (-60, 0, 0),
        "shoulder_R": (-20 + 35 * s, 10, 0), "elbow_R": (-60, 0, 0),
        "hip_L": (34 * s - 8, -4, 0), "knee_L": (18 + max(0.0, s) * 70, 0, 0),
        "ankle_L": (-10, 0, 0),
        "hip_R": (-34 * s - 8, 4, 0), "knee_R": (18 + max(0.0, -s) * 70, 0, 0),
        "ankle_R": (-10, 0, 0),
    })
    return p


def clips():
    s = stance()
    idle = [Key(f / 8, stance(2 * math.sin(f / 8 * 2 * math.pi)),
                (0, 0, -0.02 - 0.008 * math.sin(f / 8 * 2 * math.pi)))
            for f in range(9)]
    run = [Key(f / 8, run_pose(f / 8),
               (0, -0.02, -0.03 + 0.04 * abs(math.cos(f / 8 * 2 * math.pi))),
               ease="linear") for f in range(9)]

    # Titan's Slash: the blade held out level and swept across his front.
    level = dict(shoulder_R=(-88, 0, 0), elbow_R=(-6, 0, 0), sword=(94, 0, 0))
    wind_a = with_(s, chest=(4, 0, 60), hips=(0, 0, 30), **level,
                   shoulder_L=(-30, -20, 0), elbow_L=(-60, 0, 0))
    wind_a["shoulder_R"] = (-84, 0, 50)
    cut_a = with_(s, chest=(12, 0, -70), hips=(0, 0, -30), **level,
                  shoulder_L=(-10, -40, 0), elbow_L=(-30, 0, 0),
                  hip_L=(-30, -6, -6), knee_L=(30, 0, 0))
    cut_a["shoulder_R"] = (-84, 0, -40)
    # The backhand return.
    wind_b = with_(cut_a)
    cut_b = with_(wind_a, chest=(12, 0, 70), hips=(0, 0, 30),
                  hip_R=(-24, 6, 6), knee_R=(28, 0, 0))
    # Ravager's Edge: raised straight up, then brought down through the floor.
    raise_ = with_(s, chest=(-14, 0, 0), neck=(-10, 0, 0),
                   shoulder_R=(-170, 10, 0), elbow_R=(-20, 0, 0),
                   sword=(100, 0, 0), shoulder_L=(-150, -20, 0),
                   elbow_L=(-40, 0, 0))
    smash = with_(s, chest=(42, 0, 0), neck=(20, 0, 0),
                  shoulder_R=(-70, 0, 0), elbow_R=(-4, 0, 0), sword=(110, 0, 0),
                  shoulder_L=(-60, -10, 0), elbow_L=(-20, 0, 0),
                  hip_L=(-60, -6, 0), knee_L=(80, 0, 0), ankle_L=(-20, 0, 0),
                  hip_R=(10, 6, 0), knee_R=(40, 0, 0))
    # Fury of the Blade: the sword thrust skyward, head thrown back.
    fury = with_(s, chest=(-18, 0, 0), neck=(-30, 0, 0),
                 shoulder_R=(-178, 6, 0), elbow_R=(-4, 0, 0), sword=(92, 0, 0),
                 shoulder_L=(-20, -50, 0), elbow_L=(-90, 0, 0),
                 hip_L=(-10, -14, 0), hip_R=(-10, 14, 0))
    # Blade Rush: a low lunge, blade levelled like a lance.
    lunge = with_(s, chest=(30, 0, 10), neck=(-10, 0, 0),
                  shoulder_R=(-80, 0, 10), elbow_R=(-4, 0, 0), sword=(84, 0, 0),
                  shoulder_L=(10, -30, 0), elbow_L=(-20, 0, 0),
                  hip_L=(-70, -4, 0), knee_L=(70, 0, 0), ankle_L=(-10, 0, 0),
                  hip_R=(40, 4, 0), knee_R=(10, 0, 0), ankle_R=(-30, 0, 0))
    wind_a, cut_a, wind_b, cut_b = (armed(wind_a), armed(cut_a), armed(wind_b),
                                    armed(cut_b))
    raise_, smash, fury, lunge = (armed(raise_), armed(smash), armed(fury),
                                  armed(lunge))
    hurt = with_(s, chest=(-18, -6, 10), neck=(-20, 0, 0),
                 shoulder_L=(-20, -50, 0), elbow_L=(-30, 0, 0))
    kneel = with_(s, chest=(30, 0, 0), neck=(30, 0, 0),
                  shoulder_R=(-40, 20, 0), elbow_R=(-20, 0, 0),
                  shoulder_L=(-10, -20, 0), elbow_L=(-30, 0, 0),
                  hip_L=(-80, -6, 0), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 6, 0), knee_R=(120, 0, 0), ankle_R=(-40, 0, 0))
    lying = {"hips": (0, 0, 0), "chest": (-4, 0, 6), "neck": (-10, 0, 14),
             "shoulder_L": (-150, -60, 0), "elbow_L": (-20, 0, 0),
             "shoulder_R": (-40, 70, 0), "elbow_R": (-30, 0, 0),
             "sword": (0, 0, 0, 0), "back": (0, 0, 0, 1),
             "hip_L": (-40, -14, 0), "knee_L": (60, 0, 0),
             "hip_R": (-20, 14, 0), "knee_R": (30, 0, 0)}

    return [
        Clip("idle", 8, 8, True, idle),
        Clip("run", 8, 12, True, run),
        Clip("slash_a", 6, 18, False, [
            Key(0.0, s), Key(0.3, wind_a, (0, 0.02, -0.02), ease="out"),
            Key(0.6, cut_a, (0, -0.08, -0.06), ease="in"),
            Key(0.8, cut_a, (0, -0.08, -0.06)), Key(1.0, s)]),
        Clip("slash_b", 6, 18, False, [
            Key(0.0, wind_b, (0, -0.04, -0.04)),
            Key(0.55, cut_b, (0, -0.08, -0.06), ease="in"),
            Key(0.8, cut_b, (0, -0.08, -0.06)), Key(1.0, s)]),
        Clip("ravager", 8, 14, False, [
            Key(0.0, s), Key(0.35, raise_, (0, 0.04, 0.04), ease="out"),
            Key(0.6, smash, (0, -0.12, -0.18), ease="in"),
            Key(0.85, smash, (0, -0.12, -0.18)), Key(1.0, s)]),
        Clip("fury", 8, 12, False, [
            Key(0.0, s), Key(0.35, fury, (0, 0, 0.04), ease="out"),
            Key(0.75, with_(fury, chest=(-22, 0, 0)), (0, 0, 0.05)),
            Key(1.0, s)]),
        Clip("rush", 6, 18, False, [
            Key(0.0, s), Key(0.25, lunge, (0, -0.14, -0.14), ease="out"),
            Key(0.75, lunge, (0, -0.16, -0.14)), Key(1.0, s)]),
        Clip("hit", 4, 16, False, [
            Key(0.0, s), Key(0.3, hurt, (0, 0.06, -0.02), ease="out"),
            Key(1.0, s)]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, (0, 0.04, -0.02)),
            Key(0.35, kneel, (0, 0.06, -0.44)),
            Key(0.7, lying, (0, 0.10, 0.16), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, 0.10, 0.16), rot=(-90, 0, 0))]),
    ]
