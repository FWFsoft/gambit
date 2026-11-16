"""NyxCRT: Nyx's CRT on the floor, where she set it down.

While Nyx is a swarm the game draws this sheet at the spot she left
(Player::propX/propY), facing the way she faced when she dropped it. The set
sits where her "drop" clip leaves it: in front of her feet and a little to
her right, measured from that clip's last pose (nyx.crouch(nyx.DEEP)). So the
swap from her sprite to this one reads as the set touching down.

One still frame per direction.
"""

import os

from forge import Clip, Key, Model
import nyx

NAME = "NyxCRT"
STYLE = "ink"
RIM = (90, 220, 255)                 # the screen's cyan
CELL = (112, 96, 72)

# Centre of the set at the end of the drop, from Nyx's feet (x her left,
# -y her front), and how far it is turned.
SPOT = (-0.14, -0.345)
TURN = 8.0


def build():
    m = Model(NAME)
    m.joint("crt", None, (SPOT[0], SPOT[1], 0.0))
    nyx.attach_prop(m, os.path.join(nyx.REPO, nyx.CRT_MODEL), "crt",
                    nyx.CRT_HEIGHT, (0.0, 0.0, nyx.CRT_HEIGHT / 2.0))
    return m, {"crt": (0, 0, TURN)}


def clips():
    still = {"crt": (0, 0, TURN)}
    return [Clip("idle", 1, 1, True, [Key(0.0, still, (0, 0, 0)),
                                      Key(1.0, still, (0, 0, 0))])]
