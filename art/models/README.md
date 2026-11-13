# Character models (source art)

One generated 3D model per character: `art/models/<Name>.glb`. The sprite
forge puts it on the forge skeleton, animates it and bakes the in-game
sprite sheet from it (`.claude/skills/character-art/SKILL.md`).

These are source files. They are not shipped: the game only loads the baked
sheets in `assets/sprites/actors/`. Keep them out of `assets/`, which is
bundled whole into the web build.

## Why a generated model

The concept art holds the design: the face, the muscle, the cloth folds,
the ink. A model built from blobs in code cannot carry that, however it is
shaded. An image-to-3D tool turns the concept art into a mesh whose shape
and texture carry the drawing. Code then does what code is good at: motion,
lighting, ink, eight directions, consistency across the cast.

## How to generate one (for the human with the tool account)

Any image-to-3D tool that exports a textured GLB works (Meshy, Tripo,
Hunyuan3D, Rodin, ...).

**Input image**

- Best: the character's **turnaround** (front, side and back views), using
  the tool's multi-view input.
- The character stands in an **A-pose**: arms straight and held about 45
  degrees out from the body, legs a little apart, feet flat, facing the
  camera. Make an A-pose turnaround with the AI art pipeline if the splash
  art is in a fighting stance. Most tools also have an "A-pose" or
  "T-pose" switch: turn it on.
- Nothing in the hands. Weapons, shields and anything the character draws,
  throws or drops are made separately (as their own GLB, or in code) so
  they can move on their own.
- Long loose cloth (capes, sashes, coat tails) is better left off the body
  and added in code, where it swings on springs. Short, tight costume
  stays on the model.

Why the A-pose matters: the forge decides which joint moves each part of
the surface. Arms resting against the body or a fighting guard glue hands
to hips and forearms to chests.

**Settings**

| Setting | Value |
| --- | --- |
| Format | GLB (FBX and OBJ also load) |
| Texture | on; base colour only, 1024 or 2048 px. Prefer a flat or "unlit" texture if the tool offers it: the forge adds its own light and ink |
| Triangles | 20,000 to 60,000 |
| Rig / animation | not needed; any rig in the file is ignored |
| Symmetry | on, unless the design is asymmetric (Wade's tattoo sleeve) |

**Check before handing it over**

- The silhouette matches the character's shape brief in
  `docs/design/ART_DIRECTION.md`. Regenerate if the proportions drifted to
  generic "game character".
- The face reads. Faces are where generators fail most often.
- File size under about 15 MB.

Save it as `art/models/<Name>.glb` with the same name the game uses
(`Wade`, `Presidente`, `Gravon`, ...), then ask a Claude Code session to
"make <Name> from the model".
