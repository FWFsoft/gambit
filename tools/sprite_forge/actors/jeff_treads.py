"""Jeff (G3F-0427, the Recycler Bot) on tank treads (They See Me Rollin').

The body is art/models/JeffTreads.glb (Meshy, from the concept art), on the joints in
art/models/JeffTreads.fit.json. The motion and the reasons for it are shared with
his other body, in _jeff_body.py. Preview with preview.py jeff_treads fire out.png.
"""


import _jeff_body

NAME = "JeffTreads"
MODEL = "art/models/JeffTreads.glb"
HEIGHT = 2.05                          # a head taller than Wade: the tank and eye stalk
YAW = 0.0
CENTRE_X = 0.019                     # the nozzle arm pulls the automatic centring 2 cm
                                     # off the line through the treads, chest and head
STYLE = "ink"
RIM = _jeff_body.RIM
CELL = _jeff_body.CELL
FOLLOW = {}

# The treads and the chassis between them are hard parts, not legs: bolted
# to the hips so they never bend or smear. The claw and the nozzle hang
# further out than the chassis box, so they stay on the arms.
RIGID = (("hips", 0.70, 9.0),      # the treads: everything below their tops
         ("hips", 0.95, 0.40))     # the chassis up to the hips
BODY = _jeff_body.Body(NAME, MODEL, HEIGHT, treads=True,
                       centre_x=CENTRE_X, rigid=RIGID)
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
