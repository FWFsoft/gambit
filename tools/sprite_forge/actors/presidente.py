"""El Presidente. Body from art/models/Presidente.glb (Meshy, from the
splash art), on the joints a human placed in the fit editor
(art/models/Presidente.fit.json). The clips below were written for the
earlier code-built body and carry over: the joints keep their names.

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
import os

from forge import Clip, Key, Model
import _hero as h
import _rig
import anim
import skin

PIXEL_SCALE = 1.35  # chunkier art pixels, same size on screen (see forge.ActorSpec)
NAME = "Presidente"
CELL = (210, 250, 174)               # wide and deep below the feet: the Splash lands him flat, arms flung forward
MODEL = "art/models/Presidente.glb"
HEIGHT = 2.2
STYLE = "ink"
RIM = (232, 182, 52)                 # championship gold
# The flag cape hangs beside his hands: keep the wrists off its hem.
ARMS = [f"{j}_{s}" for j in ("shoulder", "elbow", "wrist") for s in "LR"]
FLOORS = ((ARMS, 1.05, "hips", 0.10),)
FIT = skin.load_fit(os.path.join(h.REPO, MODEL))
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
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, floors=FLOORS)
    # The cape is part of the model now; its two joints stay (empty) so
    # the clips that swing them still load.
    c = m.rest_world("chest")
    m.joint("cape", "chest", (c[0], c[1] + 0.20, c[2] + 0.10))
    m.joint("cape_low", "cape", (c[0], c[1] + 0.24, c[2] - 0.40))
    return m, stance()


def stance(sway=0.0):
    """The power pose: fists on hips, elbows flared, feet planted wide.
    The gaps between arms and body are what make his outline his."""
    return {
        "carry": (0, 0, 0),   # roll of a held foe: 0 upright, 90 lying across
        "hips": (0, 0, 0),
        "chest": (-8 + sway, 0, 0),
        "neck": (-16, 0, 0),
        "cape": (4 - sway * 2, 0, 0),
        "cape_low": (6 - sway * 3, 0, 0),
        # Fists at the belt, elbows flared: on the skinned body an elbow
        # bends by pitch (the old code-built pose rolled it, which on this
        # body swings the forearm outward).
        "shoulder_L": (4, -44, 0), "elbow_L": (-100, 0, 0),
        "shoulder_R": (4, 44, 0), "elbow_R": (-100, 0, 0),
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


def code_clips():
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
                  shoulder_R=(-80, 0, -18), elbow_L=(0, -56, 0),
                  elbow_R=(0, 56, 0))
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
                 shoulder_L=(-10, -88, 0), elbow_L=(0, 84, 0),
                 shoulder_R=(-10, 88, 0), elbow_R=(0, -84, 0),
                 cape=(14, 0, 0))
    # Abrazo de oso: bear hug, squeezing in pulses
    hug_open = with_(s, chest=(4, 0, 0), shoulder_L=(-80, 0, 40),
                     shoulder_R=(-80, 0, -40), elbow_L=(0, -16, 0),
                     elbow_R=(0, 16, 0))
    hug = with_(s, chest=(-14, 0, 0), neck=(-10, 0, 0),
                shoulder_L=(-78, 0, 26), shoulder_R=(-78, 0, -26),
                elbow_L=(0, -62, 0), elbow_R=(0, 62, 0))
    squeeze = with_(hug, chest=(-26, 0, 0), neck=(-18, 0, 0),
                    shoulder_L=(-74, 0, 16), shoulder_R=(-74, 0, -16),
                    elbow_L=(0, -80, 0), elbow_R=(0, 80, 0))
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


# ---- El Splash de Rana as editable data (pose_editor.py) -------------------
# The empty-handed LSHIFT ("splash") is keyed in the pose editor against four
# reference models (art/models/refs/PresidenteSplash1-4.glb): jump squat,
# initial leap, mid-air tuck, splash spread. It is saved to
# art/models/Presidente.anim.json; every other clip is still code above.
# 14 frames at 18 fps; the server lands him 0.6 s in (frame ~10.8).

SPLASH_FRAMES = (0, 3, 6, 10)

_TABLE = None
_RIG = None


def joint_table():
    """This body's joints as [(name, parent, rest position)], for the rig."""
    global _TABLE
    if _TABLE is None:
        import bpy
        data = skin.prepare(os.path.join(h.REPO, MODEL), HEIGHT, FIT,
                            log=lambda *a: None, floors=FLOORS)
        bpy.data.objects.remove(data["obj"], do_unlink=True)
        _TABLE = [(n, p, tuple(float(c) for c in data["rest_pos"][n]))
                  for n, p in data["skeleton"]]
    return _TABLE


def rig():
    global _RIG
    if _RIG is None:
        _RIG = _rig.Rig(joint_table(), max_reach=0.999)
    return _RIG


def splash_poses():
    """The first guess at the four keys, as (frame, pose, root offset,
    root rotation), from the reference models. A human dials them in."""
    s = stance()
    squat = with_(s, hips=(-6, 0, 0), abdomen=(16, 0, 0), chest=(24, 0, 0),
                  neck=(-20, 0, 0),
                  # left hand down to the floor between the feet
                  shoulder_L=(-50, -14, 0), elbow_L=(-6, 0, 0),
                  # right fist pulled back by the hip
                  shoulder_R=(30, 34, 0), elbow_R=(-100, 0, 0),
                  hip_L=(-78, -22, 0), knee_L=(118, 0, 0), ankle_L=(-34, 8, 0),
                  hip_R=(-78, 22, 0), knee_R=(118, 0, 0), ankle_R=(-34, -8, 0))
    leap = with_(s, hips=(0, 0, 0), abdomen=(0, 0, 0), chest=(-6, 0, 0),
                 neck=(-6, 0, 0),
                 # both fists up at the chest
                 shoulder_L=(-50, -34, 0), elbow_L=(-110, 0, 0),
                 shoulder_R=(-50, 34, 0), elbow_R=(-110, 0, 0),
                 # left knee driving up, right leg trailing, toe pointed
                 hip_L=(-86, -10, 0), knee_L=(96, 0, 0), ankle_L=(-10, 0, 0),
                 hip_R=(14, 10, 0), knee_R=(34, 0, 0), ankle_R=(34, 0, 0))
    tuck = with_(s, hips=(-10, 0, 0), abdomen=(24, 0, 0), chest=(30, 0, 0),
                 neck=(10, 0, 0),
                 # hands on the shins
                 shoulder_L=(-46, -14, 0), elbow_L=(-50, 0, 0),
                 shoulder_R=(-46, 14, 0), elbow_R=(-50, 0, 0),
                 hip_L=(-120, -12, 0), knee_L=(140, 0, 0), ankle_L=(-10, 0, 0),
                 hip_R=(-120, 12, 0), knee_R=(140, 0, 0), ankle_R=(-10, 0, 0))
    spread = with_(s, hips=(0, 0, 0), abdomen=(-8, 0, 0), chest=(-14, 0, 0),
                   neck=(-24, 0, 0),
                   # arms flung wide and forward
                   shoulder_L=(-70, -80, 0), elbow_L=(-10, 0, 0),
                   shoulder_R=(-70, 80, 0), elbow_R=(-10, 0, 0),
                   # legs stretched out behind, a little apart
                   hip_L=(14, -14, 0), knee_L=(16, 0, 0), ankle_L=(30, 0, 0),
                   hip_R=(14, 14, 0), knee_R=(16, 0, 0), ankle_R=(30, 0, 0))
    a, b, c, d = SPLASH_FRAMES
    return [(a, squat, (0, 0, -0.42), (0, 0, 0)),
            (b, leap, (0, -0.10, 0.55), (0, 0, 0)),
            (c, tuck, (0, -0.25, 1.05), (0, 0, 0)),
            # face down, flat, still high: he lands on the next frames
            (d, spread, (0, -0.45, 0.60), (78, 0, 0))]


def splash_animation():
    r = rig()
    out = anim.Builder()
    names = ("jump squat", "initial leap", "mid-air tuck", "splash spread")
    keys = []
    for (frame, pose, off, rot), name in zip(splash_poses(), names):
        keys.append((frame, out.control(r.control(pose, off), name), "smooth", rot))
    clip = []
    for frame, cname, how, rot in keys:
        k = {"frame": frame, "control": cname, "ease": how}
        if any(abs(v) > 1e-9 for v in rot):
            k["rot"] = list(rot)
        clip.append(k)
    data = out.data()
    data["clips"].append({"name": "splash", "frames": 14, "fps": 18,
                          "loop": False, "overlap": 0.0, "keys": clip})
    return data


def current():
    """The saved animation if there is one, else the first guess."""
    data = anim.load(os.path.join(h.REPO, MODEL)) or splash_animation()
    anim.check(data)
    return data


def clips():
    """Code clips, with the ones keyed as data (splash) swapped in."""
    made = anim.bake(current(), rig(), stance())
    names = {c.name for c in made}
    return [c for c in code_clips() if c.name not in names] + made
