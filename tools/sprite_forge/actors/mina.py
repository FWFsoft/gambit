"""Mina, the Professional (Striker). Body from art/models/Mina.glb; her
bombs are art/models/MinaBomb.glb (the small ball she passes) and
art/models/MinaBigBomb.glb (the oversized one she carries on her back and
kicks with the Long Shot).

Shape brief: a street footballer in tactical kit. Bandaged and armoured
legs, shoulder-mounted grenade canisters, a high ponytail. The big bomb
on her back is the signature piece: it rounds off her outline from every
side and says "bombs" before she does anything. She stands like a player
waiting for the pass: weight on the left leg, right foot forward and
turned out, hands loose on the hips.

Kit (Character Concepts doc; src/ArenaData.cpp):
  attack     Pass the Ball: a quick side-foot pass along the ground.
  secondary  The Long Shot (hold): she swings the big bomb off her back,
             drops it, steps back and blasts it. The first 45% is the
             wind-up the client holds while charging.
  special    GOAL!!!!: the celebration, arms up, as the bombs rain.
  mobility   Handball!: the ball tucked under her arm, sprinting.
"""

import math
import os

from forge import Model
import _hero as h
from _hero import HIDE, SHOW, with_
import skin

NAME = "Mina"
MODEL = "art/models/Mina.glb"
BALL = "art/models/MinaBomb.glb"
BIG = "art/models/MinaBigBomb.glb"
HEIGHT = 1.8
YAW = 0.0
STYLE = "ink"
RIM = (255, 150, 180)                # her pink
CELL = (192, 176, 136)
K = HEIGHT / 1.95
BALL_SIZE = 0.30
BIG_SIZE = 0.72

FIT = skin.load_fit(os.path.join(h.REPO, MODEL))


def build():
    m = Model(NAME)
    skin.attach(m, os.path.join(h.REPO, MODEL), HEIGHT, FIT, yaw=YAW)
    h.prop(m, BIG, "big_back", "chest", h.back(m, 0.30, 0.12), BIG_SIZE)
    # On the ground in front of her right foot, ready for the Long Shot.
    h.prop(m, BIG, "big_ground", None, (-0.10, -0.55, BIG_SIZE / 2), BIG_SIZE,
           material="big_ground")
    # The pass: a ball at her right instep.
    h.prop(m, BALL, "ball", None, (-0.14, -0.30, BALL_SIZE / 2), BALL_SIZE)
    # Handball: tucked under the right arm.
    h.prop(m, BALL, "ball_hand", "wrist_R", h.palm(m, "R"), BALL_SIZE,
           material="ball_hand")
    return m, stand()


def props(big_back=SHOW, big_ground=HIDE, ball=HIDE, ball_hand=HIDE):
    return {"big_back": (0, 0, 0, big_back), "big_ground": (0, 0, 0, big_ground),
            "ball": (0, 0, 0, ball), "ball_hand": (0, 0, 0, ball_hand)}


def stand(breath=0.0):
    """Waiting for the pass: weight on the left leg, the right foot out
    front and turned out, hands on her hips."""
    return dict(props(), **{
        "hips": (0, 0, 6),
        "abdomen": (2 + 1 * breath, 0, -2),
        "chest": (2 + 2 * breath, 0, -4),
        "neck": (0, 0, 0),
        "head": (-4, 0, 4),
        "shoulder_L": (6, -26 - 2 * breath, 0), "elbow_L": (-86, 0, 0),
        "shoulder_R": (6, 26 + 2 * breath, 0), "elbow_R": (-86, 0, 0),
        "hip_L": (2, -4, -4), "knee_L": (6, 0, 0), "ankle_L": (-2, 2, 0),
        "hip_R": (-14, 10, 18), "knee_R": (8, 0, 0), "ankle_R": (-4, -4, 0),
    })


def loose(breath=0.0):
    """The run's base: arms free (the hands-on-hips idle would lock them)."""
    return with_(stand(breath), shoulder_L=(0, -10, 0), shoulder_R=(0, 10, 0),
                 hip_R=(-4, 4, 0))


def o(x, y, z):
    return (x * K, y * K, z * K)


def clips():
    s = stand()
    run = h.run_pose(loose, lean=10, arm=40, leg=38, knee=70)

    # Pass the Ball: plant the left foot, swing the right leg through and
    # strike with the inside of the foot.
    plant = with_(s, **props(ball=SHOW), hips=(0, 0, -16), chest=(6, 0, 14),
                  shoulder_L=(-30, -36, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(20, 40, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-10, -6, 0), knee_L=(22, 0, 0),
                  hip_R=(36, 8, 20), knee_R=(70, 0, 0), ankle_R=(10, 0, 0))
    pas = with_(s, **props(ball=HIDE), hips=(0, 0, 14), chest=(4, 0, -12),
                shoulder_L=(10, -44, 0), elbow_L=(-30, 0, 0),
                shoulder_R=(-30, 30, 0), elbow_R=(-30, 0, 0),
                hip_L=(-10, -6, 0), knee_L=(24, 0, 0),
                hip_R=(-56, 2, 30), knee_R=(12, 0, 0), ankle_R=(-6, 0, 0))

    # The Long Shot: reach back for the bomb, set it down, step back, and
    # a full laces-through blast.
    reach = with_(s, chest=(-6, 0, 20), head=(-8, 0, 30),
                  shoulder_R=(-150, 40, 0), elbow_R=(-60, 0, 0),
                  shoulder_L=(-150, -40, 0), elbow_L=(-60, 0, 0))
    drop = with_(s, **props(big_back=HIDE, big_ground=SHOW),
                 hips=(-10, 0, 0), abdomen=(14, 0, 0), chest=(18, 0, 0),
                 shoulder_L=(-50, -16, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(-50, 16, 0), elbow_R=(-20, 0, 0),
                 hip_L=(-24, -6, 0), knee_L=(40, 0, 0),
                 hip_R=(-20, 6, 0), knee_R=(40, 0, 0))
    cock = with_(s, **props(big_back=HIDE, big_ground=SHOW),
                 hips=(4, 0, -10), abdomen=(-6, 0, 0), chest=(-12, 0, 10),
                 head=(10, 0, 0),
                 shoulder_L=(-70, -50, 0), elbow_L=(-20, 0, 0),
                 shoulder_R=(40, 50, 0), elbow_R=(-20, 0, 0),
                 hip_L=(-20, -6, 0), knee_L=(30, 0, 0),
                 hip_R=(50, 6, 0), knee_R=(100, 0, 0), ankle_R=(30, 0, 0))
    blast = with_(s, **props(big_back=HIDE, big_ground=HIDE),
                  hips=(-8, 0, 10), abdomen=(-10, 0, 0), chest=(-16, 0, -8),
                  head=(-6, 0, 0),
                  shoulder_L=(-40, -70, 0), elbow_L=(-10, 0, 0),
                  shoulder_R=(-10, 70, 0), elbow_R=(-10, 0, 0),
                  hip_L=(-10, -6, 0), knee_L=(24, 0, 0),
                  hip_R=(-100, 4, 0), knee_R=(6, 0, 0), ankle_R=(-20, 0, 0))

    # GOAL!!!!: knee slide energy without the slide: both fists up, head
    # back, a jump.
    cheer = with_(s, hips=(0, 0, 0), abdomen=(-8, 0, 0), chest=(-16, 0, 0),
                  head=(-24, 0, 0),
                  shoulder_L=(-170, -24, 0), elbow_L=(-30, 0, 0),
                  shoulder_R=(-170, 24, 0), elbow_R=(-30, 0, 0),
                  hip_L=(-4, -8, 0), knee_L=(10, 0, 0), ankle_L=(30, 0, 0),
                  hip_R=(-4, 8, 0), knee_R=(10, 0, 0), ankle_R=(30, 0, 0))
    crouch = with_(s, hips=(-20, 0, 0), abdomen=(20, 0, 0), chest=(20, 0, 0),
                   shoulder_L=(20, -20, 0), elbow_L=(-60, 0, 0),
                   shoulder_R=(20, 20, 0), elbow_R=(-60, 0, 0),
                   hip_L=(-60, -8, 0), knee_L=(90, 0, 0), ankle_L=(-20, 0, 0),
                   hip_R=(-60, 8, 0), knee_R=(90, 0, 0), ankle_R=(-20, 0, 0))

    # Handball!: the ball tucked under the right arm, sprinting low.
    def handball(phase):
        p = run(phase)
        return with_(p, **props(ball_hand=SHOW), chest=(18, 0, p["chest"][2]),
                     shoulder_R=(-20, 20, 0), elbow_R=(-100, 0, 0))

    return [
        h.breathe(stand, K),
        h.stride(run, K),
        h.shot("attack", 7, 16, [
            (0.0, s, o(0, 0, 0)),
            (0.45, plant, o(0, 0.04, -0.04)),
            (0.65, pas, o(0, -0.06, -0.02), "out"),
            (1.0, s, o(0, 0, 0))]),
        h.shot("secondary", 12, 14, [
            (0.0, s, o(0, 0, 0)),
            (0.2, reach, o(0, 0, 0)),
            (0.32, drop, o(0, 0, -0.10)),
            (0.45, cock, o(0, 0.18, -0.03)),
            (0.6, blast, o(0, -0.10, 0.02), "out"),
            (0.8, blast, o(0, -0.12, 0.0)),
            (1.0, s, o(0, 0, 0))]),
        h.shot("special", 10, 12, [
            (0.0, s, o(0, 0, 0)),
            (0.25, crouch, o(0, 0, -0.22)),
            (0.5, cheer, o(0, 0, 0.30), "out"),
            (0.7, cheer, o(0, 0, 0.18)),
            (1.0, s, o(0, 0, 0), "in")]),
        h.stride(handball, K, name="mobility", fps=14, bounce=0.05),
    ] + h.hit_and_death(stand, K)
