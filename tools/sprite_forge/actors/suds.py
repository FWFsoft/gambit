"""Suds Sullivan, the Brew Brawler. Modelled from assets/previews/Suds.png.

A big, grinning bruiser: grey flat-top, orange shades, a blue Hawaiian
shirt open over a white tank, an orange strap across the chest, red
power-armour gauntlets and boots, a tool belt, a battered white cooler
slung at his hip and a brew can always in hand.

Shape brief: the barrel and the cooler. A wide barrel of a torso on armoured
legs; the cooler juts out at his left hip and the can is raised high in his
right hand, so his outline is lopsided from every side, unlike any other hero.
"""

import math

from forge import Clip, Key, Model

PIXEL_SCALE = 1.35  # chunkier art pixels, same size on screen (see forge.ActorSpec)
NAME = "Suds"
CELL = (176, 176, 150)

SKIN = (226, 146, 92)
HAIR = (196, 198, 204)
SHADES = (255, 112, 36)
SHIRT = (58, 148, 214)
FLOWER = (238, 234, 214)
PETAL = (242, 120, 58)
TANK = (232, 228, 216)
STRAP = (226, 112, 38)
ARMOR = (212, 62, 34)
ARMOR_DARK = (96, 64, 66)
BELT = (122, 92, 58)
POUCH = (176, 132, 70)
CAN = (210, 214, 222)
LABEL = (240, 204, 58)
COOLER = (234, 226, 206)
COOLER_TRIM = (170, 66, 48)


def build():
    m = Model("Suds")
    skin = m.material("skin", SKIN)
    hair = m.material("hair", HAIR)
    shades = m.material("shades", SHADES, glow=True)
    shirt = m.material("shirt", SHIRT)
    flower = m.material("flower", FLOWER)
    petal = m.material("petal", PETAL)
    tank = m.material("tank", TANK)
    strap = m.material("strap", STRAP)
    armor = m.material("armor", ARMOR)
    armor_dark = m.material("armor_dark", ARMOR_DARK)
    belt = m.material("belt", BELT)
    pouch = m.material("pouch", POUCH)
    can = m.material("can", CAN)
    label = m.material("label", LABEL)
    cooler = m.material("cooler", COOLER)
    trim = m.material("cooler_trim", COOLER_TRIM)

    # ---- skeleton: short legs, a huge barrel ---------------------------------
    m.joint("hips", None, (0, 0, 0.96))
    m.joint("chest", "hips", (0, 0, 1.08))
    m.joint("neck", "chest", (0, 0, 1.64))
    for side, x in (("L", 1), ("R", -1)):
        m.joint(f"shoulder_{side}", "chest", (0.40 * x, 0, 1.56))
        m.joint(f"elbow_{side}", f"shoulder_{side}", (0.46 * x, 0, 1.27))
        m.joint(f"wrist_{side}", f"elbow_{side}", (0.47 * x, 0, 1.00))
        m.joint(f"hip_{side}", "hips", (0.18 * x, 0, 0.92))
        m.joint(f"knee_{side}", f"hip_{side}", (0.20 * x, 0, 0.52))
        m.joint(f"ankle_{side}", f"knee_{side}", (0.20 * x, 0, 0.12))

    # ---- torso: a barrel in a Hawaiian shirt ------------------------------------
    m.blob("chest", shirt, (0, 0.0, 1.32), (0.78, 0.56, 0.62), segments=11,
           rings=8)
    m.box("chest", tank, (0, -0.25, 1.36), (0.26, 0.08, 0.44), taper=0.9)
    for x in (1, -1):
        m.box("chest", shirt, (0.18 * x, -0.27, 1.50), (0.10, 0.05, 0.18),
              rot=(0, 18 * x, 0))                                   # collar
    # Flowers: pale blossoms with orange hearts scattered over the shirt.
    blossoms = ((0.26, -0.24, 1.22), (-0.30, -0.22, 1.42), (0.30, -0.18, 1.50),
                (-0.22, -0.25, 1.15), (0.36, 0.0, 1.34), (-0.38, 0.02, 1.26),
                (0.12, 0.27, 1.44), (-0.16, 0.27, 1.22), (0.30, 0.20, 1.18),
                (-0.30, 0.18, 1.50))
    for x, y, z in blossoms:
        m.box("chest", flower, (x, y * 1.04, z), (0.09, 0.03, 0.09))
        m.box("chest", petal, (x, y * 1.08, z), (0.035, 0.03, 0.035))
    # The strap across the chest, shoulder to hip.
    m.box("chest", strap, (-0.02, -0.28, 1.34), (0.10, 0.04, 0.70),
          rot=(0, -38, 0))
    m.box("chest", strap, (0.0, 0.28, 1.34), (0.10, 0.04, 0.70), rot=(0, 38, 0))

    # ---- head: flat-top, shades, a grin -----------------------------------------
    m.limb("neck", skin, (0, 0, 1.58), (0, -0.02, 1.70), 0.14, 0.12)
    m.blob("neck", skin, (0, -0.04, 1.82), (0.28, 0.30, 0.30), segments=10,
           rings=8)
    m.box("neck", skin, (0, -0.10, 1.72), (0.22, 0.18, 0.10))       # jaw
    m.box("neck", shades, (0, -0.18, 1.86), (0.26, 0.04, 0.06))
    m.box("neck", hair, (0, 0.0, 1.98), (0.26, 0.28, 0.08))          # flat-top
    for x in (1, -1):
        m.box("neck", hair, (0.12 * x, 0.02, 1.92), (0.05, 0.24, 0.10))

    # ---- arms: short sleeves, then red power gauntlets ------------------------------
    for side, x in (("L", 1), ("R", -1)):
        sh, el, wr = f"shoulder_{side}", f"elbow_{side}", f"wrist_{side}"
        m.blob(sh, shirt, (0.40 * x, 0, 1.54), (0.28, 0.28, 0.26))
        m.limb(sh, skin, (0.42 * x, 0, 1.44), (0.46 * x, 0, 1.27), 0.13, 0.12)
        m.blob(el, armor, (0.46 * x, 0, 1.24), (0.22, 0.22, 0.20))
        m.limb(el, armor, (0.46 * x, 0, 1.24), (0.47 * x, 0, 1.03), 0.15, 0.14)
        m.limb(el, armor_dark, (0.47 * x, 0, 1.06), (0.47 * x, 0, 1.02), 0.155,
               0.155)
        m.box(wr, armor, (0.47 * x, -0.01, 0.91), (0.22, 0.23, 0.20), bevel=0.04)
    # The can, in the right hand: it rises with the arm.
    m.limb("wrist_R", can, (-0.47, -0.04, 0.84), (-0.47, -0.04, 0.46), 0.10,
           0.10)
    m.limb("wrist_R", label, (-0.47, -0.04, 0.70), (-0.47, -0.04, 0.58), 0.104,
           0.104, cap=False)

    # ---- belt, pouches and the cooler -------------------------------------------------
    m.box("hips", belt, (0, 0, 1.00), (0.66, 0.46, 0.12))
    for x in (0.26, -0.26, 0.0):
        m.box("hips", pouch, (x, -0.24, 0.96), (0.11, 0.06, 0.14))
    # The cooler hangs at his left hip, sticking out past his body.
    m.box("hips", cooler, (0.60, -0.06, 0.78), (0.82, 0.36, 0.42), bevel=0.05)
    m.box("hips", trim, (0.60, -0.06, 1.00), (0.84, 0.38, 0.05))
    m.box("hips", trim, (1.02, -0.06, 0.78), (0.02, 0.22, 0.12))   # handle
    m.box("hips", armor_dark, (0.60, -0.25, 0.76), (0.56, 0.012, 0.18))

    # ---- legs: armoured shins and big boots -----------------------------------------------
    m.box("hips", armor_dark, (0, 0, 0.90), (0.56, 0.40, 0.18))
    for side, x in (("L", 1), ("R", -1)):
        hp, kn, an = f"hip_{side}", f"knee_{side}", f"ankle_{side}"
        m.limb(hp, armor_dark, (0.18 * x, 0, 0.94), (0.20 * x, 0, 0.54), 0.15,
               0.13)
        m.blob(kn, armor, (0.20 * x, -0.04, 0.52), (0.16, 0.14, 0.14))
        m.limb(kn, armor, (0.20 * x, 0, 0.50), (0.20 * x, 0, 0.16), 0.15, 0.17)
        m.box(an, armor, (0.20 * x, -0.07, 0.07), (0.24, 0.40, 0.14), bevel=0.04)
        m.box(an, armor_dark, (0.20 * x, -0.07, 0.008), (0.25, 0.41, 0.02))

    return m, stance()


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

def stance(sway=0.0):
    """Cheers: can raised high in the right hand, left hand on the cooler,
    leaning back on wide-planted boots."""
    return {
        "hips": (0, 0, 0),
        "chest": (-6 + sway, 0, 0),
        "neck": (-4, 0, 0),
        "shoulder_R": (-150 - sway * 2, 22, 0), "elbow_R": (-40, 0, 0),
        "shoulder_L": (-6, -14, 0), "elbow_L": (-24, 0, 0),
        "hip_L": (-4, -14, 0), "knee_L": (10, 0, 0), "ankle_L": (-6, 10, 0),
        "hip_R": (-4, 14, 0), "knee_R": (10, 0, 0), "ankle_R": (-6, -10, 0),
    }


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(phase):
    s = math.sin(phase * 2 * math.pi)
    p = stance()
    p.update({
        "hips": (0, 0, 6 * s),
        "chest": (10, 0, -6 * s),
        "shoulder_R": (-120 + 10 * s, 22, 0), "elbow_R": (-60, 0, 0),
        "shoulder_L": (-10 + 25 * s, -14, 0), "elbow_L": (-40, 0, 0),
        "hip_L": (30 * s - 6, -8, 0), "knee_L": (14 + max(0.0, s) * 55, 0, 0),
        "ankle_L": (-8, 0, 0),
        "hip_R": (-30 * s - 6, 8, 0), "knee_R": (14 + max(0.0, -s) * 55, 0, 0),
        "ankle_R": (-8, 0, 0),
    })
    return p


def throw_keys(start, end, wind):
    """One overhand lob of the can between t=start and t=end."""
    s = stance()
    back = with_(s, chest=(-14, 0, 20), shoulder_R=(-175, 30, 0),
                 elbow_R=(-110, 0, 0))
    release = with_(s, chest=(18, 0, -16), neck=(6, 0, 0),
                    shoulder_R=(-95, 10, 0), elbow_R=(-10, 0, 0),
                    hip_L=(-20, -14, 0), knee_L=(24, 0, 0))
    span = end - start
    return [Key(start + span * wind, back, (0, 0.03, -0.02), ease="out"),
            Key(start + span * 0.75, release, (0, -0.06, -0.04), ease="in"),
            Key(end, with_(release, shoulder_R=(-110, 14, 0),
                           elbow_R=(-30, 0, 0)), (0, -0.04, -0.03))]


def clips():
    s = stance()
    idle = [Key(f / 8, stance(3 * math.sin(f / 8 * 2 * math.pi)),
                (0, 0, -0.02 - 0.012 * math.sin(f / 8 * 2 * math.pi)))
            for f in range(9)]
    run = [Key(f / 8, run_pose(f / 8),
               (0, -0.02, -0.03 + 0.04 * abs(math.cos(f / 8 * 2 * math.pi))),
               ease="linear") for f in range(9)]

    # Frothy Cleanse: a belly-deep breath, then a belch that bends him forward.
    inhale = with_(s, chest=(-24, 0, 0), neck=(-20, 0, 0),
                   shoulder_L=(-30, -50, 0), elbow_L=(-40, 0, 0),
                   shoulder_R=(-40, 50, 0), elbow_R=(-40, 0, 0))
    belch = with_(s, chest=(30, 0, 0), neck=(26, 0, 0),
                  shoulder_L=(-20, -70, 0), elbow_L=(-20, 0, 0),
                  shoulder_R=(-30, 70, 0), elbow_R=(-20, 0, 0),
                  hip_L=(-24, -14, 0), knee_L=(30, 0, 0))
    # What's In This Stuff?: chug the can, head back, then a roaring flex.
    chug = with_(s, chest=(-14, 0, 0), neck=(-40, 0, 0),
                 shoulder_R=(-120, -10, 10), elbow_R=(-120, 0, 0))
    flex = with_(s, chest=(-10, 0, 0), neck=(-16, 0, 0),
                 shoulder_L=(-10, -88, 0), elbow_L=(0, -105, 0),
                 shoulder_R=(-10, 88, 0), elbow_R=(0, 105, 0))
    hurt = with_(s, chest=(-18, -6, 10), neck=(-20, 0, 0),
                 shoulder_L=(-20, -50, 0), elbow_L=(-30, 0, 0),
                 shoulder_R=(-60, 50, 0), elbow_R=(-50, 0, 0))
    kneel = with_(s, chest=(30, 0, 0), neck=(30, 0, 0),
                  shoulder_L=(-10, -20, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-10, 20, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-80, -6, 0), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 6, 0), knee_R=(120, 0, 0), ankle_R=(-40, 0, 0))
    lying = {"hips": (0, 0, 0), "chest": (-4, 0, 6), "neck": (-10, 0, 14),
             "shoulder_L": (-150, -60, 0), "elbow_L": (-20, 0, 0),
             "shoulder_R": (-40, 70, 0), "elbow_R": (-30, 0, 0),
             "hip_L": (-40, -14, 0), "knee_L": (60, 0, 0),
             "hip_R": (-20, 14, 0), "knee_R": (30, 0, 0)}

    rounds = [Key(0.0, s)]
    for k in range(3):
        rounds += throw_keys(k / 3, (k + 1) / 3, 0.35)
    rounds.append(Key(1.0, s))

    return [
        Clip("idle", 8, 8, True, idle),
        Clip("run", 8, 12, True, run),
        Clip("toss", 6, 18, False,
             [Key(0.0, s)] + throw_keys(0.0, 0.8, 0.3) + [Key(1.0, s)]),
        Clip("belch", 8, 14, False, [
            Key(0.0, s), Key(0.3, inhale, (0, 0.04, 0.02), ease="out"),
            Key(0.5, belch, (0, -0.08, -0.06), ease="in"),
            Key(0.8, belch, (0, -0.08, -0.06)), Key(1.0, s)]),
        Clip("rounds", 12, 18, False, rounds),
        Clip("chug", 10, 12, False, [
            Key(0.0, s), Key(0.25, chug, (0, 0.02, 0)),
            Key(0.5, chug, (0, 0.02, 0.01)),
            Key(0.7, flex, (0, 0, 0.04), ease="out"),
            Key(0.9, with_(flex, chest=(-16, 0, 0)), (0, 0, 0.05)),
            Key(1.0, s)]),
        Clip("hit", 4, 16, False, [
            Key(0.0, s), Key(0.3, hurt, (0, 0.06, -0.02), ease="out"),
            Key(1.0, s)]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, (0, 0.04, -0.02)),
            Key(0.35, kneel, (0, 0.06, -0.44)),
            Key(0.7, lying, (0, 0.10, 0.18), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, 0.10, 0.18), rot=(-90, 0, 0))]),
    ]
