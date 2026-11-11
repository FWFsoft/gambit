# Sprite forge

In-game character and creature art, built by Claude as code.

Each actor in `actors/` is a Python file that builds a low-poly 3D model
from simple parts (blobs, tapered limbs, boxes), hangs it on a small
joint skeleton, and defines animation clips as keyframed poses.
`build.py` renders every clip in all 8 directions from the game camera and
turns the renders into pixel art with one shared set of rules (light
direction, colour ramps, outlines, pixel size) in `forge.py`. Those shared
rules are what keep every character and creature looking like part of one
game.

No image model draws anything. Your concept art and turnarounds are the
reference Claude models from.

## Setup (once)

```bash
python3 -m venv /opt/forge
/opt/forge/bin/pip install bpy numpy pillow
```

`bpy` is Blender as a Python module; it needs the Python version its wheel
was built for (3.13 for bpy 5.3).

## Use

```bash
/opt/forge/bin/python tools/sprite_forge/build.py              # every actor
/opt/forge/bin/python tools/sprite_forge/build.py wade toxlith  # some
```

It writes `assets/sprites/actors/<Name>.png` and `manifest.txt`. The game
uses an actor's baked sheet automatically for any character or enemy with
the same name; everything else falls back to the older placeholder sprites.
A full character with 10 clips bakes in about 30 seconds.

While modelling or animating:

```bash
/opt/forge/bin/python tools/sprite_forge/preview.py wade jab /tmp/jab.png
python3 tools/sprite_forge/showcase.py /tmp/all.gif Wade:1 Toxlith:7
```

## Adding an actor

Copy `actors/toxlith.py` (creature) or `actors/wade.py` (character):

- `build()` returns `(Model, rest_pose)`
- `clips()` returns `Clip`s; each clip is a list of `Key(t, pose, root_offset,
  root_rotation)`
- optional `CELL = (width, height, feet_line)` in pixels

Clip names the game looks for: `idle`, `run`, `hit`, `death`, and
`attack` for enemies. Character abilities map to clips in `actionClip()`
in `src/ArenaView.cpp` (Wade: `jab` / `cross` on LMB, `uppercut_R` /
`uppercut_L` on RMB, `slip` on Q, `roll` on Shift).

## Style knobs (`forge.py`)

`PIXEL_SIZE` (overall sprite scale), `KEY_LIGHT` / `FILL_LIGHT`,
`make_ramp()` (shadow and highlight hue shifts), `OUTLINE_RGB`,
`DEPTH_EDGE` (how readily internal lines are drawn). Change one, rebake,
and the whole cast changes together.

## Carry sockets

An actor can set `SOCKET = ("wrist_L", "wrist_R")`. The forge then writes a
`socket <Actor> <clip> x,y,roll,front ...` line per clip: for every
direction and frame, where the midpoint of those joints is (pixels from the
feet pivot), the roll of whatever is being carried (the `carry` pseudo-joint's
x rotation in the pose, in degrees: 0 upright, 90 lying across the hands,
180 head-down) and whether it is in front of the body. The game pins a
grappled foe to that point, so El Presidente's held foe follows his hands
through leaps, the Látigo whirl and the sit-out powerbomb with no extra code
per move.
