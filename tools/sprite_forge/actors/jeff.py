"""Jeff (G3F-0427, the Recycler Bot) on legs.

The body is art/models/Jeff.glb (Meshy, from the concept art), on the joints in
art/models/Jeff.fit.json. The motion and the reasons for it are shared with
his other body, in _jeff_body.py. Preview with preview.py jeff fire out.png.
"""


import _jeff_body

NAME = "Jeff"
MODEL = "art/models/Jeff.glb"
HEIGHT = 2.05                          # a head taller than Wade: the tank and eye stalk
YAW = 0.0
CENTRE_X = 0.042                     # the nozzle arm widens the torso band the automatic
                                     # centring uses: feet, chest and head all sit 4.2 cm left of it
STYLE = "ink"
RIM = _jeff_body.RIM
CELL = _jeff_body.CELL
FOLLOW = {}

BODY = _jeff_body.Body(NAME, MODEL, HEIGHT, treads=False,
                       centre_x=CENTRE_X)
FIT = BODY.fit


def build():
    return BODY.build()


def joint_table():
    return BODY.joint_table()


def rig():
    return BODY.rig()


def stance():
    return BODY.stance()


def current():
    return BODY.current()


def clips():
    return BODY.clips()
