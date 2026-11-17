"""Kade: body from his generated model, motion as data.

The body is art/models/Kade.glb (Meshy task d93ffa36, from the A-pose
concept art), on the joints in art/models/Kade.fit.json. Nothing here
changes his proportions.

Who he is in motion: a street fighter whose weapon is the visor. He fights
bladed, left side leading, low in his legs; the hands stay fists and stay
back, because the hit comes out of his face. Every big move is a wind-up he
leans his whole body into, then the head drives forward to aim the beam.

Keys come from the human's keyframe sheets in "AA Assets/Kade/"
(<button>_animation.png): red guides are the legs, green the arms, pink
neck root to abdomen to hips.

Preview with preview.py kade beam out.png 0,2,4.
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

NAME = "Kade"
MODEL = "art/models/Kade.glb"
HEIGHT = 1.95                        # forge units; Wade is 1.95, Presidente 2.2
YAW = 0.0                            # degrees, if the model does not face -Y
STYLE = "ink"                        # required for skinned meshes
RIM = (60, 150, 255)                 # visor blue (first pass: registry says dark red)
CELL = (176, 156, 128)               # cell width, height, feet line (px)
FOLLOW = {}

# Saved from the joint editor (fit.py --editor) as art/models/Kade.fit.json.
FIT = skin.load_fit(os.path.join(REPO, MODEL))


def build():
    base = stance()
    m = Model(NAME)
    skin.attach(m, os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW, hands=True, collars=True)
    return m, base


_TABLE = None
_RIG = None


def joint_table():
    """This body's joints as [(name, parent, rest position)], for the rig."""
    global _TABLE
    if _TABLE is None:
        data = skin.prepare(os.path.join(REPO, MODEL), HEIGHT, FIT, yaw=YAW,
                            log=lambda *a: None, hands=True, collars=True)
        bpy.data.objects.remove(data["obj"], do_unlink=True)
        _TABLE = [(n, p, tuple(float(c) for c in data["rest_pos"][n]))
                  for n, p in data["skeleton"]]
    return _TABLE


def rig():
    global _RIG
    if _RIG is None:
        _RIG = _rig.Rig(joint_table(), max_reach=0.9995)   # his ready locks the knees
    return _RIG


def current():
    """The saved animation if there is one, else the starting point."""
    data = anim.load(os.path.join(REPO, MODEL)) or animation()
    anim.check(data)
    return data


def clips():
    """The posed clips (saved from the pose editor), plus the everyday
    clips the game needs that are not posed by hand yet: run, hit, death."""
    data = current()
    extra = kit_animation(data)
    have = {c["name"] for c in data["clips"]}
    merged = {"controls": dict(data["controls"]),
              "clips": list(data["clips"])}
    for name, ctl_ in extra["controls"].items():
        merged["controls"].setdefault(name, ctl_)
    merged["clips"] += [c for c in extra["clips"] if c["name"] not in have]
    made = anim.bake(merged, rig(), stance())
    have = {c.name for c in made}
    return made + [c for c in everyday_clips() if c.name not in have]


def kit_animation(data):
    """Clips for the rest of the kit, as controls (so they can be posed in
    the pose editor later). Built from the saved keys where they exist.

    slap          LMB Refraction Wall: a short dash with his open hand out;
                  he slaps the floor and the mirror stands up out of it.
                  Frame 0 is the crouch held while aiming
    release_hold  Q Full Release, while the beam runs: the fire key held,
                  trembling
    spent         Q's end: limiters back on, he hangs over his knees,
                  heaving, smoke curling off him (the game draws the smoke)
    step          LSHIFT Mirror Step: he lands out of the mirror crouched
                  and rises
    """
    out = anim.Builder()
    C = data["controls"]
    G = {k: tuple(v) for k, v in C["stance"].items()}
    fire = {k: tuple(v) for k, v in C.get("beam 1", ctl()).items()}

    def nudge(c, dx=0.0, dz=0.0):
        c = dict(c)
        o = c["offset"]
        c["offset"] = (o[0] + dx, o[1], o[2] + dz)
        return c

    def clip(name, frames, fps, keys, loop=False):
        out.clip(name, frames, fps, loop, 0.02,
                 [(k[0], k[1], k[2] if len(k) > 2 else "smooth", (0, 0, 0)) for k in keys])

    # ---- slap (LMB Refraction Wall): a short dash, his open hand out ahead;
    # he slaps the floor and the mirror stands up out of it. Frame 0 is the
    # crouch he holds while the mirror is aimed.
    ready = ctl(off=(0, 0.02, -0.12), twist=(-8, -4, -4), lean=(10, 8),
                nod=-6, turn=0.0, tuck=0.0, grip=(0.2, 0.8),
                L=dict(sh=(0.16, 0.06, -0.48), elbow=(0.6, 1, 0.2)),
                R=dict(sh=(-0.18, 0.06, -0.50), elbow=(-0.6, 1, 0)),
                fl=dict(at=(0.26, -0.18, FLOOR), rot=(0, 0, 12), knee=(0.3, -1, 0)),
                fr=dict(at=(-0.26, 0.18, FLOOR), rot=(0, 0, -20), knee=(-0.3, -1, 0)))
    reach = ctl(off=(0, -0.12, -0.24), twist=(-10, -6, -6), lean=(26, 20),
                nod=-14, turn=0.0, tuck=-4.0, grip=(0.0, 0.9),
                L=dict(sh=(0.06, -0.58, -0.30), elbow=(0.6, 0.2, 0.4)),
                R=dict(sh=(-0.22, 0.26, -0.44), elbow=(-0.6, 1, 0.2)),
                fl=dict(at=(0.24, -0.44, FLOOR), rot=(0, 0, 10), knee=(0.3, -1, 0)),
                fr=dict(at=(-0.26, 0.42, FLOOR), rot=(0, 0, -25), knee=(-0.3, -1, 0)))
    slap = ctl(off=(0, -0.26, -0.74), twist=(-6, -4, -4), lean=(64, 44),
               nod=-12, turn=0.0, tuck=-8.0, grip=(0.0, 0.9),
               L=dict(sh=(0.04, -0.50, -0.74), elbow=(0.7, 0.5, 0.3)),
               R=dict(sh=(-0.24, 0.30, -0.30), elbow=(-0.6, 1, 0.3)),
               fl=dict(at=(0.28, -0.56, FLOOR), rot=(0, 0, 12), knee=(0.3, -1, 0)),
               fr=dict(at=(-0.28, 0.52, FLOOR), rot=(0, 0, -30), knee=(-0.4, -1, 0)))
    clip("slap", 12, 20, [(0, ready), (2, reach, "out"), (3, slap, "snap"),
                          (6, slap), (11, G, "in")])

    # ---- Q: the beam held, trembling ---------------------------------------
    clip("release_hold", 4, 12, [(0, fire), (2, nudge(fire, 0.006, -0.006)),
                                 (4, fire)], loop=True)

    # ---- Q's end: spent, heaving ---------------------------------------------
    spent = ctl(off=(0, 0.10, -0.30), twist=(0, 0, 0), lean=(48, 34),
                nod=10, turn=0.0, tuck=-10.0, grip=(0.4, 0.4),
                L=dict(sh=(0.02, -0.36, -0.56), elbow=(0.8, 0.4, 0)),
                R=dict(sh=(-0.02, -0.36, -0.56), elbow=(-0.8, 0.4, 0)),
                fl=dict(at=(0.32, -0.06, FLOOR), rot=(0, 0, 16), knee=(0.5, -1, 0)),
                fr=dict(at=(-0.32, 0.06, FLOOR), rot=(0, 0, -16), knee=(-0.5, -1, 0)))
    heave = dict(spent)
    heave["offset"] = (spent["offset"][0], spent["offset"][1], spent["offset"][2] + 0.03)
    heave["chest"] = (spent["chest"][0] - 6,) + tuple(spent["chest"][1:])
    clip("spent", 12, 10, [(0, spent), (6, heave), (12, spent)], loop=True)

    # ---- LSHIFT: out of the mirror, crouched, rising ------------------------
    land = ctl(off=(0, 0.02, -0.30), lean=(16, 12), nod=-10, turn=0.0,
               tuck=0.0, grip=(0.9, 0.9),
               L=dict(sh=(0.20, -0.10, -0.50), elbow=(0.6, 1, 0)),
               R=dict(sh=(-0.20, 0.06, -0.50), elbow=(-0.6, 1, 0)),
               fl=dict(at=(0.28, -0.14, FLOOR), rot=(0, 0, 14), knee=(0.4, -1, 0)),
               fr=dict(at=(-0.28, 0.16, FLOOR), rot=(0, 0, -16), knee=(-0.4, -1, 0)))
    clip("step", 8, 20, [(0, land), (3, land), (7, G, "in")])
    return out.data()


def with_(base, **changes):
    pose = dict(base)
    pose.update(changes)
    return pose


def run_pose(p):
    """A grounded jog: fists low and loose, the chest square, toes pointing
    where he runs (turned-out feet in a run read as a waddle)."""
    s = math.sin(p * 2 * math.pi)

    def leg(sign):
        swing = -32 * s * sign - 6          # negative = forward
        fold = 12 + 70 * max(0.0, s * sign * -1)
        return (swing, 0, 0), (fold, 0, 0), (-6 - 8 * s * sign, 0, 0)

    hl, kl, al = leg(1)
    hr, kr, ar = leg(-1)
    return {
        "hips": (6, 0, 6 * s), "abdomen": (6, 0, -2 * s),
        "chest": (6, 0, -6 * s), "neck": (-6, 0, 2 * s), "head": (-8, 0, 2 * s),
        "shoulder_L": (30 * s, -12, 0), "elbow_L": (-70, 0, 0), "wrist_L": (0, 0, 0),
        "shoulder_R": (-30 * s, 12, 0), "elbow_R": (-70, 0, 0), "wrist_R": (0, 0, 0),
        "hip_L": hl, "knee_L": kl, "ankle_L": al,
        "hip_R": hr, "knee_R": kr, "ankle_R": ar,
        "fingers_L": fist("L", 0.9), "tips_L": fist("L", 0.9),
        "fingers_R": fist("R", 0.9), "tips_R": fist("R", 0.9),
    }


def everyday_clips():
    """run, hit and death as joint angles (Isaac's recipe, on Kade's body)."""
    g = stance()
    low = tuple(rig().solve(ctl())[1])
    run = [Key(f / 10, run_pose(f / 10), (0, -0.03, -0.06 + 0.05 * (1 - abs(math.sin(f / 10 * 2 * math.pi)))),
               ease="linear") for f in range(11)]
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
    return [
        Clip("run", 10, 14, True, run, overlap=0.03),
        Clip("hit", 6, 20, False, [
            Key(0.0, g, low), Key(0.22, hurt, (0, 0.11, -0.12), ease="out"),
            Key(0.55, with_(hurt, head=(-14, 0, 6)), (0, 0.09, -0.13)),
            Key(1.0, g, low)], overlap=0.025),
        Clip("death", 12, 12, False, [
            Key(0.0, hurt, (0, 0.08, -0.12)),
            Key(0.2, stagger, (0, 0.16, -0.18), ease="out"),
            Key(0.45, kneel, (0, 0.0, -0.52)),
            Key(0.75, lying, (0, -0.48, 0.18), rot=(-88, 0, 0), ease="in"),
            Key(1.0, lying, (0, -0.48, 0.18), rot=(-90, 0, 0))], overlap=0.02),
    ]


# ---------------------------------------------------------------------------
# The starting point, by position
# ---------------------------------------------------------------------------
# x is his left, -y is forward, z is up. Hands are measured from the head
# joint, feet are in the world.

FLOOR = 0.165                        # ankle height with the foot flat (from the fit: rest ankle z)

# Ready: square, a little narrower than the shoulders, fists loose at his
# sides (sheet key 1).
# Ready (sheet key 1): a power stance. Feet wide and even, toes out, knees
# locked; the hips sit square between the feet, the spine stacked straight
# over them with only the chest lifted. The arms hang from the shoulders,
# a little out from the body with the elbows soft and pointing back (not
# out: that rolls the arms in and the shoulder pad into the chest), fists
# at crotch height clear of the thighs.
FOOT_L = dict(at=(0.27, 0.00, FLOOR), rot=(0, 0, 20), knee=(0.3, -1, 0))
FOOT_R = dict(at=(-0.27, 0.00, FLOOR), rot=(0, 0, -20), knee=(-0.3, -1, 0))
FIST_L = dict(sh=(0.19, -0.03, -0.555), elbow=(0.45, 1, 0))
FIST_R = dict(sh=(-0.19, -0.03, -0.555), elbow=(-0.45, 1, 0))
READY_OFF = (0.0, 0.0, -0.016)       # high enough that both knees lock
READY_LEAN = (3.0, -4.0)             # stacked over the hips, chest lifted
READY_NOD = -3.0                     # chin up a touch
READY_TUCK = 4.0                     # neck carried back with the chest; only the head levels
READY_TURN = 14.0                    # degrees the head turns past the body to the target       # hips over the left foot, a little lower


# Closing the hands (skin hands=True): the fingers fold at the knuckles and
# again at the middle joints, each by up to FIST_DEG, about the axis that
# carries them into this model's palm (the palms face back and in, toward
# the thighs). Measured on close-ups: past about 80 degrees the fingertips
# push through the glove.
FIST_DEG = 80.0
_FIST_AXIS = {"L": (1.0, 1.0, 0.0), "R": (1.0, -1.0, 0.0)}   # R mirrors L


def fist(side, k):
    """Finger angles for a hand closed k of the way (0 open, 1 a fist)."""
    from mathutils import Matrix, Vector
    if abs(k) < 1e-6:
        return (0.0, 0.0, 0.0)
    e = Matrix.Rotation(math.radians(FIST_DEG * k), 3,
                        Vector(_FIST_AXIS[side]).normalized()).to_euler("XYZ")
    return tuple(math.degrees(a) for a in e)


def ctl(off=READY_OFF, twist=(0, 0, 0), lean=READY_LEAN, side=(0, 0),
        tilt=0.0, nod=READY_NOD, look=0.9, L=FIST_L, R=FIST_R, fl=FOOT_L, fr=FOOT_R,
        grip=(0.25, 0.25), turn=READY_TURN, tuck=READY_TUCK):
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
    grip   how closed the (left, right) hands are: 0 open, 1 a fist
    turn   extra head turn to his left (toward the target), degrees
    tuck   degrees the neck tips back while the head tips forward as much:
           the head sits back over the shoulders instead of jutting
    """
    r = rig()
    c = {"offset": tuple(off)}
    c["hips"] = (tilt, 0, twist[0])
    c["abdomen"] = (lean[0], side[0], twist[1])
    c["chest"] = (lean[1], side[1], twist[2])
    yaw, bend, roll = sum(twist), lean[0] + lean[1] + tilt, side[0] + side[1]
    c["neck"] = (-0.3 * bend - tuck, -0.3 * roll, -0.35 * look * yaw + 0.35 * turn)
    c["head"] = (-0.4 * bend + nod + tuck, -0.4 * roll, -0.65 * look * yaw + 0.65 * turn)
    head = r.joint_at(c, "head")
    for s, arm in (("L", L), ("R", R)):
        sh = r.joint_at(c, f"shoulder_{s}")
        if "sh" in arm:                  # measured from the shoulder
            c[f"hand_{s}"] = tuple(sh[i] + arm["sh"][i] for i in range(3))
        else:                            # measured from the head
            c[f"hand_{s}"] = tuple(head[i] + arm["at"][i] for i in range(3))
        c[f"elbow_{s}"] = _rig.aim(sh, arm.get("elbow", (0, 0, -1)))
        c[f"wrist_{s}"] = tuple(arm.get("wrist", (0, 0, 0)))
        c[f"fore_{s}"] = (1.0, 1.0, 1.0)
    for s, foot in (("L", fl), ("R", fr)):
        c[f"foot_{s}"] = tuple(foot["at"])
        c[f"footrot_{s}"] = tuple(foot.get("rot", (0, 0, 0)))
        c[f"knee_{s}"] = _rig.aim(r.joint_at(c, f"hip_{s}"), foot.get("knee", (0, -1, 0)))
    for s, k in zip("LR", grip):
        c[f"j:fingers_{s}"] = c[f"j:tips_{s}"] = fist(s, k)
    return c


_STANCE = None


def stance():
    """The stance as joint angles: what the model stands in and what fills
    any joint a clip leaves alone."""
    global _STANCE
    if _STANCE is None:
        _STANCE = rig().solve(ctl())[0]
    return dict(_STANCE)


# ---- RMB: the visor beam (sheet RMB_animation.png) -------------------------
# A left-lead lunge that deepens as the charge builds: the right leg slides
# back and straightens, the left knee drives forward over the toe, the hips
# blade to his right while the head stays on the target. The right fist
# cocks back and up until the elbow is at shoulder height (the charge
# reads from the arm), the left fist tucks in at the belt. On release the
# spine pitches forward and the head drives the visor at the target.

def lunge(depth, reach=0.0):
    """Feet and hips for a lunge `depth` deep (0 = ready, 1 = full charge).
    `reach` pushes the hips forward over the lead knee (the release)."""
    d = depth
    fl = dict(at=(0.19 + 0.05 * d, -0.03 - 0.42 * d, FLOOR),
              rot=(0, 0, 8 + 4 * d), knee=(0.25, -1, 0))
    fr = dict(at=(-0.19 - 0.06 * d, 0.03 + 0.72 * d, FLOOR),
              rot=(0, 0, -8 - 32 * d), knee=(-0.4, -1, -0.2))
    off = (0, 0.10 * d - 0.10 * reach, -0.02 - 0.30 * d)
    return off, fl, fr


def beam_key(depth, lean, twist, R, L, nod=0.0, reach=0.0, grip=0.25):
    """`grip`: both fists close with the charge (0 open .. 1 clenched)."""
    off, fl, fr = lunge(depth, reach)
    return ctl(off=off, twist=twist, lean=lean, nod=nod, L=L, R=R, fl=fl, fr=fr,
               grip=(grip, grip), turn=0.0, tuck=0.0)


def animation():
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

    # ---- idle: the ready key, breathing --------------------------------------
    breathe = ctl(off=(0, 0, READY_OFF[2] + 0.006), lean=(3, -7),
                  L=dict(sh=(0.195, -0.03, -0.55), elbow=(0.45, 1, 0)),
                  R=dict(sh=(-0.195, -0.03, -0.55), elbow=(-0.45, 1, 0)))
    clip("idle", 12, 8, [(0, G), (6, breathe), (12, G)], loop=True, overlap=0.04)

    # Sheet keys 2 to 7.
    # Key 2, Initiate: he steps into it. The left foot plants well ahead
    # with the shin upright and the thigh driving forward; the right foot
    # slides back, its thigh nearly upright and the shin trailing. Hips stay
    # upright while the chest tips toward the target. The rear elbow swings
    # back behind him with the forearm hanging; the front arm hangs with the
    # forearm angled forward, fist by the front thigh.
    initiate = ctl(off=(0.0, 0.02, -0.11), twist=(-16, -6, -6), lean=(8, 14), side=(8, 12),
                   nod=-4, look=1.0, turn=0.0, tuck=0.0, grip=(0.45, 0.45),
                   R=dict(sh=(-0.07, 0.02, -0.48), elbow=(-0.3, 1, 0.1)),
                   L=dict(sh=(0.05, -0.24, -0.48), elbow=(0.3, 1, -0.1)),
                   fl=dict(at=(0.20, -0.32, FLOOR), rot=(0, 0, 12), knee=(0.2, -1, 0)),
                   fr=dict(at=(-0.24, 0.36, FLOOR), rot=(0, 0, -25), knee=(-0.2, -1, 0)))
    # Keys 2-3 are a row: he grips something in front of him and hauls it
    # in to brace against the blast coming out of his visor. The rear elbow
    # drives straight back and the fist comes in to the ribs, forearm
    # pointing forward (never hanging behind the hip: that reads as a hand
    # going to the back pocket). The front fist stays out ahead, gripping,
    # elbow tucked at his side.
    #
    # Key 3, Charge 10-33%: the lunge deepens. The rear leg runs back
    # nearly straight; the front thigh drops toward level over an upright
    # shin. The chest tips further over the front leg. Mid-row.
    # Key 3 measured from the posed 3D reference (Meshy "Key 3"): feet a
    # stance-length apart front to back and only modestly apart sideways,
    # front knee forward over the foot, rear leg long; hips to neck nearly
    # upright, the hunch is shoulders rolled forward and the head pushed
    # ahead; shoulders turned ~18 degrees (rear shoulder back); elbows out,
    # rear fist behind the hip, front fist ahead of it, both at belt height.
    charge_a = ctl(off=(0.0, -0.05, -0.18), twist=(-10, -4, -4), lean=(6, 10),
                   side=(0, 0), nod=8, look=1.0, turn=0.0, tuck=-22.0,
                   grip=(0.75, 0.75),
                   R=dict(sh=(-0.24, 0.20, -0.44), elbow=(-0.6, 0.8, -0.2)),
                   L=dict(sh=(0.26, -0.20, -0.46), elbow=(1, 0.2, -0.3)),
                   fl=dict(at=(0.29, -0.43, FLOOR), rot=(0, 0, 10), knee=(0.25, -1, 0)),
                   fr=dict(at=(-0.27, 0.43, FLOOR), rot=(0, 0, -30), knee=(-0.4, -1, 0)))
    charge_b = beam_key(0.90, grip=0.95, lean=(18, 14), twist=(-28, -12, -12), nod=-8,
                        R=dict(at=(-0.36, 0.44, -0.34), elbow=(-0.3, 1, 1.1)),
                        L=dict(at=(0.12, -0.36, -0.72), elbow=(1, 0.3, -0.3)))
    charge_c = beam_key(1.00, grip=1.0, lean=(20, 16), twist=(-30, -12, -12), nod=-8,
                        R=dict(at=(-0.36, 0.48, -0.30), elbow=(-0.3, 1, 1.2)),
                        L=dict(at=(0.10, -0.36, -0.72), elbow=(1, 0.3, -0.3)))
    release = beam_key(1.00, grip=1.0, lean=(26, 22), twist=(-30, -12, -12), nod=-20,
                       reach=1.0,
                       R=dict(at=(-0.34, 0.50, -0.38), elbow=(-0.3, 1, 1.0)),
                       L=dict(at=(0.08, -0.30, -0.76), elbow=(1, 0.4, -0.3)))
    recover = beam_key(0.85, grip=0.85, lean=(12, 10), twist=(-22, -8, -8), nod=-6,
                       R=dict(at=(-0.34, 0.30, -0.56), elbow=(-0.5, 1, 0.5)),
                       L=dict(at=(0.16, -0.28, -0.74), elbow=(1, 0.3, -0.3)))

    # Hold: from ready down into the full charge, then a small shiver at the
    # bottom that loops while the button is held.
    clip("beam_charge", 14, 12, [
        (0, G),
        (2, initiate, "out"),
        (5, charge_a),
        (9, charge_b),
        (13, charge_c),
    ])
    shiver = beam_key(1.02, grip=1.0, lean=(21, 17), twist=(-31, -12, -12), nod=-8,
                      R=dict(at=(-0.36, 0.50, -0.28), elbow=(-0.3, 1, 1.2)),
                      L=dict(at=(0.10, -0.36, -0.72), elbow=(1, 0.3, -0.3)))
    clip("beam_hold", 4, 12, [(0, charge_c), (2, shiver), (4, charge_c)], loop=True)

    # Release: the head snaps forward on frame 1, the fire is held, then he
    # rises out of the lunge (key 7) and steps back to ready.
    clip("beam", 12, 20, [
        (0, charge_c),
        (1, release, "snap"),
        (4, release),
        (7, recover, "in"),
        (11, G),
    ])
    return out.data()
