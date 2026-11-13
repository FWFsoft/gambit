"""Pose by position: forward and inverse kinematics for the body kit.

Body-kit poses are joint angles, which are fine for a guard or a run cycle
but hopeless for matching a reference ("the fist ends here, the rear heel
is up, the feet do not slide"). This module lets a clip be authored in
*control space*:

    offset            where the hips are (root translation)
    hips..head        spine angles, as in a normal pose
    hand_L / hand_R   where the wrist is, in model space
    elbow_L / elbow_R a point the elbow leans toward (model space)
    foot_L / foot_R   where the ankle is
    footrot_L / _R    the foot's world rotation (pitch, roll, yaw)
    knee_L / knee_R   a point the knee leans toward

`Rig.solve` turns a control into an ordinary pose, and `Rig.clip` samples a
list of control keys once per frame and solves every frame, so hands travel
on the paths that were authored and planted feet stay planted. Angles are
interpolated only between neighbouring frames, which also keeps Euler
flips out of the picture.

Conventions (same as forge.py): the character faces -y, his left is +x,
z is up; rotations are XYZ Euler degrees; limbs hang along -z at rest.
"""

import math

from mathutils import Euler, Matrix, Vector

from forge import Clip, Key

SPINE = ("hips", "abdomen", "chest", "neck", "head")
SIDES = ("L", "R")


def _mat(r):
    return Euler([math.radians(a) for a in r[:3]], "XYZ").to_matrix()


def _deg(mat, near=None):
    if near is not None:
        e = mat.to_euler("XYZ", Euler([math.radians(a) for a in near[:3]], "XYZ"))
    else:
        e = mat.to_euler("XYZ")
    return tuple(math.degrees(a) for a in e)


def _frame(axis, toward):
    """Orthonormal frame: first column along `axis`, second as close to
    `toward` as it can be."""
    x = axis.normalized()
    y = toward - x * toward.dot(x)
    if y.length < 1e-6:
        y = Vector((0, 1, 0)) - x * x.y
        if y.length < 1e-6:
            y = Vector((1, 0, 0)) - x * x.x
    y.normalize()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed()


HINT_DISTANCE = 5.0


def hint(origin, mid, end):
    """A far-away point in the direction the middle joint (elbow, knee)
    sticks out from the origin-to-end line. Far away, so the direction
    barely changes when the body moves and hints blend sensibly."""
    axis = (end - origin)
    out = mid - origin
    if axis.length > 1e-6:
        axis = axis.normalized()
        out = out - axis * out.dot(axis)
    if out.length < 1e-4:
        out = Vector((0, 0, -1))
    return tuple(origin + out.normalized() * HINT_DISTANCE)


def aim(origin, direction):
    """A hint from a direction: "elbow out to the side", "knee forward"."""
    return tuple(Vector(origin) + Vector(direction).normalized() * HINT_DISTANCE)


def lerp(a, b, u):
    return tuple(a[i] + (b[i] - a[i]) * u for i in range(len(a)))


def ease(kind, u):
    if kind == "linear":
        return u
    if kind == "in":
        return u * u
    if kind == "out":
        return 1 - (1 - u) * (1 - u)
    if kind == "snap":                      # most of the way, at once
        return 1 - (1 - u) ** 3
    return u * u * (3 - 2 * u)


class Rig:
    def __init__(self, table):
        self.order = [n for n, _, _ in table]
        self.parent = {n: p for n, p, _ in table}
        self.rest = {n: Vector(pos) for n, _, pos in table}
        self.local = {n: (self.rest[n] - self.rest[p]) if p else self.rest[n].copy()
                      for n, p, _ in table}

    # ---- forward ---------------------------------------------------------
    def fk(self, pose, offset=(0, 0, 0)):
        """{joint: (world rotation, world position)} for a pose."""
        out = {}
        for n in self.order:
            p = self.parent[n]
            rl = _mat(pose.get(n, (0, 0, 0)))
            if p is None:
                out[n] = (rl, self.local[n] + Vector(offset))
            else:
                rp, pp = out[p]
                out[n] = (rp @ rl, pp + rp @ self.local[n])
        return out

    def control(self, pose, offset=(0, 0, 0)):
        """The control that reproduces a pose."""
        w = self.fk(pose, offset)
        c = {"offset": tuple(offset)}
        for n in SPINE:
            c[n] = tuple(pose.get(n, (0, 0, 0))[:3])
        for s in SIDES:
            c[f"hand_{s}"] = tuple(w[f"wrist_{s}"][1])
            c[f"elbow_{s}"] = hint(w[f"shoulder_{s}"][1], w[f"elbow_{s}"][1],
                                   w[f"wrist_{s}"][1])
            c[f"wrist_{s}"] = tuple(pose.get(f"wrist_{s}", (0, 0, 0)))
            c[f"fore_{s}"] = (1.0, 1.0, 1.0)
            c[f"foot_{s}"] = tuple(w[f"ankle_{s}"][1])
            c[f"footrot_{s}"] = _deg(w[f"ankle_{s}"][0])
            c[f"knee_{s}"] = hint(w[f"hip_{s}"][1], w[f"knee_{s}"][1],
                                  w[f"ankle_{s}"][1])
        return c

    def ride(self, c, pose, sides=SIDES):
        """Put the hands where `pose`'s arm angles carry them on control
        `c`'s spine: a guard that travels with the torso."""
        full = dict(pose)
        for n in SPINE:
            full[n] = c[n]
        w = self.fk(full, c["offset"])
        out = dict(c)
        for s in sides:
            out[f"hand_{s}"] = tuple(w[f"wrist_{s}"][1])
            out[f"elbow_{s}"] = hint(w[f"shoulder_{s}"][1], w[f"elbow_{s}"][1],
                                     w[f"wrist_{s}"][1])
            out[f"wrist_{s}"] = tuple(pose.get(f"wrist_{s}", (0, 0, 0)))
            out[f"fore_{s}"] = (1.0, 1.0, 1.0)
        return out

    def joint_at(self, c, joint):
        """Where a spine-driven joint (a shoulder, a hip) is for control c."""
        return self.fk({n: c[n] for n in SPINE}, c["offset"])[joint][1]

    # ---- inverse ---------------------------------------------------------
    def _two_bone(self, rp, origin, target, pole, mid, end, sign, near):
        """Angles for a 3-DOF joint at `origin` and a hinge (local x) at
        `mid` that put `end` on `target`, the middle leaning toward `pole`."""
        u0, l0 = self.local[mid], self.local[end]
        t = Vector(target) - origin
        reach = u0.length + l0.length
        d = max(0.12 * reach, min(0.997 * reach, t.length))
        if t.length < 1e-6:
            t = Vector((0, -1, 0))

        def tip(theta):
            return u0 + Matrix.Rotation(theta, 3, "X") @ l0

        lo, hi = 0.0, math.radians(176.0)       # bend: 0 straight .. folded
        for _ in range(40):
            m = (lo + hi) / 2
            if tip(sign * m).length > d:
                lo = m
            else:
                hi = m
        theta = sign * (lo + hi) / 2
        e = tip(theta)
        local = _frame(e, u0)
        world = _frame(t, Vector(pole) - origin)
        r = world @ local.transposed()
        return _deg(rp.transposed() @ r, near), math.degrees(theta), r

    def solve(self, c, near=None):
        """Control -> (pose, offset). `near` is the previous frame's pose, so
        angles stay continuous."""
        near = near or {}
        pose = {n: tuple(c[n]) for n in SPINE}
        w = self.fk(pose, c["offset"])
        for s in SIDES:
            sh, el, wr = f"shoulder_{s}", f"elbow_{s}", f"wrist_{s}"
            rp, origin = w["chest"][0], w[sh][1]
            ang, bend, _ = self._two_bone(rp, origin, c[f"hand_{s}"],
                                          c[f"elbow_{s}"], el, wr, -1,
                                          near.get(sh))
            pose[sh] = ang
            pose[el] = (bend, 0.0, 0.0) + tuple(c.get(f"fore_{s}", (1.0, 1.0, 1.0)))
            pose[wr] = tuple(c.get(wr, (0, 0, 0)))

            hp, kn, an = f"hip_{s}", f"knee_{s}", f"ankle_{s}"
            rp, origin = w["hips"][0], w[hp][1]
            ang, bend, r = self._two_bone(rp, origin, c[f"foot_{s}"],
                                          c[f"knee_{s}"], kn, an, 1,
                                          near.get(hp))
            pose[hp] = ang
            pose[kn] = (bend, 0.0, 0.0)
            shin = r @ Matrix.Rotation(math.radians(bend), 3, "X")
            pose[an] = _deg(shin.transposed() @ _mat(c[f"footrot_{s}"]),
                            near.get(an))
        for k, v in c.items():                  # extra joints pass through
            if k.startswith("j:"):
                pose[k[2:]] = tuple(v)
        return pose, tuple(c["offset"])

    # ---- clips -----------------------------------------------------------
    @staticmethod
    def blend(a, b, u):
        out = {}
        for k in a:
            va, vb = a[k], b.get(k, a[k])
            if len(va) != len(vb) or k.startswith("fore_") or \
                    k.startswith("wrist_"):
                out[k] = vb if u >= 0.5 else va     # scales step
            else:
                out[k] = lerp(va, vb, u)
        for k in b:
            if k not in a:
                out[k] = b[k]
        return out

    def sample(self, keys, f):
        """Control at frame `f` from [(frame, control, ease)]."""
        if f <= keys[0][0]:
            return keys[0][1]
        if f >= keys[-1][0]:
            return keys[-1][1]
        for (f0, c0, _), (f1, c1, e1) in zip(keys, keys[1:]):
            if f0 <= f <= f1:
                return self.blend(c0, c1, ease(e1, (f - f0) / max(1e-6, f1 - f0)))
        return keys[-1][1]

    def clip(self, name, frames, fps, keys, base=None):
        """A one-shot Clip from control keys [(frame, control[, ease])]:
        sampled and solved once per frame. `base` fills joints the rig does
        not drive (hair, sash)."""
        keys = [(k[0], k[1], k[2] if len(k) > 2 else "smooth") for k in keys]
        out, prev = [], None
        for f in range(frames):
            pose, off = self.solve(self.sample(keys, f), prev)
            prev = pose
            full = dict(base or {})
            full.update(pose)
            out.append(Key(f / max(1, frames - 1), full, off, ease="linear"))
        return Clip(name, frames, fps, False, out)
