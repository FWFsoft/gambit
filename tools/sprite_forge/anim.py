"""Animation as data: controls and the clips that use them.

An actor can keep its motion as a plain dictionary instead of code, so that
a human can edit it in the pose editor (pose_editor.py) and the result can
be saved next to the model as art/models/<Name>.anim.json.

A control says where things are, not what the joint angles are (see
actors/_rig.py): where the hips are, the spine's angles, where each wrist
and ankle is, and which way each elbow and knee leans. The rig turns a
control into a pose, so hands land where they were put and planted feet
stay planted on any body.

    {
      "controls": {
        "guard": {"offset": [0, 0, -0.03], "hips": [0, 0, 10], ...,
                  "hand_L": [x, y, z], "elbow_L": [x, y, z], ...},
      },
      "clips": [
        {"name": "jab", "frames": 8, "fps": 24, "loop": False, "overlap": 0.0,
         "keys": [{"frame": 0, "control": "guard", "ease": "smooth"}, ...]},
      ],
    }

Several keys can use one control: change "guard" and every key that uses it
changes. A key may carry `rot`, a rotation of the whole body (a fall).
"""

import json
import os

from forge import Clip, Key


def _lerp(a, b, u):
    return tuple(a[i] + (b[i] - a[i]) * u for i in range(3))


def bake(data, rig, base):
    """The forge Clips for an animation dictionary: every frame is sampled
    from the control keys and solved. `base` fills joints the rig does not
    drive (hair, sash)."""
    from _rig import ease
    controls = {n: {k: tuple(v) for k, v in c.items()} for n, c in data["controls"].items()}
    clips = []
    for c in data["clips"]:
        keys = [(k["frame"], controls[k["control"]], k.get("ease", "smooth")) for k in c["keys"]]
        rots = [(k["frame"], tuple(k.get("rot", (0, 0, 0))), k.get("ease", "smooth")) for k in c["keys"]]
        out, prev = [], None
        for f in range(c["frames"]):
            pose, off = rig.solve(rig.sample(keys, f), prev)
            prev = pose
            rot = rots[-1][1] if f >= rots[-1][0] else rots[0][1]
            for (f0, r0, _), (f1, r1, e1) in zip(rots, rots[1:]):
                if f0 <= f <= f1:
                    rot = _lerp(r0, r1, ease(e1, (f - f0) / max(1e-6, f1 - f0)))
                    break
            full = dict(base or {})
            full.update(pose)
            t = f / c["frames"] if c["loop"] else f / max(1, c["frames"] - 1)
            out.append(Key(t, full, off, rot, "linear"))
        if c["loop"]:                      # close the cycle
            out.append(Key(1.0, out[0].pose, out[0].offset, out[0].rot, "linear"))
        clips.append(Clip(c["name"], c["frames"], c["fps"], c["loop"], out,
                          overlap=c.get("overlap", 0.0)))
    return clips


class Builder:
    """Collects controls and clips into the dictionary, giving one name to
    controls that are the same."""

    def __init__(self):
        self.controls, self.clips, self._seen = {}, [], {}

    def control(self, c, name):
        c = {k: [round(float(x), 4) for x in v] for k, v in c.items()}
        sig = json.dumps(c, sort_keys=True)
        if sig not in self._seen:
            assert name not in self.controls, f"two controls called '{name}'"
            self.controls[name] = c
            self._seen[sig] = name
        return self._seen[sig]

    def clip(self, name, frames, fps, loop, overlap, keys):
        """keys: [(frame, control, ease, rot)]"""
        out = []
        for frame, c, how, rot in keys:
            k = {"frame": frame, "control": self.control(c, f"{name} {frame:g}"), "ease": how}
            if any(abs(r) > 1e-9 for r in rot):
                k["rot"] = [round(float(r), 3) for r in rot]
            out.append(k)
        self.clips.append({"name": name, "frames": frames, "fps": fps, "loop": loop,
                           "overlap": overlap, "keys": out})

    def data(self):
        return {"controls": self.controls, "clips": self.clips}


def path_for(model_path):
    return os.path.splitext(model_path)[0] + ".anim.json"


def load(model_path):
    """The animation a human saved from the pose editor, or None."""
    path = path_for(model_path)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


NEEDED = ("offset", "hips", "abdomen", "chest", "neck", "head") + tuple(
    f"{part}_{s}" for s in "LR" for part in ("hand", "elbow", "foot", "footrot", "knee"))


def check(data):
    """Crash on an animation that would not play."""
    for name, c in data["controls"].items():
        missing = [k for k in NEEDED if k not in c]
        assert not missing, f"control '{name}' has no {missing}"
    for c in data["clips"]:
        assert c["keys"], f"clip '{c['name']}' has no keys"
        frames = [k["frame"] for k in c["keys"]]
        assert frames == sorted(frames), f"clip '{c['name']}': keys out of order"
        for k in c["keys"]:
            assert k["control"] in data["controls"], \
                f"clip '{c['name']}': no control '{k['control']}'"
