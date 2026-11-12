"""El Presidente. Modelled from assets/previews/Presidente.png (splash art).

A huge luchador statesman: blue star-spangled mask with red eye patches and a
gold crest, an open charcoal suit jacket over a bare chest, a gold chain,
a giant championship belt, an American-flag cape hanging off his right
shoulder, baggy charcoal trousers and black combat boots with gold laces.
His left wrist carries a chunky gold gadget; his right is taped red, white
and blue.

His moves are grapples, so most clips come in two forms: empty-handed, and
"holding" (arms overhead, carrying a foe; the game draws the foe there).
"""

import math

from forge import Clip, Key, Model

PIXEL_SCALE = 1.35  # chunkier art pixels, same size on screen (see forge.ActorSpec)
NAME = "Presidente"
CELL = (144, 196, 174)
# The game pins a grappled foe to the midpoint of his hands.
SOCKET = ("wrist_L", "wrist_R")
LEAP_HEIGHT = 1.15   # root lift at the top of a leap (was 0.72)

SKIN = (200, 112, 56)
MASK = (44, 78, 186)
MASK_RED = (206, 42, 46)
WHITE = (236, 236, 240)
GOLD = (232, 182, 52)
JACKET = (50, 52, 66)
SHIRT = (214, 214, 220)
PANTS = (96, 98, 110)
BOOT = (36, 34, 44)
CAPE_BLUE = (42, 62, 156)
CAPE_RED = (196, 42, 48)
EYE = (250, 236, 120)


def build():
    m = Model("Presidente")
    skin = m.material("skin", SKIN)
    mask = m.material("mask", MASK)
    red = m.material("red", MASK_RED)
    white = m.material("white", WHITE)
    gold = m.material("gold", GOLD)
    jacket = m.material("jacket", JACKET)
    shirt = m.material("shirt", SHIRT)
    pants = m.material("pants", PANTS)
    boot = m.material("boot", BOOT)
    cblue = m.material("cape_blue", CAPE_BLUE)
    cred = m.material("cape_red", CAPE_RED)
    eye = m.material("eye", EYE, glow=True)

    # ---- skeleton: Wade's layout, scaled up and broader -------------------
    m.joint("hips", None, (0, 0, 1.02))
    m.joint("chest", "hips", (0, 0, 1.14))
    m.joint("neck", "chest", (0, 0, 1.62))
    m.joint("cape", "chest", (0, 0.20, 1.58))
    m.joint("cape_low", "cape", (0, 0.24, 1.08))
    for side, x in (("L", 1), ("R", -1)):
        m.joint(f"shoulder_{side}", "chest", (0.44 * x, 0, 1.55))
        m.joint(f"elbow_{side}", f"shoulder_{side}", (0.49 * x, 0, 1.24))
        m.joint(f"wrist_{side}", f"elbow_{side}", (0.50 * x, 0, 0.98))
        m.joint(f"hip_{side}", "hips", (0.15 * x, 0, 0.98))
        m.joint(f"knee_{side}", f"hip_{side}", (0.16 * x, 0, 0.55))
        m.joint(f"ankle_{side}", f"knee_{side}", (0.16 * x, 0, 0.13))

    # ---- belly, chest and abs: a heavyweight's barrel --------------------
    m.blob("chest", skin, (0, -0.02, 1.22), (0.40, 0.30, 0.28), segments=9,
           rings=6)
    m.blob("chest", skin, (0, 0.01, 1.44), (0.72, 0.36, 0.38), segments=10,
           rings=7)
    for x in (1, -1):
        m.blob("chest", skin, (0.12 * x, -0.13, 1.45), (0.24, 0.10, 0.17),
               segments=8, rings=5)
        m.box("chest", jacket, (0.03 * x, -0.165, 1.25), (0.03, 0.012, 0.04))
    m.box("chest", jacket, (0, -0.17, 1.35), (0.014, 0.012, 0.12))

    # Open suit jacket: side panels, back, lapels with a white shirt collar
    for x in (1, -1):
        m.box("chest", jacket, (0.33 * x, 0.05, 1.38), (0.13, 0.34, 0.50),
              taper=1.35)
        m.box("chest", jacket, (0.25 * x, -0.12, 1.42), (0.06, 0.06, 0.32),
              rot=(0, 10 * x, 0))                              # lapel
        m.box("chest", shirt, (0.13 * x, -0.14, 1.56), (0.08, 0.05, 0.08),
              rot=(0, -20 * x, 0))                             # collar
    m.box("chest", jacket, (0, 0.18, 1.38), (0.70, 0.07, 0.52), taper=1.3)
    # gold chain across the chest and the flag pin
    m.limb("chest", gold, (0.17, -0.155, 1.56), (0.0, -0.18, 1.44), 0.012, 0.012,
           sides=4)
    m.limb("chest", gold, (-0.17, -0.155, 1.56), (0.0, -0.18, 1.44), 0.012,
           0.012, sides=4)
    m.box("chest", red, (-0.24, -0.19, 1.47), (0.05, 0.02, 0.035))

    # ---- head: the mask ---------------------------------------------------
    m.limb("neck", skin, (0, 0, 1.58), (0, -0.01, 1.70), 0.10, 0.09)
    m.blob("neck", mask, (0, -0.02, 1.80), (0.26, 0.27, 0.30), segments=10,
           rings=8)
    m.box("neck", skin, (0, -0.07, 1.705), (0.15, 0.17, 0.07))   # chin
    m.box("neck", mask, (0, -0.10, 1.74), (0.14, 0.06, 0.035))   # mouth edge
    for x in (1, -1):
        m.box("neck", red, (0.06 * x, -0.135, 1.81), (0.085, 0.02, 0.06),
              rot=(0, 18 * x, 0))
        m.box("neck", eye, (0.055 * x, -0.142, 1.81), (0.035, 0.02, 0.022))
        m.box("neck", white, (0.10 * x, -0.06, 1.90), (0.03, 0.03, 0.03))  # stars
    m.box("neck", gold, (0, -0.13, 1.885), (0.05, 0.02, 0.06))      # badge
    # The crest: a tall striped fin, the top of his silhouette from any side.
    m.box("neck", red, (0, 0.01, 2.06), (0.07, 0.34, 0.26), taper=0.7)
    for x in (1, -1):
        m.box("neck", white, (0.04 * x, 0.01, 2.04), (0.02, 0.32, 0.22),
              taper=0.7)
    m.box("neck", red, (0, 0.0, 1.955), (0.05, 0.22, 0.03))         # stripe
    m.box("neck", white, (0.035, 0.0, 1.95), (0.02, 0.22, 0.03))
    m.box("neck", white, (-0.035, 0.0, 1.95), (0.02, 0.22, 0.03))

    # ---- cape: flag over his right shoulder, hanging to the knees ---------
    m.box("cape", cblue, (-0.12, 0.20, 1.40), (0.70, 0.05, 0.36), taper=1.1)
    m.box("cape", cblue, (-0.36, 0.02, 1.52), (0.18, 0.40, 0.12))   # drape
    m.box("cape", cblue, (-0.45, 0.05, 1.30), (0.06, 0.34, 0.40))   # side fall
    m.box("cape", cred, (-0.47, 0.05, 1.02), (0.05, 0.32, 0.20))
    for (x, z) in ((-0.28, 1.48), (-0.12, 1.42), (0.05, 1.47), (-0.20, 1.32),
                   (0.12, 1.34), (-0.36, 1.36)):
        m.box("cape", white, (x, 0.23, z), (0.035, 0.02, 0.035))
    for i in range(5):
        mat = cred if i % 2 == 0 else white
        m.box("cape_low", mat, (-0.12, 0.25 + i * 0.012, 1.18 - i * 0.11),
              (0.74 + i * 0.03, 0.04, 0.11), rot=(-6, 0, 0))

    # ---- arms: jacket sleeves, big fists ------------------------------------
    for side, x in (("L", 1), ("R", -1)):
        sh, el, wr = f"shoulder_{side}", f"elbow_{side}", f"wrist_{side}"
        m.blob(sh, jacket, (0.45 * x, 0, 1.55), (0.34, 0.30, 0.30))  # boulder
        m.limb(sh, jacket, (0.36 * x, 0.02, 1.62), (0.10 * x, 0.02, 1.68), 0.15,
               0.12)                                                # trapezius
        m.limb(sh, jacket, (0.45 * x, 0, 1.52), (0.49 * x, 0, 1.24), 0.15, 0.12)
        m.limb(el, jacket, (0.49 * x, 0, 1.24), (0.50 * x, 0, 1.03), 0.12, 0.10)
        m.limb(el, gold, (0.50 * x, 0, 1.08), (0.50 * x, 0, 1.05), 0.095, 0.095)
        m.box(wr, skin, (0.50 * x, -0.005, 0.91), (0.19, 0.20, 0.19), bevel=0.03)
        if side == "L":
            m.box(el, gold, (0.50 * x, -0.07, 1.00), (0.11, 0.06, 0.09))  # gadget
            m.box(el, red, (0.50 * x, -0.10, 1.00), (0.06, 0.02, 0.05))
        else:
            for i, mat in enumerate((cred, white, cblue)):
                m.limb(wr, mat, (0.50 * x, 0, 1.0 - i * 0.025),
                       (0.50 * x, 0, 0.975 - i * 0.025), 0.088, 0.088,
                       cap=False)
        m.box(wr, white, (0.50 * x, -0.075, 0.92), (0.155, 0.03, 0.10))

    # ---- belt: the championship ---------------------------------------------
    m.box("hips", boot, (0, 0, 1.07), (0.50, 0.33, 0.10))
    m.box("hips", gold, (0, -0.18, 1.07), (0.44, 0.05, 0.22), bevel=0.02)
    m.box("hips", boot, (0, -0.21, 1.07), (0.30, 0.012, 0.07))  # lettering
    m.box("hips", white, (0, -0.215, 1.07), (0.22, 0.012, 0.03))
    m.box("hips", red, (0.17, -0.21, 1.07), (0.04, 0.012, 0.04))
    m.box("hips", red, (-0.17, -0.21, 1.07), (0.04, 0.012, 0.04))
    for x in (1, -1):
        m.box("hips", gold, (0.21 * x, -0.13, 1.07), (0.08, 0.04, 0.11),
              rot=(0, 0, 25 * x))

    # ---- legs: baggy trousers into combat boots ------------------------------
    m.box("hips", pants, (0, 0, 0.94), (0.52, 0.34, 0.22), taper=1.04)
    for side, x in (("L", 1), ("R", -1)):
        hp, kn, an = f"hip_{side}", f"knee_{side}", f"ankle_{side}"
        m.limb(hp, pants, (0.15 * x, 0, 1.00), (0.16 * x, 0, 0.55), 0.13, 0.10)
        m.limb(kn, pants, (0.16 * x, 0, 0.57), (0.16 * x, 0, 0.30), 0.10, 0.085)
        m.limb(kn, boot, (0.16 * x, 0, 0.33), (0.16 * x, 0, 0.08), 0.10, 0.10)
        m.box(kn, gold, (0.16 * x, -0.10, 0.22), (0.035, 0.02, 0.20))  # laces
        m.box(an, boot, (0.16 * x, -0.06, 0.05), (0.15, 0.30, 0.10), bevel=0.025)
        m.box(an, cred, (0.16 * x, -0.06, 0.006), (0.155, 0.305, 0.015))

    return m, stance()


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

def stance(sway=0.0):
    """The power pose: fists on hips, elbows flared, feet planted wide.
    The gaps between arms and body are what make his outline his."""
    return {
        "carry": (0, 0, 0),   # roll of a held foe: 0 upright, 90 lying across
        "hips": (0, 0, 0),
        "chest": (6 + sway, 0, 0),
        "neck": (-4, 0, 0),
        "cape": (4 - sway * 2, 0, 0),
        "cape_low": (6 - sway * 3, 0, 0),
        "shoulder_L": (-8, -44, 0), "elbow_L": (-10, 105, 0),
        "shoulder_R": (-8, 44, 0), "elbow_R": (-10, -105, 0),
        "hip_L": (-6, -17, 0), "knee_L": (22 + sway, 0, 0), "ankle_L": (-10, 14, 0),
        "hip_R": (-6, 17, 0), "knee_R": (22 + sway, 0, 0), "ankle_R": (-10, -14, 0),
    }


def overhead(sway=0.0):
    """Holding a foe above his head."""
    p = stance(sway)
    p.update({
        "chest": (-6, 0, 0), "neck": (-14, 0, 0),
        "shoulder_L": (-168, -14, 0), "elbow_L": (-24, 0, 0),
        "shoulder_R": (-168, 14, 0), "elbow_R": (-24, 0, 0),
        "cape": (10, 0, 0),
        "carry": (90, 0, 0),
    })
    return p


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(phase, holding=False):
    s = math.sin(phase * 2 * math.pi)
    p = overhead() if holding else stance()
    p.update({
        "hips": (0, 0, 7 * s),
        "chest": ((4 if holding else 14), 0, -7 * s),
        "cape": (16, 0, 0), "cape_low": (10 + 5 * s, 0, 0),
        "hip_L": (32 * s - 6, -6, 0), "knee_L": (20 + max(0.0, s) * 60, 0, 0),
        "ankle_L": (-10, 0, 0),
        "hip_R": (-32 * s - 6, 6, 0), "knee_R": (20 + max(0.0, -s) * 60, 0, 0),
        "ankle_R": (-10, 0, 0),
    })
    if not holding:
        p.update({
            "shoulder_L": (-20 - 30 * s, -20, 0), "elbow_L": (-70, 0, 0),
            "shoulder_R": (-20 + 30 * s, 20, 0), "elbow_R": (-70, 0, 0),
        })
    return p


def leap_keys(frames, holding, land_t=0.83):
    """Crouch, launch, hang, crash down. Height comes from root offset;
    the server moves him across the ground. `land_t` is when he touches
    down (the server's leap time divided by the clip's length)."""
    up = overhead() if holding else with_(
        stance(), shoulder_L=(-160, -40, 0), elbow_L=(-20, 0, 0),
        shoulder_R=(-160, 40, 0), elbow_R=(-20, 0, 0))
    crouch = with_(up if holding else stance(), chest=(28, 0, 0),
                   hip_L=(-50, -10, 0), knee_L=(90, 0, 0), ankle_L=(-30, 0, 0),
                   hip_R=(-50, 10, 0), knee_R=(90, 0, 0), ankle_R=(-30, 0, 0))
    tuck = with_(up, chest=(-10, 0, 0), cape=(30, 0, 0), cape_low=(18, 0, 0),
                 hip_L=(-60, -14, 0), knee_L=(70, 0, 0),
                 hip_R=(-40, 14, 0), knee_R=(60, 0, 0))
    # Body splash (empty-handed) lands chest-first; carrying a foe he lands
    # on his feet and slams them down.
    land = (with_(stance(), chest=(40, 0, 0), shoulder_L=(-100, -50, 0),
                  shoulder_R=(-100, 50, 0), elbow_L=(-10, 0, 0),
                  elbow_R=(-10, 0, 0), hip_L=(-70, -14, 0), knee_L=(100, 0, 0),
                  hip_R=(-70, 14, 0), knee_R=(100, 0, 0), ankle_L=(-30, 0, 0),
                  ankle_R=(-30, 0, 0))
            if not holding else
            with_(stance(), chest=(46, 0, 0), shoulder_L=(-80, -10, 0),
                  shoulder_R=(-80, 10, 0), elbow_L=(-10, 0, 0),
                  elbow_R=(-10, 0, 0), hip_L=(-60, -12, 0), knee_L=(100, 0, 0),
                  hip_R=(-60, 12, 0), knee_R=(100, 0, 0), carry=(150, 0, 0)))
    t0 = 0.14
    keys = [Key(0.0, stance() if not holding else overhead(), (0, 0, 0)),
            Key(t0, crouch, (0, 0, -0.16), ease="out")]
    # Parabolic hop sampled densely so the arc is smooth.
    span = land_t - 0.02 - t0
    for i in range(1, 8):
        u = i / 8
        height = LEAP_HEIGHT * 4 * u * (1 - u)
        keys.append(Key(t0 + u * span, tuck, (0, -0.06 * u, height),
                        ease="linear"))
    keys.append(Key(land_t, land, (0, -0.06, -0.16), ease="in"))
    keys.append(Key(1.0, land if not holding else overhead(),
                    (0, -0.06, -0.12 if not holding else 0.0)))
    return keys


def sitout_keys(land_t):
    """Bomba: a leaping sit-out powerbomb. He jumps with the foe overhead,
    swings them down in front of him on the way down and sits out, driving
    them back-first into the ground between his legs."""
    up = overhead()
    crouch = with_(up, chest=(24, 0, 0),
                   hip_L=(-50, -10, 0), knee_L=(90, 0, 0), ankle_L=(-30, 0, 0),
                   hip_R=(-50, 10, 0), knee_R=(90, 0, 0), ankle_R=(-30, 0, 0))
    # Rising: foe hoisted high, body arched back, knees tucked.
    hoist = with_(up, chest=(-16, 0, 0), neck=(-20, 0, 0), cape=(34, 0, 0),
                  cape_low=(20, 0, 0), shoulder_L=(-176, -10, 0),
                  shoulder_R=(-176, 10, 0), elbow_L=(-6, 0, 0),
                  elbow_R=(-6, 0, 0), hip_L=(-50, -14, 0), knee_L=(80, 0, 0),
                  hip_R=(-30, 14, 0), knee_R=(70, 0, 0), carry=(120, 0, 0))
    # Falling: arms whip the foe forward, legs kick out ahead for the sit.
    drive = with_(up, chest=(14, 0, 0), neck=(4, 0, 0), cape=(50, 0, 0),
                  cape_low=(30, 0, 0), shoulder_L=(-110, -8, 0),
                  shoulder_R=(-110, 8, 0), elbow_L=(-14, 0, 0),
                  elbow_R=(-14, 0, 0), hip_L=(-80, -18, 0), knee_L=(30, 0, 0),
                  hip_R=(-80, 18, 0), knee_R=(30, 0, 0), ankle_L=(-10, 0, 0),
                  ankle_R=(-10, 0, 0), carry=(160, 0, 0))
    # The sit-out: seated, legs straight out in a V, foe pinned in front.
    sit = with_(stance(), chest=(34, 0, 0), neck=(16, 0, 0), cape=(-20, 0, 0),
                cape_low=(-30, 0, 0), shoulder_L=(-58, -12, 0),
                shoulder_R=(-58, 12, 0), elbow_L=(-8, 0, 0),
                elbow_R=(-8, 0, 0), hip_L=(-90, -22, 0), knee_L=(4, 0, 0),
                ankle_L=(20, 0, 0), hip_R=(-90, 22, 0), knee_R=(4, 0, 0),
                ankle_R=(20, 0, 0), carry=(180, 0, 0))
    sit_low = -0.78   # hips 1.02 -> ~0.24: seat on the floor
    t0 = 0.12
    apex = t0 + (land_t - t0) * 0.45
    keys = [Key(0.0, up, (0, 0, 0)),
            Key(t0, crouch, (0, 0, -0.16), ease="out")]
    for i in range(1, 9):
        u = i / 9
        t = t0 + u * (land_t - 0.03 - t0)
        height = LEAP_HEIGHT * 1.1 * 4 * u * (1 - u)
        pose = hoist if t <= apex else drive
        # Fall toward the seat, not the standing height.
        if u > 0.5:
            height += sit_low * ((u - 0.5) / 0.5) ** 2
        keys.append(Key(t, pose, (0, -0.06 * u, height), ease="linear"))
    keys.append(Key(land_t, sit, (0, 0.06, sit_low), ease="in"))
    keys.append(Key(land_t + 0.05, with_(sit, chest=(40, 0, 0)),
                    (0, 0.06, sit_low + 0.04), ease="out"))
    keys.append(Key(0.9, sit, (0, 0.06, sit_low)))
    keys.append(Key(1.0, with_(stance(), chest=(30, 0, 0),
                               hip_L=(-70, -10, 0), knee_L=(110, 0, 0),
                               hip_R=(-70, 10, 0), knee_R=(110, 0, 0),
                               ankle_L=(-30, 0, 0), ankle_R=(-30, 0, 0)),
                    (0, 0.02, -0.22), ease="out"))
    return keys


def clips():
    idle = [Key(f / 8, stance(3 * math.sin(f / 8 * 2 * math.pi)),
                (0, 0, -0.03 - 0.008 * math.sin(f / 8 * 2 * math.pi)))
            for f in range(9)]
    hold_idle = [Key(f / 6, overhead(3 * math.sin(f / 6 * 2 * math.pi)),
                     (0, 0, -0.03 - 0.01 * math.sin(f / 6 * 2 * math.pi)))
                 for f in range(7)]
    run = [Key(f / 8, run_pose(f / 8),
               (0, 0, -0.03 + 0.04 * abs(math.cos(f / 8 * 2 * math.pi))),
               ease="linear") for f in range(9)]
    hold_run = [Key(f / 8, run_pose(f / 8, True),
                    (0, 0, -0.03 + 0.03 * abs(math.cos(f / 8 * 2 * math.pi))),
                    ease="linear") for f in range(9)]

    s = stance()
    # Lucha Libre: lunge and clamp both hands on the foe
    reach = with_(s, chest=(26, 0, 0), shoulder_L=(-84, 10, 0),
                  shoulder_R=(-84, -10, 0), elbow_L=(-10, 0, 0),
                  elbow_R=(-10, 0, 0), hip_L=(-30, -10, 0), knee_L=(30, 0, 0))
    clamp = with_(s, chest=(12, 0, 0), shoulder_L=(-80, 0, 18),
                  shoulder_R=(-80, 0, -18), elbow_L=(0, 70, 0),
                  elbow_R=(0, -70, 0))
    # Látigo: spin with the foe at arm's length, then let go
    whirl = [Key(0.0, overhead(), (0, 0, 0))]
    for i, angle in enumerate((90, 200, 310, 420)):
        whirl.append(Key(0.12 + i * 0.16, with_(
            s, chest=(-6, 0, 0), shoulder_L=(-90, -60, 0), elbow_L=(-10, 0, 0),
            shoulder_R=(-90, 60, 0), elbow_R=(-10, 0, 0), cape=(30, 0, 0),
            cape_low=(18, 0, 0), carry=(90, 0, 0)), (0, 0, 0), (0, 0, angle),
            ease="linear"))
    whirl.append(Key(0.86, with_(s, chest=(20, 0, 0), shoulder_L=(-100, 0, 0),
                                 shoulder_R=(-100, 0, 0), elbow_L=(0, 0, 0),
                                 elbow_R=(0, 0, 0)),
                     (0, -0.04, 0), (0, 0, 360), ease="out"))
    whirl.append(Key(1.0, s, (0, 0, 0), (0, 0, 360)))
    # Pesado: shoulder-first charge
    charge = with_(s, chest=(34, 0, -24), neck=(10, 0, 0),
                   shoulder_L=(-40, -10, 0), elbow_L=(-100, 0, 0),
                   shoulder_R=(-20, 30, 0), elbow_R=(-90, 0, 0),
                   cape=(30, 0, 0), cape_low=(16, 0, 0))
    charge_a = with_(charge, hip_L=(-40, -6, 0), knee_L=(40, 0, 0),
                     hip_R=(30, 6, 0), knee_R=(60, 0, 0))
    charge_b = with_(charge, hip_L=(30, -6, 0), knee_L=(60, 0, 0),
                     hip_R=(-40, 6, 0), knee_R=(40, 0, 0))
    # Grande Cajones: double-biceps flex and a roar
    flex = with_(s, chest=(-14, 0, 0), neck=(-24, 0, 0),
                 shoulder_L=(-10, -88, 0), elbow_L=(0, -105, 0),
                 shoulder_R=(-10, 88, 0), elbow_R=(0, 105, 0),
                 cape=(14, 0, 0))
    # Abrazo de oso: bear hug, squeezing in pulses
    hug_open = with_(s, chest=(4, 0, 0), shoulder_L=(-80, 0, 40),
                     shoulder_R=(-80, 0, -40), elbow_L=(0, 20, 0),
                     elbow_R=(0, -20, 0))
    hug = with_(s, chest=(-14, 0, 0), neck=(-10, 0, 0),
                shoulder_L=(-78, 0, 26), shoulder_R=(-78, 0, -26),
                elbow_L=(0, 78, 0), elbow_R=(0, -78, 0))
    squeeze = with_(hug, chest=(-26, 0, 0), neck=(-18, 0, 0),
                    shoulder_L=(-74, 0, 16), shoulder_R=(-74, 0, -16),
                    elbow_L=(0, 100, 0), elbow_R=(0, -100, 0))
    hurt = with_(s, chest=(-18, -6, 10), neck=(-20, 0, 0),
                 shoulder_L=(-20, -50, 0), shoulder_R=(-20, 50, 0),
                 elbow_L=(-30, 0, 0), elbow_R=(-30, 0, 0),
                 cape=(-10, 0, 0))
    kneel = with_(s, chest=(30, 0, 0), neck=(30, 0, 0),
                  shoulder_L=(-10, -20, 0), shoulder_R=(-10, 20, 0),
                  elbow_L=(-30, 0, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-80, -6, 0), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 6, 0), knee_R=(120, 0, 0), ankle_R=(-40, 0, 0))
    lying = {"hips": (0, 0, 0), "chest": (-4, 0, 6), "neck": (-10, 0, 14),
             "cape": (-60, 0, 0), "cape_low": (0, 0, 0),
             "shoulder_L": (-150, -60, 0), "elbow_L": (-20, 0, 0),
             "shoulder_R": (-40, 70, 0), "elbow_R": (-30, 0, 0),
             "hip_L": (-40, -14, 0), "knee_L": (60, 0, 0),
             "hip_R": (-20, 14, 0), "knee_R": (30, 0, 0)}

    return [
        Clip("idle", 8, 8, True, idle),
        Clip("run", 8, 12, True, run),
        Clip("hold_idle", 6, 8, True, hold_idle),
        Clip("hold_run", 8, 12, True, hold_run),
        Clip("grab", 5, 18, False, [
            Key(0.0, s), Key(0.4, reach, (0, -0.10, -0.04), ease="out"),
            Key(0.7, clamp, (0, -0.06, -0.02)),
            Key(1.0, overhead())]),
        Clip("latigo", 8, 20, False, whirl),
        Clip("pesado", 6, 18, False, [
            Key(0.0, charge_a, (0, -0.04, -0.04)),
            Key(0.33, charge_b, (0, -0.06, -0.02), ease="linear"),
            Key(0.66, charge_a, (0, -0.06, -0.04), ease="linear"),
            Key(1.0, s, (0, 0, 0))]),
        # Timings match the server: Bomba lands at 620ms, leaps at 600ms.
        Clip("bomba", 16, 18, False, sitout_keys(0.62 / (15 / 18))),
        Clip("cajones", 8, 14, False, [
            Key(0.0, s), Key(0.3, flex, (0, 0.02, 0.02), ease="out"),
            Key(0.75, with_(flex, chest=(-20, 0, 0)), (0, 0.02, 0.03)),
            Key(1.0, s)]),
        Clip("abrazo", 10, 13, False, [
            Key(0.0, hug_open), Key(0.15, hug),
            Key(0.3, squeeze, ease="in"), Key(0.42, hug),
            Key(0.55, squeeze, ease="in"), Key(0.67, hug),
            Key(0.8, squeeze, ease="in"), Key(1.0, s)]),
        Clip("splash", 14, 18, False, leap_keys(14, False, 0.60 / (13 / 18))),
        Clip("rana_bomba", 14, 18, False, leap_keys(14, True, 0.60 / (13 / 18))),
        Clip("hit", 4, 16, False, [
            Key(0.0, s), Key(0.3, hurt, (0, 0.06, -0.02), ease="out"),
            Key(1.0, s)]),
        Clip("death", 9, 12, False, [
            Key(0.0, hurt, (0, 0.04, -0.02)),
            Key(0.35, kneel, (0, 0.06, -0.44)),
            Key(0.7, lying, (0, 0.10, 0.16), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, 0.10, 0.16), rot=(-90, 0, 0))]),
    ]
