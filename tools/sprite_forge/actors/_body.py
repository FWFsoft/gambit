"""The heroic body kit: a fighting-game body built from proportions.

Every hero starts here so the cast shares one anatomy language (3rd Strike
and MvC2 exaggeration: big hands and forearms, broad chests over narrow
waists, strong legs) while each character's proportions make them their own
shape. Characters add costume and props on top.

Skeleton (faces -Y, its left is +X):

    hips - abdomen - chest - neck - head
                     chest - shoulder_L/R - elbow - wrist
    hips - hip_L/R - knee - ankle

The spine has three joints so the torso bends and twists instead of
swinging as one block. Rotation conventions are the forge's (see Model).
"""

from dataclasses import dataclass, field
from typing import Tuple

from forge import Model


@dataclass
class Build:
    """Proportions. Defaults are an athletic hero about 2 units tall."""
    head: float = 0.27            # head width
    shoulders: float = 0.30       # shoulder joints' distance from the midline
    chest: Tuple[float, float, float] = (0.62, 0.32, 0.34)   # ribcage size
    waist: float = 0.26           # waist width
    hip_width: float = 0.15       # hip joints' distance from the midline
    upper_arm: Tuple[float, float, float] = (0.10, 0.12, 0.08)   # radii
    forearm: Tuple[float, float, float] = (0.085, 0.11, 0.075)
    hand: float = 0.18            # fist size
    thigh: Tuple[float, float, float] = (0.15, 0.16, 0.10)
    calf: Tuple[float, float, float] = (0.10, 0.12, 0.07)
    foot: Tuple[float, float, float] = (0.13, 0.28, 0.09)
    arm_length: float = 1.0
    leg_length: float = 1.0
    neck: float = 0.085
    deltoid: float = 2.3          # shoulder cap size, x the biceps radius
    pecs: float = 1.0             # chest muscle size
    square: float = 0.0           # clavicle bar radius: squares the shoulder line

    # Joint heights at leg_length = 1 (scaled from there).
    def z(self, base):
        return base


def joint_table(b: Build):
    """The body's joints as [(name, parent, rest position)], parents first.
    Pure numbers, so pose tools (_rig.py) can work without a Model."""
    L = b.leg_length
    hip_z = 0.97 * L
    table = [
        ("hips", None, (0, 0, hip_z + 0.03)),
        ("abdomen", "hips", (0, 0, hip_z + 0.17)),
        ("chest", "abdomen", (0, 0, hip_z + 0.35)),
        ("neck", "chest", (0, 0, hip_z + 0.60)),
        ("head", "neck", (0, -0.01, hip_z + 0.70)),
    ]
    sh_z = hip_z + 0.53
    upper = 0.28 * b.arm_length
    fore = 0.26 * b.arm_length
    for side, x in (("L", 1), ("R", -1)):
        sx = b.shoulders * x
        table.append((f"shoulder_{side}", "chest", (sx, 0, sh_z)))
        table.append((f"elbow_{side}", f"shoulder_{side}",
                      (sx + 0.02 * x, 0, sh_z - upper)))
        table.append((f"wrist_{side}", f"elbow_{side}",
                      (sx + 0.03 * x, 0, sh_z - upper - fore)))
        hx = b.hip_width * x
        table.append((f"hip_{side}", "hips", (hx, 0, hip_z)))
        table.append((f"knee_{side}", f"hip_{side}", (hx + 0.01 * x, 0, 0.53 * L)))
        table.append((f"ankle_{side}", f"knee_{side}", (hx + 0.01 * x, 0, 0.10 * L)))
    return table


def skeleton(m: Model, b: Build):
    """Joints for the body. Returns key heights for costume building."""
    for name, parent, pos in joint_table(b):
        m.joint(name, parent, pos)
    L = b.leg_length
    hip_z = 0.97 * L
    return {"hip_z": hip_z, "shoulder_z": hip_z + 0.53,
            "upper": 0.28 * b.arm_length, "fore": 0.26 * b.arm_length}


def torso(m: Model, b: Build, mat, waist_mat=None):
    """Ribcage, lats, pecs, abdomen: a hard V."""
    H = skeleton_heights(m)
    cw, cd, ch = b.chest
    hz = H["hips"]
    waist_mat = waist_mat or mat
    m.blob("abdomen", waist_mat, (0, 0.0, hz + 0.18), (b.waist, b.waist * 0.75, 0.26),
           segments=16, rings=10, smooth=True)
    m.blob("chest", mat, (0, 0.02, hz + 0.40), (cw, cd, ch), segments=18,
           rings=12, smooth=True)
    if b.square > 0:
        # A bar from shoulder to shoulder: a square, trapezoid top instead of
        # a round barrel.
        m.tube("chest", mat, [(-b.shoulders * 0.95, 0.0, hz + 0.52),
                              (0, -0.02, hz + 0.53),
                              (b.shoulders * 0.95, 0.0, hz + 0.52)],
               [b.square * 0.8, b.square, b.square * 0.8], sides=12)
    for x in (1, -1):
        m.blob("chest", mat, (0.10 * x * cw / 0.6, -cd * 0.33, hz + 0.43),
               (cw * 0.36 * b.pecs, cd * 0.32 * b.pecs, ch * 0.45 * b.pecs),
               segments=12, rings=8, smooth=True)                  # pecs
        m.tube("chest", mat, [(0.07 * x, 0.03, hz + 0.58),
                              (b.shoulders * 0.62 * x, 0.04, hz + 0.57),
                              (b.shoulders * 0.95 * x, 0.02, hz + 0.53)],
               [b.neck * 0.9, b.neck * 1.1, b.neck * 0.7], sides=10)  # traps


def head(m: Model, b: Build, skin, neck=True):
    """Neck and skull. Faces and hair are the character's."""
    H = skeleton_heights(m)
    hz = H["hips"]
    if neck:
        m.tube("neck", skin, [(0, 0, hz + 0.56), (0, -0.01, hz + 0.70)],
               [b.neck, b.neck * 0.95])
    w = b.head
    m.blob("head", skin, (0, -0.02, hz + 0.81), (w, w * 1.05, w * 1.15),
           segments=16, rings=12, smooth=True)
    m.blob("head", skin, (0, -0.07, hz + 0.73), (w * 0.72, w * 0.7, w * 0.42),
           segments=12, rings=8, smooth=True)                      # jaw


def arms(m: Model, b: Build, upper_mat, fore_mat, hand_mat, sides=("L", "R")):
    """Deltoids, biceps, heavy forearms and big fists."""
    for side in sides:
        x = 1 if side == "L" else -1
        sh, el, wr = f"shoulder_{side}", f"elbow_{side}", f"wrist_{side}"
        s, e, w = (m.rest_world(sh), m.rest_world(el), m.rest_world(wr))
        r0, r1, r2 = b.upper_arm
        d = b.deltoid
        m.blob(sh, upper_mat, (s[0], s[1], s[2] - 0.01),
               (r1 * d, r1 * d * 0.95, r1 * d * 0.92), segments=14, rings=10,
               smooth=True)                                         # deltoid
        m.tube(sh, upper_mat, [s, ((s[0] + e[0]) / 2, -0.015, (s[2] + e[2]) / 2), e],
               [r0, r1, r2])
        m.blob(el, fore_mat, e, (r2 * 2.0, r2 * 2.0, r2 * 2.0), segments=10,
               rings=8, smooth=True)                                # elbow
        f0, f1, f2 = b.forearm
        m.tube(el, fore_mat, [e, (e[0], e[1] - 0.01, e[2] - 0.09), w], [f0, f1, f2])
        m.blob(wr, hand_mat, (w[0], w[1] - 0.01, w[2] - b.hand * 0.42),
               (b.hand, b.hand * 1.05, b.hand * 1.0), segments=12, rings=10,
               smooth=True)                                         # fist


def legs(m: Model, b: Build, thigh_mat, calf_mat, foot_mat):
    for side in ("L", "R"):
        hp, kn, an = f"hip_{side}", f"knee_{side}", f"ankle_{side}"
        h, k, a = m.rest_world(hp), m.rest_world(kn), m.rest_world(an)
        t0, t1, t2 = b.thigh
        m.tube(hp, thigh_mat, [h, ((h[0] + k[0]) / 2, -0.02, (h[2] + k[2]) / 2), k],
               [t0, t1, t2])
        m.blob(kn, calf_mat, k, (t2 * 2.0, t2 * 2.0, t2 * 2.0), segments=10,
               rings=8, smooth=True)
        c0, c1, c2 = b.calf
        m.tube(kn, calf_mat, [k, (k[0], k[1] + 0.02, k[2] - 0.14), a], [c0, c1, c2])
        fw, fl, fh = b.foot
        m.blob(an, foot_mat, (a[0], a[1] - fl * 0.28, fh * 0.45), (fw, fl, fh),
               segments=12, rings=8, smooth=True)


def skeleton_heights(m: Model):
    return {"hips": m.rest_world("hips")[2] - 0.03}
