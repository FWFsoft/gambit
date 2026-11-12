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

Look for `art/models/<Name>.glb`. If there is none, you cannot make one:
you have no image-generation or image-to-3D tool. Ask the human for it and
point them at `art/models/README.md`, which says exactly what to generate
(A-pose, nothing in the hands, textured GLB). Meanwhile you can still do
steps 1, 4 and 5 against the character's existing code-built actor, since
poses and clips carry over.

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
guess. It is usually close on hips, knees and ankles and wrong on the
neck, head and shoulders. Correct it:

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
