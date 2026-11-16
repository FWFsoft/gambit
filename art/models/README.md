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

## Generating one with the Meshy API

`tools/sprite_forge/meshy.py` does the trip from concept art to
`art/models/<Name>.glb` without the Meshy website. It needs a Meshy API key
(`MESHY_API_KEY`, or the key saved on the Claude Code cloud environment).
Set the variable in your shell, or copy `.env.example` to `.env` at the repo
root and put the key there; `.env` is git-ignored, so the key stays off
GitHub.

```bash
python tools/sprite_forge/meshy.py balance                 # free
python tools/sprite_forge/meshy.py list                    # free: tasks on the account
python tools/sprite_forge/meshy.py fetch Wade <task id>    # free: a model made on the website
python tools/sprite_forge/meshy.py generate Wade front.png side.png back.png   # 30 credits
python tools/sprite_forge/meshy.py remesh Wade --task <task id>                # 5 credits
```

`generate` sends the settings in the table below (A-pose, 40,000 triangles,
2K base colour only). Give the front view first; one image works, a
turnaround of two to four is better. It saves the model, Meshy's preview
render (`<Name>.preview.png`: look at it) and a record of the task
(`<Name>.meshy.json`), and checks the triangle count and file size.

A model made on the website usually comes back far too heavy (Wade was
700,000 triangles and 44 MB). `tools/sprite_forge/shrink.py` cuts it down
locally in Blender for free; prefer it to `remesh`:

```bash
<forge python> tools/sprite_forge/shrink.py big.glb art/models/Wade.glb
```

Download links on a Meshy task expire after about three days, so fetch a
model you want to keep.

### Spending credits

Credits are bought with real money and shared by the team. These rules are
for anyone, human or agent, running the tool; agents must follow them
exactly.

1. **Ask first, every time.** `generate` and `remesh` print the cost and
   the balance and stop. Tell the user that cost, the balance, and what the
   run is for, and wait for a yes in the conversation. Only then run it
   again with `--approve-credits <cost>`.
2. **One yes, one run.** An approval covers that single command at that
   cost. A second attempt, a different character or a different setting is
   a new question. "Go ahead with Wade" is not approval for the cast.
3. **No retries on your own.** If the result is bad (mushy face, wrong
   pose), show the user the preview and ask before generating again. Never
   put a paid command in a loop or a script.
4. **Free first.** Use `fetch` for a model that already exists, `shrink.py`
   in place of `remesh`, and `--from-task` only when new input images are
   not needed.
5. **Say what was spent.** After a paid run, report the credits spent and
   the balance left. If the spend differs from the printed cost, say so:
   Meshy's prices have changed and the table in `meshy.py` needs fixing.
6. **Stay inside the tool.** Do not call paid Meshy endpoints any other way
   (curl, the Meshy CLI, an MCP server), where nothing checks the cost.

Prices when this was written (2026-11): a textured model 30 credits (15
with `--model meshy-6-lite`, for drafts), a remesh 5. A failed task is
refunded by Meshy; a bad-looking one is not.

## How to generate one by hand (on the tool's website)

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
