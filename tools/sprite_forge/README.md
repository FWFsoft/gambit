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

Write a shape brief first (`docs/design/ART_DIRECTION.md`): the dominant
shape, the signature piece that breaks the outline, the idle pose. Then
copy `actors/toxlith.py` (creature) or `actors/wade.py` (character) and
give it its own proportions:

- `build()` returns `(Model, rest_pose)`
- `clips()` returns `Clip`s; each clip is a list of `Key(t, pose, root_offset,
  root_rotation)`
- optional `CELL = (width, height, feet_line)` in standard pixels
- optional `PIXEL_SCALE` (characters use 1.35): chunkier art pixels at
  the same on-screen size; the sheet is rendered smaller and the game draws
  it larger
- preview at a given pixel scale:
  `/opt/forge/bin/python tools/sprite_forge/preview.py wade idle out.png 0 1.35`

After baking, run the silhouette check: the "who's that?" test from the
art on-ramp. It writes a lineup of black silhouettes at in-game size and
flags pairs that are too alike:

```bash
python3 tools/sprite_forge/silhouette.py            # forge actors
python3 tools/sprite_forge/silhouette.py --legacy   # plus the old cut-outs
```

A pose value may carry a fourth number, a scale: `"sword": (0, 0, 0, 0)`
hides the joint and everything on it. Gravon uses this to wear his sword
on his back and draw it into his hand only for attacks. Scale steps between
keys rather than blending.

Clip names the game looks for: `idle`, `run`, `hit`, `death`, and
`attack` for enemies. Character abilities map to clips in `actionClip()`
in `src/ArenaView.cpp` (Wade: `jab` / `cross` on LMB, `uppercut_R` /
`uppercut_L` on RMB, `slip` on Q, `roll` on Shift).

## Body kit and motion

Characters start from `actors/_body.py`. A `Build` sets their proportions;
`skeleton`, `torso`, `head`, `arms` and `legs` build the body, and the
character adds costume and props. See Wade.

Motion tools, all in `forge.py`:
- **Overlapping action:** `Clip(..., overlap=0.01)` delays each joint by
  that many seconds per level below the hips, so motion ripples outward.
- **Secondary motion:** `FOLLOW = {"hair": ("head", gain, stiffness,
  damping, drag)}` in an actor module springs a joint after its driver.
- **Squash and stretch:** a six-value pose `(rx, ry, rz, sx, sy, sz)`
  scales a joint in its rest axes (a forearm that stretches on a punch).

## Toon style

Set `STYLE = "toon"` and a signature `RIM = (r, g, b)` in an actor module
to render smooth art instead of pixel art. It renders at twice the
resolution, then averages down. The shading is:
- three cel bands,
- ink on the outer contour (`TOON_OUTLINE`), on overlaps and on creases
  (`CREASE_DEG`),
- a rim light from behind (`RIM_LIGHT`).

The sheet is marked `smooth` in the manifest, so the game filters it
linearly. Use `PIXEL_SCALE` below 1 (Gravon uses 0.75) so the sheet is
drawn close to 1:1 on screen.

For organic shapes, use `m.tube(joint, material, points, radii)`: a smooth
tube through control points, with the radius easing through `radii`. Use
it for muscles, hair spikes, tails, horns and cloth flaps. For smooth bodies,
use `blob(..., smooth=True)` with more segments.

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
