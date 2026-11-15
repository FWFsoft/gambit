"""Animation as data: named poses and the clips that use them.

An actor can describe its motion as a plain dictionary instead of code, so
that a human can edit it in the pose editor (pose_editor.py) and the result
can be saved next to the model as art/models/<Name>.anim.json.

    {
      "poses": {
        "guard":    {"base": None,    "joints": {"hips": [0, 0, 10], ...}},
        "jab load": {"base": "guard", "joints": {"shoulder_L": [-34, -16, 14]}},
      },
      "clips": [
        {"name": "jab", "frames": 8, "fps": 24, "loop": False, "overlap": 0.008,
         "keys": [{"t": 0.0, "pose": "guard", "offset": [0, 0, -0.03],
                   "rot": [0, 0, 0], "ease": "smooth"}, ...]},
      ],
    }

A pose with a `base` only lists the joints it changes; the rest follow the
base, so fixing the guard fixes every pose built on it. A joint is three
rotations in degrees, optionally followed by a scale (one number, or three)
for a smear frame. A key may carry `add`: rotations added on top of its pose
(the idle is the guard plus a bounce).
"""

import json
import os

from forge import Clip, Key


def resolve(poses, name):
    """A pose's full joint table, with its bases filled in."""
    seen, chain = set(), []
    while name is not None:
        assert name in poses, f"no pose called '{name}'"
        assert name not in seen, f"pose '{name}' is its own base"
        seen.add(name)
        chain.append(poses[name])
        name = poses[name].get("base")
    out = {}
    for pose in reversed(chain):
        out.update({j: tuple(v) for j, v in pose["joints"].items()})
    return out


def key_pose(poses, key):
    pose = resolve(poses, key["pose"])
    for joint, d in (key.get("add") or {}).items():
        r = pose.get(joint, (0, 0, 0))
        pose[joint] = tuple(r[i] + d[i] for i in range(3)) + tuple(r[3:])
    return pose


def to_clips(data):
    """The forge Clips for an animation dictionary."""
    clips = []
    for c in data["clips"]:
        keys = [Key(k["t"], key_pose(data["poses"], k), tuple(k.get("offset", (0, 0, 0))),
                    tuple(k.get("rot", (0, 0, 0))), k.get("ease", "smooth"))
                for k in c["keys"]]
        clips.append(Clip(c["name"], c["frames"], c["fps"], c["loop"], keys,
                          overlap=c.get("overlap", 0.0)))
    return clips


def path_for(model_path):
    return os.path.splitext(model_path)[0] + ".anim.json"


def load(model_path):
    """The animation a human saved from the pose editor, or None."""
    path = path_for(model_path)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def check(data, joints):
    """Crash on an animation that would not play: unknown poses or joints."""
    for name, pose in data["poses"].items():
        assert pose.get("base") is None or pose["base"] in data["poses"], \
            f"pose '{name}': no base pose '{pose['base']}'"
        for joint, v in pose["joints"].items():
            assert joint in joints, f"pose '{name}': no joint '{joint}'"
            assert len(v) in (3, 4, 6), f"pose '{name}', {joint}: {v}"
    for c in data["clips"]:
        assert c["keys"], f"clip '{c['name']}' has no keys"
        for k in c["keys"]:
            assert k["pose"] in data["poses"], \
                f"clip '{c['name']}': no pose '{k['pose']}'"
