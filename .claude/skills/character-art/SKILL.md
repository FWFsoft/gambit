---
name: character-art
description: Create or upgrade an in-game character or enemy sprite from a generated 3D model (image-to-3D GLB) with the sprite forge, fit, pose, bake and check it. Use when asked to make, redo or improve a character's or creature's in-game art, add a new actor to the forge, or when a new file appears in art/models/.
---

# Character art: from a generated model to an in-game sprite

Read `docs/design/ART_DIRECTION.md` first ("The look" and the character's
shape brief). The division of labour it sets out is the point of this
skill:

- **The drawing comes from the concept art**, through an image-to-3D
  model in `art/models/<Name>.glb`. Do not model a body from blobs and
  tubes in code; that route was tried and cannot carry the designs.
- **Code does the motion, the ink, the light and the consistency.** That
  is this skill.

`<py>` below is the forge interpreter:
`tools/sprite_forge/.venv/Scripts/python.exe` on Windows,
`tools/sprite_forge/.venv/bin/python` elsewhere. Setup is in
`tools/sprite_forge/build.py`. If it is missing, create it (it needs `uv`
and downloads Blender's Python module, about 400 MB: tell the user).

## 0. Is there a model?

Look for `art/models/<Name>.glb`. If there is none, it has to be made from
the concept art with an image-to-3D tool (`art/models/README.md` says what
to generate: A-pose, nothing in the hands, textured GLB):

- The human may already have made one on the Meshy website. Check with
  `python tools/sprite_forge/meshy.py list` (free), download it with
  `meshy.py fetch <Name> <task id>` (free), and if it is too heavy cut it
  down with `<py> tools/sprite_forge/shrink.py` (free).
- Otherwise `meshy.py generate <Name> <images>` makes one. **This spends
  Meshy credits, which are money.** Run it without `--approve-credits`
  first, tell the user the cost and the balance it prints, and wait for a
  yes. One yes covers one run; never retry a paid command on your own.
  The rules are in `art/models/README.md`, "Spending credits".
- You cannot draw the input images. If there is no A-pose turnaround, ask
  the human for one; a splash in a fighting stance makes a bad model.
- No key or no network: ask the human for the model.

Look at `art/models/<Name>.preview.png` before going on. Meanwhile you can
still do steps 1, 4 and 5 against the character's existing code-built
actor, since poses and clips carry over.

## 1. Shape brief

If `docs/design/ART_DIRECTION.md` has no brief for this character, write
one (a few lines: dominant shape, signature piece, idle pose) from the
splash art in `assets/previews/<Name>.png` and the Character Concepts doc.
Look at the splash art yourself with the Read tool.

## 2. Fit the skeleton

```bash
<py> tools/sprite_forge/fit.py art/models/<Name>.glb <scratch>/fit.png --height 1.95
```

Open the image. Four panels: front and side with joints, skin weights by
joint, and the forge rest pose. The printed `FIT = {...}` is an automatic
guess. It is usually close on the arms below the shoulder, the knees and
the ankles, and wrong on the spine (hips, abdomen, chest) and shoulders.

**Let the human place the joints.** They see where a shoulder belongs
faster than you can dial it in from a picture:

1. Add `--editor <scratch>/<name>_fit.html` to the command above. It writes
   a page with the front and side views and draggable joints.
2. Publish that page with the Artifact tool, with
   `capabilities: {"db": {}, "user": {}}`, and give the human the link. To
   update a page you already published for this character, publish to the
   same link.
3. When they say they have saved, read the fit with the `ArtifactData`
   tool (`action: "get"`, `collection: "fits"`, `doc_id: "<Name>"`) and
   write its `name`, `model_id` and `fit` to
   `art/models/<Name>.fit.json`. The actor picks it up through
   `skin.load_fit()`.
4. Run `fit.py <actor> out.png` again and look at panels 3 and 4, which
   the page cannot show: the weights and the rest pose. If they are wrong,
   say what you see and send the human back to the same link.

What right looks like, for you and for them:

- every joint sits inside the body at the real joint. Shoulders belong
  inside the deltoids, not on the neck; the neck at the base of the
  skull; the head joint at ear height;
- in the weights panel, colours change at the joints and no arm colour
  leaks onto the torso or thigh. A leak means the arms were too close to
  the body in the model: ask for an A-pose regeneration rather than
  fighting it;
- the rest-pose panel is the same character with arms at the sides.

Heights for scale: Wade 1.95, Presidente about 2.2, a small enemy 0.8.
If the model faces the wrong way, pass `--yaw 180` (and set `YAW`).

## 3. Write the actor

Copy `tools/sprite_forge/actors/_mesh_template.py` to
`actors/<name>.py`. Set `NAME`, `MODEL`, `HEIGHT`, `RIM` (the character's
signature colour) and paste the corrected `FIT`. Re-run step 2 with the
actor name (`fit.py <name> out.png`) until it is right.

If the character already has a code-built actor, keep its poses, clips,
`FOLLOW`, `SOCKET` and prop code, and replace only the body: remove the
`_body.skeleton / torso / head / arms / legs` calls and the costume parts
that the model now carries, and call `skin.attach(...)` instead. Joints
keep their names, so the clips still apply. Arm and leg lengths will
differ a little: re-measure poses that depend on reach (a guard with the
gloves at the cheeks, hands on hips) by reading joint positions, not by
eye.

Parts that must move on their own stay in code, on their own joints: a
weapon that changes hands or is hidden (`"sword": (0, 0, 0, 0)`), cloth
on springs (`FOLLOW`). Give them `m.material(...)` colours picked from the
model's texture so they sit in the same drawing.

## 4. Pose and animate

This is where the game feel is made; spend the effort here. The rules are
in `ART_DIRECTION.md` ("Proportions and motion") and
`tools/sprite_forge/README.md`: anticipation, a snap with a stretched
limb, a held impact, recoil; hips lead and the fist arrives last
(`overlap`); secondary motion on springs. Preview constantly:

```bash
<py> tools/sprite_forge/preview.py <name> idle <scratch>/idle.png
<py> tools/sprite_forge/preview.py <name> jab <scratch>/jab.png 0,3,5
```

Look at every preview with the Read tool. Check each clip from at least
the front (column 1), the side (column 3) and the back (column 5).

**Do not change the character's proportions to fix a pose.** The model and
the fitted skeleton are the character. Scale on a joint is for the smear
frames of a hit only (a forearm that stretches, a fist that swells), used
sparingly; an idle or any held pose has none. If the face or the hands do
not read, fix the pose, or say that the model needs regenerating.

**Let the human pose the keys.** When a body changes (a new model, a new
fit), poses that put a hand somewhere exact (a guard at the cheeks) land
wrong. Get them close by measuring (`measure.py <actor> <pose function>`
prints where the hands are from the head), then hand over the pose editor:

1. The actor keeps its motion as data: named poses and the clips that use
   them (`anim.py`; `actors/_wade_mesh.py` is the example). It has
   `current()`, which returns the saved animation or the starting point.
2. `<py> tools/sprite_forge/pose_editor.py <actor> <scratch>/<name>_poses.html`
   writes a page with the skinned model from the front, the side and the
   game camera. The human steps through each clip's keys and drags joints;
   bones keep their length.
3. Publish it with the Artifact tool, with
   `capabilities: {"db": {}, "user": {}}`, and give the human the link.
4. When they say they have saved, read it with `ArtifactData`
   (`action: "get"`, `collection: "anims"`, `doc_id: "<Name>"`, with
   `out_dir` since it is large) and write its `anim` to
   `art/models/<Name>.anim.json`. Then render previews with the real ink
   shading and look at them: the page shows the flat texture only, with no
   follow-through or springs.

Clip names the game needs: `idle`, `run`, `hit`, `death`, `attack` for
enemies, and one per ability, mapped in `actionClip()` in
`src/ArenaView.cpp`.

## 5. Bake and check

```bash
<py> tools/sprite_forge/build.py <name>
python tools/sprite_forge/silhouette.py
```

- Read the silhouette lineup. A flagged look-alike weakens Clarity; fix
  the shape or the idle pose.
- Crop a few cells from `assets/sprites/actors/<Name>.png` and compare
  them with the splash art side by side at 3x. Ask: would a player who
  knows the splash art recognise this at a glance? If the face or the
  signature piece is lost at this size, exaggerate it (scale the head
  joint a little, thicken the prop) rather than adding detail.
- Sheet size: keep a hero under about 4096 x 4096. Trim frames or lower
  `PIXEL_SCALE` resolution before adding directions.
- If the game builds on this machine, run `/visual-test` or record frames
  and look at the character in a fight. If it does not, say so plainly in
  your summary: the sprite is verified in previews only.

If you changed `skin.py`, `forge.py` shading or the fit code, also run
`<py> tools/sprite_forge/selftest_skin.py`.

## 6. Log it

Run `/design-review`. Add the entry to `docs/design/DECISIONS.md`
(status "Built, needs review") with the pillar that decided each choice,
and add or update the shape brief. Put a before/after image in
`docs/prototype/` when the look changed.

## When the result is not good enough

Be honest about which layer is failing, and fix that layer:

| Symptom | Layer | Fix |
| --- | --- | --- |
| Face is mush, proportions generic, costume wrong | the model | regenerate (better input image, multi-view, A-pose); do not paint over it in code |
| Limbs tear, hands drag the hips, shoulders collapse | the fit | correct `FIT`; check the weights panel; if arms touch the body, regenerate in A-pose |
| Looks stiff or weightless in motion | the clips | timing, overlap, stretch, held impact |
| Reads flat or muddy at game size | the ink | `INK_*` constants in `forge.py` change the whole cast together; change them deliberately and rebake everyone |
| Looks fine alone, lost in the game | the presentation | floor values, light, shadow, glow: see "The look" in `ART_DIRECTION.md` |
