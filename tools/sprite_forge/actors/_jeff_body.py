"""Jeff (G3F-0427, the Recycler Bot): what his two bodies share.

Jeff has two generated models: on legs (art/models/Jeff.glb) and on tank
treads (art/models/JeffTreads.glb), from the same concept art. Each is its
own actor and its own sheet (jeff.py, jeff_treads.py); the game swaps the
sheet when They See Me Rollin' toggles. They share the arms, the spine and
every clip's upper body here, so the two read as one machine.

Who he is in motion: salvage pulled from the wreck and pointed at the crew's
enemies. Heavy, square and mechanical: the spine barely bends, the head
does the looking (a robot's tilt), and the right arm, the nozzle that both
cuts and vacuums, is always up and pointed. Nothing is graceful; every
move is a machine doing a job, with a jolt of recoil at the end.

The right arm is the nozzle: Thermal Disassembly fires from it, and
Acquire Biomass sucks through it. Reverse of Installation fires from the
port in his chest, so that clip throws the chest forward and the arms back.
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

RIM = (255, 150, 40)                 # hazard orange: his plating and eye
CELL = (192, 176, 136)               # wide: the nozzle arm and the tank on his back


class Body:
    """One of Jeff's bodies: a model, its fit and its rig."""

    def __init__(self, name, model, height, treads, centre_x=0.0, rigid=()):
        self.name = name
        self.rigid = tuple(rigid)
        self.centre_x = centre_x
        self.model = model
        self.height = height
        self.treads = treads
        self.fit = skin.load_fit(os.path.join(REPO, model))
        self._table = None
        self._rig = None
        self._stance = None

    def build(self):
        base = self.stance()
        m = Model(self.name)
        skin.attach(m, os.path.join(REPO, self.model), self.height, self.fit,
                    centre_x=self.centre_x, rigid=self.rigid)
        return m, base

    def joint_table(self):
        if self._table is None:
            data = skin.prepare(os.path.join(REPO, self.model), self.height,
                                self.fit, log=lambda *a: None,
                                centre_x=self.centre_x, rigid=self.rigid)
            bpy.data.objects.remove(data["obj"], do_unlink=True)
            self._table = [(n, p, tuple(float(c) for c in data["rest_pos"][n]))
                           for n, p in skin.HUMANOID]
        return self._table

    def rig(self):
        if self._rig is None:
            self._rig = _rig.Rig(self.joint_table())
        return self._rig

    def rest(self, joint):
        for n, _p, at in self.joint_table():
            if n == joint:
                return at
        raise KeyError(joint)

    # -- controls -----------------------------------------------------------
    def feet(self):
        """Where the feet stand. On treads they never move: the legs inside
        the treads stay as modelled, so the treads stay rigid."""
        if self.treads:
            l, r = self.rest("ankle_L"), self.rest("ankle_R")
            return (dict(at=l, rot=(0, 0, 0), knee=(0, -1, 0)),
                    dict(at=r, rot=(0, 0, 0), knee=(0, -1, 0)))
        floor = self.rest("ankle_L")[2]
        x = abs(self.rest("ankle_L")[0])
        # A heavy machine's stance: feet wider than the hips.
        return (dict(at=(x + 0.08, -0.04, floor), rot=(0, 0, 6), knee=(0.35, -1, 0)),
                dict(at=(-x - 0.08, 0.06, floor), rot=(0, 0, -6), knee=(-0.35, -1, 0)))

    def ctl(self, off=(0, 0, -0.03), twist=(0, 0, 0), lean=(2, 0), side=(0, 0),
            tilt=0.0, nod=-2.0, look=0.95, L=None, R=None, fl=None, fr=None):
        """A control (see isaac.py for the fields). On treads the hips stay
        put: only the upper body moves."""
        r = self.rig()
        L = L or IDLE_L
        R = R or READY_R
        dfl, dfr = self.feet()
        if self.treads:
            off = (0, 0, 0)
            tilt = 0.0
            fl, fr = dfl, dfr
        fl = fl or dfl
        fr = fr or dfr
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
            c[f"knee_{s}"] = _rig.aim(r.joint_at(c, f"hip_{s}"),
                                      foot.get("knee", (0, -1, 0)))
        if self.treads:
            # The legs exactly as modelled: the rest pose's own foot, foot
            # turn and knee, so the treads never twist.
            rest = r.control({}, (0, 0, 0))
            for s in ("L", "R"):
                for k in ("foot", "footrot", "knee"):
                    c[f"{k}_{s}"] = rest[f"{k}_{s}"]
        return c

    def stance(self):
        if self._stance is None:
            self._stance = self.rig().solve(self.ctl())[0]
        return dict(self._stance)

    def current(self):
        data = anim.load(os.path.join(REPO, self.model)) or animation(self)
        anim.check(data)
        return data

    def clips(self):
        return anim.bake(self.current(), self.rig(), self.stance())


# ---------------------------------------------------------------------------
# Arms, measured from the head joint (x his left, -y forward, z up)
# ---------------------------------------------------------------------------
# The nozzle (right arm) held ready: forearm level, pointing ahead.
READY_R = dict(at=(-0.30, -0.42, -0.62), elbow=(-0.7, 0.5, -0.4))
# The left hand hangs, a claw, a little out from the body.
IDLE_L = dict(at=(0.46, -0.10, -0.88), elbow=(0.8, 0.4, -0.3))
# Fired: the nozzle straight out at chest height.
AIM_R = dict(at=(-0.20, -0.70, -0.32), elbow=(-0.8, 0.2, -0.2))
KICK_R = dict(at=(-0.24, -0.52, -0.26), elbow=(-0.8, 0.5, -0.1))


def animation(body):
    """The starting point: every clip, by position."""
    r = body.rig()
    out = anim.Builder()
    ctl = body.ctl
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
        src = Clip(name, frames, fps, loop, keys)
        base, full = body.stance(), []
        for f in range(frames):
            t = f / frames if loop else f / max(1, frames - 1)
            pose, off, rot = forge.sample(src, t, base)
            full.append((f, r.control(pose, off), "linear", rot))
        if loop:
            full.append((frames, full[0][1], "linear", full[0][3]))
        out.clip(name, frames, fps, loop, overlap, full)

    # ---- idle: the reactor hums. A slow lift of the chest, and the head
    # tilts like a machine listening ------------------------------------------
    hum = ctl(off=(0, 0, -0.01), lean=(0, -2), side=(0, 2), nod=-6,
              L=dict(at=(0.47, -0.10, -0.86), elbow=(0.8, 0.4, -0.3)),
              R=dict(at=(-0.30, -0.44, -0.58), elbow=(-0.7, 0.5, -0.4)))
    clip("idle", 12, 8, [(0, G), (6, hum), (12, G)], loop=True, overlap=0.03)

    # ---- run -----------------------------------------------------------------
    if body.treads:
        # Rolling: hunched into the speed, the body shuddering on the treads.
        def roll(j):
            return ctl(lean=(8 + 1.5 * j, 6), nod=-4 + 3 * j,
                       L=dict(at=(0.40, 0.05, -0.80 + 0.02 * j), elbow=(0.8, 0.6, -0.2)),
                       R=dict(at=(-0.30, -0.46, -0.60 + 0.02 * j), elbow=(-0.7, 0.5, -0.4)))
        clip("run", 8, 16, [(0, roll(0)), (2, roll(1)), (4, roll(-0.5)),
                             (6, roll(1)), (8, roll(0))], loop=True)
    else:
        # A heavy stomp: short stride, the nozzle held steady ahead, the
        # left arm swinging stiffly.
        run = []
        for f in range(11):
            p = f / 10
            s = math.sin(p * 2 * math.pi)

            def leg(sign):
                swing = -28 * s * sign - 4
                fold = 10 + 62 * max(0.0, s * sign * -1)
                return (swing, 0, 0), (fold, 0, 0), (-4 - 8 * s * sign, 0, 0)

            hl, kl, al = leg(1)
            hr, kr, ar = leg(-1)
            pose = {
                "hips": (4, 0, 5 * s), "abdomen": (4, 0, -2 * s),
                "chest": (4, 0, -4 * s), "neck": (-4, 0, 1 * s), "head": (-6, 0, 2 * s),
                "hip_L": hl, "knee_L": kl, "ankle_L": al,
                "hip_R": hr, "knee_R": kr, "ankle_R": ar,
            }
            run.append(Key(p, pose, (0, -0.02, -0.07 + 0.06 * (1 - abs(s))),
                           ease="linear"))
        # Arms from the controls (the angles clip keeps the stance's arms).
        angles("run", 10, 13, True, run, overlap=0.03)

    # Every cast lands on the press: one frame of load, the strike on
    # frame 1 or 2, a held frame, then the way out.

    # ---- LMB: Thermal Disassembly. The nozzle snaps up and kicks ------------
    aim = ctl(lean=(2, 2), twist=(0, -4, -8), R=AIM_R)
    kick = ctl(lean=(-1, -3), twist=(0, -4, -6), R=KICK_R)
    clip("fire", 5, 24, [(0, aim, "snap"), (1, kick, "out"), (3, aim), (5, aim)])

    # ---- RMB held: Acquire Biomass. Braced, the nozzle on the floor ahead,
    # the left claw clamped on it, juddering as it sucks ----------------------
    def suck(j):
        return ctl(off=(0, 0.04, -0.14), lean=(14 + j, 12), nod=8,
                   fl=None, fr=None,
                   L=dict(at=(-0.06, -0.46, -0.78 + 0.02 * j), elbow=(0.8, -0.2, -0.4)),
                   R=dict(at=(-0.16, -0.58, -1.08 + 0.02 * j), elbow=(-0.8, 0.4, 0.0)))
    clip("harvest_charge", 6, 16, [(0, suck(0)), (2, suck(1.5)), (4, suck(-1)),
                                   (6, suck(0))], loop=True)
    # Release: a gulp. The nozzle jerks up, the body rocks back, done.
    clip("harvest", 8, 20, [
        (0, suck(0)),
        (1, ctl(off=(0, 0.02, -0.06), lean=(-4, -8), nod=-10,
                R=dict(at=(-0.26, -0.48, -0.52), elbow=(-0.8, 0.4, -0.2))), "snap"),
        (3, ctl(off=(0, 0.01, -0.04), lean=(-2, -4), nod=-6), "out"),
        (8, G),
    ])

    # ---- Q: Reverse of Installation. The chest port fires: he hunches over
    # it, then throws the chest forward and the arms back as it blasts ---------
    hunch = ctl(off=(0, 0.04, -0.12), lean=(16, 14), nod=10,
                L=dict(at=(0.20, -0.34, -0.70), elbow=(0.8, 0.3, -0.4)),
                R=dict(at=(-0.20, -0.36, -0.70), elbow=(-0.8, 0.3, -0.4)))
    blast = ctl(off=(0, 0.06, -0.06), lean=(-8, -14), nod=-12,
                L=dict(at=(0.62, 0.20, -0.62), elbow=(0.6, 0.6, -0.2)),
                R=dict(at=(-0.62, 0.20, -0.62), elbow=(-0.6, 0.6, -0.2)))
    clip("discharge", 12, 20, [
        (0, G),
        (1, hunch, "out"),
        (2, blast, "snap"),
        (5, blast),
        (7, ctl(off=(0, 0.02, -0.08), lean=(6, 4)), "in"),
        (12, G),
    ])

    # ---- LSHIFT: the reassembly. Folded down tight, then up and locked into
    # the new body (each sheet has the half it ends in) ------------------------
    fold = ctl(off=(0, 0.04, -0.30), lean=(24, 18), nod=16,
               L=dict(at=(0.26, -0.24, -0.66), elbow=(0.8, 0.2, -0.4)),
               R=dict(at=(-0.26, -0.30, -0.66), elbow=(-0.8, 0.2, -0.4)))
    clip("transform", 10, 20, [
        (0, fold),
        (3, fold),
        (5, ctl(off=(0, 0, 0.04), lean=(-4, -6), nod=-10), "snap"),
        (7, ctl(off=(0, 0, -0.04), lean=(4, 2)), "in"),
        (10, G),
    ])

    # ---- getting hit, and going down ----------------------------------------
    hurt = ctl(off=(0, 0.06, -0.06), lean=(-8, -12), twist=(0, 6, 12), nod=-16,
               L=dict(at=(0.56, 0.10, -0.70), elbow=(0.8, 0.6, -0.2)),
               R=dict(at=(-0.40, -0.20, -0.56), elbow=(-0.8, 0.6, -0.2)))
    clip("hit", 6, 20, [(0, G), (1, hurt, "out"), (3, hurt), (6, G)], overlap=0.02)
    # Death: he powers down rather than toppling. The knees give, he drops
    # to the floor with the head hanging, and settles with a small tip
    # forward. A full fall would leave the cell from some directions, and a
    # slumped machine reads as "down, revive me" from every side.
    slump = ctl(off=(0, 0.02, -0.36), lean=(26, 22), nod=24,
                L=dict(at=(0.40, -0.30, -1.00), elbow=(0.8, 0.2, -0.6)),
                R=dict(at=(-0.40, -0.30, -1.00), elbow=(-0.8, 0.2, -0.6)))
    if body.treads:
        down = ctl(lean=(30, 26), nod=30,
                   L=dict(at=(0.42, -0.36, -0.98), elbow=(0.8, 0.2, -0.6)),
                   R=dict(at=(-0.40, -0.40, -1.00), elbow=(-0.8, 0.2, -0.6)))
    else:
        floor = body.rest("ankle_L")[2]
        x = abs(body.rest("ankle_L")[0])
        down = ctl(off=(0, 0.10, -0.62), lean=(30, 26), nod=30,
                   fl=dict(at=(x + 0.10, -0.26, floor), rot=(0, 0, 14), knee=(0.4, -1, 0.4)),
                   fr=dict(at=(-x - 0.10, -0.20, floor), rot=(0, 0, -14), knee=(-0.4, -1, 0.4)),
                   L=dict(at=(0.42, -0.36, -0.98), elbow=(0.8, 0.2, -0.6)),
                   R=dict(at=(-0.40, -0.40, -1.00), elbow=(-0.8, 0.2, -0.6)))
    clip("death", 12, 12, [
        (0, hurt),
        (3, slump, "in"),
        (7, down, "in", (10, 0, 0)),
        (9, down, "out", (16, 0, 0)),
        (12, down, "smooth", (14, 0, 0)),
    ], overlap=0.02)
    return out.data()
