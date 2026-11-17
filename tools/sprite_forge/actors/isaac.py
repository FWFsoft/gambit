"""Isaac, the Beacon (Winger): body from his generated model, motion as data.

The body is art/models/Isaac.glb (Meshy, from the A-pose concept art), on
the joints a human placed (art/models/Isaac.fit.json). Nothing here changes
his proportions.

Who he is in motion: a brawler's body that never makes a fist. He is the
team's light, so he stands tall and calm, chin level, and everything he
does comes out of open palms: a two-handed push of light (Ray), a hand
held out on a rope of light (Lifeline), a raised hand that comes down in
blessing (Blessing), and arms flung wide as he arrives (Flash). Weight
comes from the canister rack on his right shoulder: he moves big and
deliberately, never twitchy.

Preview with preview.py isaac ray out.png 0,2,4.
"""

import math
import os

import bpy

import _rig
import anim
import forge
from forge import Clip, Key, Model
import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

NAME = "Isaac"
MODEL = "art/models/Isaac.glb"
HEIGHT = 2.1                         # broad and tall: between Wade (1.95) and Presidente (2.2)
YAW = 0.0                            # degrees, if the model does not face -Y
CENTRE_X = -0.07                     # body's centre line (nose, crown, crotch): the backpack
                                     # and cape pull the automatic centring 7 cm off it
STYLE = "ink"                        # required for skinned meshes
RIM = (255, 170, 60)                 # radiant amber: his eyes and glowing canisters
CELL = (184, 184, 128)               # taller cell than the cast: room below the feet for his fall
FOLLOW = {}

# Saved from the joint editor (fit.py --editor) as art/models/Isaac.fit.json.
FIT = skin.load_fit(os.path.join(REPO, MODEL))


def build():
    base = stance()
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                centre_x=CENTRE_X)
    return m, base


_TABLE = None
_RIG = None


def joint_table():
    """This body's joints as [(name, parent, rest position)], for the rig."""
    global _TABLE
    if _TABLE is None:
        data = skin.prepare(os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                            log=lambda *a: None, centre_x=CENTRE_X)
        bpy.data.objects.remove(data["obj"], do_unlink=True)
        _TABLE = [(n, p, tuple(float(c) for c in data["rest_pos"][n]))
                  for n, p in skin.HUMANOID]
    return _TABLE


def rig():
    global _RIG
    if _RIG is None:
        _RIG = _rig.Rig(joint_table())
    return _RIG


def current():
    """The saved animation if there is one, else the starting point."""
    data = anim.load(os.path.join(REPO, MODEL)) or animation()
    anim.check(data)
    return data


def clips():
    return anim.bake(current(), rig(), stance())


# ---------------------------------------------------------------------------
# The starting point, by position
# ---------------------------------------------------------------------------
# x is his left, -y is forward, z is up. Hands are measured from the head
# joint, feet are in the world.

FLOOR = 0.20                         # ankle height with the foot flat

# Square, shoulder-wide stance, toes a little out: planted, not bladed.
FOOT_L = dict(at=(0.27, -0.06, FLOOR), rot=(0, 0, 10), knee=(0.3, -1, 0))
FOOT_R = dict(at=(-0.27, 0.08, FLOOR), rot=(0, 0, -12), knee=(-0.3, -1, 0))
# Open hands held out from his sides, palms forward: the giver's stance, and
# an outline the barrel-shaped Suds does not have (silhouette.py).
OPEN_L = dict(at=(0.50, -0.10, -0.80), elbow=(0.8, 0.5, -0.3))
OPEN_R = dict(at=(-0.50, -0.08, -0.80), elbow=(-0.8, 0.5, -0.3))


def ctl(off=(0, 0, -0.04), twist=(0, 0, 0), lean=(0, -2), side=(0, 0),
        tilt=0.0, nod=-4.0, look=0.9, L=OPEN_L, R=OPEN_R, fl=FOOT_L, fr=FOOT_R):
    """A control.

    off    hips, from standing straight (x his left, -y forward, z up)
    twist  yaw of (hips, abdomen, chest); + turns him to his left
    lean   forward bend of (abdomen, chest); side: side bend of the same
    tilt   hips pitch; nod: extra head pitch (+ chin down)
    look   how much the head turns back to face the target
    L, R   dict(at=, elbow=): the wrist goes to `at`, measured from the head
           joint; the elbow sticks out along `elbow`.
    fl, fr dict(at=, rot=(pitch, roll, yaw), knee=): the ankle in the world,
           the foot's rotation, and the way the knee points.
    """
    r = rig()
    c = {"offset": tuple(off)}
    c["hips"] = (tilt, 0, twist[0])
    c["abdomen"] = (lean[0], side[0], twist[1])
    c["chest"] = (lean[1], side[1], twist[2])
    yaw, bend, roll = sum(twist), lean[0] + lean[1] + tilt, side[0] + side[1]
    c["neck"] = (-0.3 * bend, -0.3 * roll, -0.35 * look * yaw)
    c["head"] = (-0.4 * bend + nod, -0.4 * roll, -0.65 * look * yaw)
    head = r.joint_at(c, "head")
    for s, arm in (("L", L), ("R", R)):
        sh = r.joint_at(c, f"shoulder_{s}")
        c[f"hand_{s}"] = tuple(head[i] + arm["at"][i] for i in range(3))
        c[f"elbow_{s}"] = _rig.aim(sh, arm.get("elbow", (0, 0, -1)))
        c[f"wrist_{s}"] = tuple(arm.get("wrist", (0, 0, 0)))
        c[f"fore_{s}"] = (1.0, 1.0, 1.0)
    for s, foot in (("L", fl), ("R", fr)):
        c[f"foot_{s}"] = tuple(foot["at"])
        c[f"footrot_{s}"] = tuple(foot.get("rot", (0, 0, 0)))
        c[f"knee_{s}"] = _rig.aim(r.joint_at(c, f"hip_{s}"), foot.get("knee", (0, -1, 0)))
    return c


_STANCE = None


def stance():
    """The stance as joint angles: what the model stands in and what fills
    any joint a clip leaves alone."""
    global _STANCE
    if _STANCE is None:
        _STANCE = rig().solve(ctl())[0]
    return dict(_STANCE)


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


TOE_IN = 28.0                        # degrees the model's feet turn out at rest


def run_pose(p):
    """A heavy, upright jog: the rack on his shoulder keeps the chest
    square, the arms swing short and open-handed."""
    s = math.sin(p * 2 * math.pi)

    def leg(sign):
        swing = -32 * s * sign - 6          # negative = forward
        fold = 12 + 70 * max(0.0, s * sign * -1)
        # The model stands with its toes turned out; a run points them
        # where he is going (turned-out feet in a run read as a waddle).
        return (swing, 0, -sign * TOE_IN), (fold, 0, 0), (-6 - 8 * s * sign, 0, 0)

    hl, kl, al = leg(1)
    hr, kr, ar = leg(-1)
    return {
        "hips": (6, 0, 6 * s), "abdomen": (6, 0, -2 * s),
        "chest": (6, 0, -6 * s), "neck": (-6, 0, 2 * s), "head": (-8, 0, 2 * s),
        "shoulder_L": (26 * s, -14, 0), "elbow_L": (-34, 0, 0), "wrist_L": (0, 0, 0),
        "shoulder_R": (-26 * s, 14, 0), "elbow_R": (-34, 0, 0), "wrist_R": (0, 0, 0),
        "hip_L": hl, "knee_L": kl, "ankle_L": al,
        "hip_R": hr, "knee_R": kr, "ankle_R": ar,
    }


def animation():
    r = rig()
    out = anim.Builder()
    G = ctl()
    out.control(G, "stance")

    def clip(name, frames, fps, keys, loop=False, overlap=0.0):
        full = []
        for k in keys:
            how = k[2] if len(k) > 2 else "smooth"
            rot = k[3] if len(k) > 3 else (0, 0, 0)
            full.append((k[0], k[1], how, rot))
        out.clip(name, frames, fps, loop, overlap, full)

    def angles(name, frames, fps, loop, keys, overlap=0.0):
        """A clip authored as joint angles: one control per frame."""
        src = Clip(name, frames, fps, loop, keys)
        base, full = stance(), []
        for f in range(frames):
            t = f / frames if loop else f / max(1, frames - 1)
            pose, off, rot = forge.sample(src, t, base)
            full.append((f, r.control(pose, off), "linear", rot))
        if loop:
            full.append((frames, full[0][1], "linear", full[0][3]))
        out.clip(name, frames, fps, loop, overlap, full)

    # ---- idle: tall and calm; the breath lifts the chest and opens the hands
    breathe = ctl(off=(0, 0, -0.02), lean=(-1, -5), nod=-6,
                  L=dict(at=(0.53, -0.13, -0.77), elbow=(0.8, 0.5, -0.3)),
                  R=dict(at=(-0.53, -0.11, -0.77), elbow=(-0.8, 0.5, -0.3)))
    clip("idle", 12, 8, [(0, G), (6, breathe), (12, G)], loop=True, overlap=0.04)

    run = []
    for f in range(11):
        p = f / 10
        s = math.sin(p * 2 * math.pi)
        run.append(Key(p, run_pose(p), (0, -0.03, -0.06 + 0.05 * (1 - abs(s))),
                       ease="linear"))
    angles("run", 10, 14, True, run, overlap=0.03)

    # The ability lands when the button is pressed, so every cast shows its
    # strike early: one frame of load, the strike on frame 2, a held frame,
    # then the way out.

    # ---- LMB: Ray of Benevolence. Gather the light at the chest, then push
    # it out with both palms, heels of the hands together: the cone's apex --
    gather = ctl(off=(0, 0.06, -0.08), twist=(0, 0, 0), lean=(-4, -8), nod=-2,
                 L=dict(at=(0.14, -0.20, -0.42), elbow=(1, 0.3, -0.5)),
                 R=dict(at=(-0.14, -0.20, -0.42), elbow=(-1, 0.3, -0.5)),
                 fr=dict(at=(-0.27, 0.14, FLOOR), rot=(0, 0, -14), knee=(-0.3, -1, 0)))
    push = ctl(off=(0, -0.06, -0.10), lean=(6, 10), nod=-2,
               L=dict(at=(0.10, -0.62, -0.32), elbow=(0.8, 0, -0.6)),
               R=dict(at=(-0.10, -0.62, -0.32), elbow=(-0.8, 0, -0.6)),
               fl=dict(at=(0.27, -0.18, FLOOR), rot=(0, 0, 10), knee=(0.3, -1, 0)))
    clip("ray", 10, 24, [
        (0, G),
        (1, gather, "out"),
        (2, push, "snap"),
        (4, push),
        (7, ctl(off=(0, -0.02, -0.06), lean=(2, 4)), "in"),
        (9, G),
    ])

    # ---- RMB (hold): Lifeline. His left hand out on the rope of light, the
    # right held at his heart, leaning back against the pull ----------------
    def tether(lean_back):
        return ctl(off=(0, 0.10 + lean_back, -0.12), twist=(-10, -4, -6),
                   lean=(-4 - 20 * lean_back, -6 - 20 * lean_back), nod=-2,
                   L=dict(at=(0.20, -0.66, -0.26), elbow=(1, 0.2, -0.4)),
                   R=dict(at=(-0.06, -0.24, -0.46), elbow=(-1, 0.4, -0.4)),
                   fl=dict(at=(0.26, -0.24, FLOOR), rot=(0, 0, 6), knee=(0.3, -1, 0)),
                   fr=dict(at=(-0.26, 0.22, FLOOR), rot=(0, 0, -20), knee=(-0.4, -1, 0)))

    T = tether(0.0)
    clip("lifeline_charge", 8, 10, [(0, T), (4, tether(0.03)), (8, T)], loop=True)

    # ---- RMB (release): pulled along the rope. The reaching hand leads, the
    # body tips after it, feet leave the ground and trail ------------------
    pulled = ctl(off=(0, -0.10, 0.10), twist=(-8, -3, -4), lean=(16, 18), nod=6,
                 L=dict(at=(0.18, -0.70, -0.10), elbow=(1, 0.2, -0.3)),
                 R=dict(at=(-0.30, 0.10, -0.60), elbow=(-0.8, 0.6, -0.2)),
                 fl=dict(at=(0.22, 0.10, 0.42), rot=(30, 0, 6), knee=(0.2, -1, 0)),
                 fr=dict(at=(-0.24, 0.40, 0.52), rot=(45, 0, -10), knee=(-0.2, -1, -0.2)))
    clip("lifeline", 10, 20, [
        (0, T),
        (1, pulled, "snap"),
        (5, pulled),
        (7, ctl(off=(0, -0.02, -0.14), lean=(8, 8)), "in"),
        (9, G),
    ])

    # ---- Q: The Blessing of Isaac. The right hand rises, palm up, then comes
    # down open toward whoever he blesses; he bows his head into it --------
    raise_hand = ctl(off=(0, 0.04, -0.02), twist=(8, 3, 4), lean=(-4, -8), nod=-12,
                     R=dict(at=(-0.30, -0.12, 0.26), elbow=(-1, 0.3, 0.2)),
                     L=dict(at=(0.08, -0.22, -0.50), elbow=(1, 0.3, -0.5)))
    bestow = ctl(off=(0, -0.04, -0.10), twist=(-6, -2, -3), lean=(10, 12), nod=12,
                 R=dict(at=(-0.20, -0.66, -0.30), elbow=(-0.8, 0, -0.5)),
                 L=dict(at=(0.08, -0.22, -0.52), elbow=(1, 0.3, -0.5)),
                 fr=dict(at=(-0.27, 0.00, FLOOR), rot=(0, 0, -10), knee=(-0.3, -1, 0)))
    clip("blessing", 12, 20, [
        (0, G),
        (1, raise_hand, "out"),
        (2, raise_hand),
        (4, bestow, "snap"),
        (7, bestow),
        (10, ctl(off=(0, 0, -0.06), lean=(4, 4), nod=2), "in"),
        (11, G),
    ])

    # ---- Shift: Flash of Sanctity. Hands meet at the heart and he folds in,
    # then he is there, arms flung wide and chest up: the heal burst ------
    pray = ctl(off=(0, 0.04, -0.22), lean=(12, 14), nod=14,
               L=dict(at=(0.04, -0.26, -0.48), elbow=(1, 0, -0.6)),
               R=dict(at=(-0.04, -0.26, -0.48), elbow=(-1, 0, -0.6)))
    spread = ctl(off=(0, 0, 0.08), lean=(-6, -12), nod=-14,
                 L=dict(at=(0.84, -0.16, -0.40), elbow=(0.3, 0.3, -1)),
                 R=dict(at=(-0.84, -0.16, -0.40), elbow=(-0.3, 0.3, -1)),
                 fl=dict(at=(0.30, -0.06, FLOOR + 0.06), rot=(-10, 0, 10), knee=(0.3, -1, 0)),
                 fr=dict(at=(-0.30, 0.08, FLOOR + 0.06), rot=(-10, 0, -12), knee=(-0.3, -1, 0)))
    clip("flash", 10, 24, [
        (0, G),
        (1, pray, "out"),
        (2, spread, "snap"),
        (5, spread),
        (8, ctl(off=(0, 0, -0.06), lean=(2, 0)), "in"),
        (9, G),
    ])

    # ---- getting hit, and going down ----------------------------------------
    g, low = stance(), tuple(G["offset"])
    hurt = with_(g, abdomen=(-8, 0, 6), chest=(-18, -4, 10), neck=(-10, 0, 4),
                 head=(-24, 0, 12), shoulder_L=(-20, -36, -6), elbow_L=(-50, 0, 0),
                 shoulder_R=(-16, 36, 6), elbow_R=(-56, 0, 0))
    stagger = with_(hurt, chest=(-24, -8, 18), head=(-16, 0, 16),
                    hip_L=(-26, -4, -8), knee_L=(36, 0, 0))
    kneel = with_(g, abdomen=(14, 0, 0), chest=(20, 0, 0), head=(26, 0, 0),
                  shoulder_L=(-10, -20, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-10, 20, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-70, 0, -6), knee_L=(120, 0, 0), ankle_L=(-40, 0, 0),
                  hip_R=(-80, 0, 8), knee_R=(130, 0, 0), ankle_R=(-40, 0, 0))
    lying = with_(g, hips=(0, 0, 0), abdomen=(-4, 0, 0), chest=(-6, 0, 8),
                  head=(-10, 0, 20), shoulder_L=(-150, 50, 0), elbow_L=(-20, 0, 0),
                  shoulder_R=(-40, -70, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-50, 0, -10), knee_L=(80, 0, 0), ankle_L=(-20, 0, 0),
                  hip_R=(-20, 0, 14), knee_R=(30, 0, 0), ankle_R=(-10, 0, 0))
    angles("hit", 6, 20, False, [
        Key(0.0, g, low), Key(0.22, hurt, (0, 0.11, -0.12), ease="out"),
        Key(0.55, with_(hurt, head=(-14, 0, 6)), (0, 0.09, -0.13)),
        Key(1.0, g, low)], overlap=0.025)
    angles("death", 12, 12, False, [
        Key(0.0, hurt, (0, 0.08, -0.12)),
        Key(0.2, stagger, (0, 0.16, -0.18), ease="out"),
        Key(0.45, kneel, (0, 0.0, -0.52)),
        # He is tall: slide the hips forward as he goes over, so he lies
        # across the middle of his cell instead of out of it.
        Key(0.75, lying, (0, -0.48, 0.18), rot=(-88, 0, 0), ease="in"),
        Key(1.0, lying, (0, -0.48, 0.18), rot=(-90, 0, 0))], overlap=0.02)
    return out.data()
