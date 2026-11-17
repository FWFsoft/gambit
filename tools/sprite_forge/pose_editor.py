#!/usr/bin/env python3
"""Write the pose editor page for an actor whose motion is data (anim.py).

    <forge python> tools/sprite_forge/pose_editor.py _wade_mesh out.html

The page shows the skinned model from the front, the side and the game's
camera. A human steps through each clip's keys and drags: a hand or a foot
goes where it is put and the limb follows (the same solver as
actors/_rig.py), the hips move the body over planted feet, the spine turns.
Bones keep their length, so nothing changes proportion. Saving stores the
whole animation dictionary.

An agent publishes the page as an Artifact (capabilities db and user), gives
the human the link, then reads the save back (ArtifactData get, collection
"anims", doc id the actor's NAME) and writes its `anim_json`, parsed, to
art/models/<Name>.anim.json. The steps are in
.claude/skills/character-art/SKILL.md, step 4.

The actor module needs: build(), rig(), current() returning the animation
dictionary, MODEL, FIT, NAME. Add --check to embed the rig's own answers so
the page's solver can be compared with it (window.__pose.verify()).

Reference poses: --ref "<clip>:<frame>=<model file>[=<label>]" (repeatable)
adds a posed model (an image-to-3D render of the key, say) as a
see-through ghost in all three views. It shows on that key by default; the
page's Reference card picks any ghost for any key and sets its size,
position and opacity. Ghosts are stored at 1.0 tall and sized on the page
to the rig's height in the key, so a crouch is compared with a crouch.
"""

import base64
import hashlib
import importlib
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "actors"))

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

import forge  # noqa: E402
import skin  # noqa: E402

TEXTURE = 1024


def b64(array):
    return base64.b64encode(np.ascontiguousarray(array).tobytes()).decode()


def mesh_payload(s):
    """The skin as flat arrays: one vertex per (mesh vertex, UV) pair."""
    mesh = s.obj.data
    mesh.calc_loop_triangles()
    tris = np.empty(len(mesh.loop_triangles) * 3, dtype=np.int32)
    mesh.loop_triangles.foreach_get("loops", tris)
    loop_vert = np.empty(len(mesh.loops), dtype=np.int32)
    mesh.loops.foreach_get("vertex_index", loop_vert)
    uv = np.zeros((len(mesh.loops), 2), dtype=np.float32)
    if mesh.uv_layers.active is not None:
        flat = np.empty(len(mesh.loops) * 2, dtype=np.float32)
        mesh.uv_layers.active.data.foreach_get("uv", flat)
        uv = flat.reshape(-1, 2)
    corner = np.concatenate([loop_vert[:, None].astype(np.float64),
                             np.round(uv, 5)], axis=1)
    unique, inverse = np.unique(corner, axis=0, return_inverse=True)
    vert = unique[:, 0].astype(np.int64)
    weights = np.round(s.w[vert] * 255).astype(np.uint8)
    return {
        "count": int(len(unique)),
        "position": b64(s.rest[vert].astype(np.float32)),
        "uv": b64(unique[:, 1:3].astype(np.float32)),
        "index": b64(inverse.reshape(-1)[tris].astype(np.uint32)),
        "skin_index": b64(s.idx[vert].astype(np.uint8)),
        "skin_weight": b64(weights),
        "skin_joints": list(s.joints),
        "skin_rest": [[round(float(c), 5) for c in p] for p in s.rest_pos],
    }


def texture_uri(s):
    """The mesh's base colour as a small JPEG."""
    # The forge swaps the mesh's own material for its render passes; the
    # texture lives on in the albedo pass materials.
    for mat in list(s.albedo_mats) + list(s.obj.data.materials):
        if mat is None or mat.node_tree is None:
            continue
        for node in mat.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image:
                img = node.image
                w, h = img.size
                px = np.empty(w * h * 4, dtype=np.float32)
                img.pixels.foreach_get(px)
                rgb = (np.clip(px.reshape(h, w, 4)[::-1, :, :3], 0, 1) * 255).astype(np.uint8)
                out = Image.fromarray(rgb)
                out.thumbnail((TEXTURE, TEXTURE), Image.LANCZOS)
                buf = io.BytesIO()
                out.save(buf, "JPEG", quality=88)
                return "data:image/jpeg;base64," + base64.b64encode(buf.getvalue()).decode()
    return None


def tips(model, fit):
    """Bone ends that are not joints (top of head, fingertips, toes), in the
    forge rest pose, so the head, hands and feet can be turned by dragging."""
    full = skin.complete_fit(fit, {})
    missing = [n for n, _ in skin.HUMANOID if n not in full]
    assert not missing, f"the fit has no position for {missing}; save one from the joint editor"
    xf, pos = skin.straighten(skin.HUMANOID, full, skin.HUMANOID_DOWN, skin.HUMANOID_LEVEL)
    out = {}
    for joint, tip in skin.HUMANOID_TAILS.items():
        if tip in dict(skin.HUMANOID) or tip not in full:
            continue
        lift = np.array(model.rest_world(joint)) - pos[joint]
        p = xf[joint][:3, :3] @ np.array(full[tip]) + xf[joint][:3, 3] + lift
        out[joint] = [round(float(c), 5) for c in p]
    return out


def ref_payload(spec):
    """One --ref: the posed model as a light, untextured ghost mesh."""
    key, path = spec.split("=", 1)
    label = None
    if "=" in path:
        path, label = path.split("=", 1)
    clip, frame = key.rsplit(":", 1)
    obj, v = skin.load_mesh(path, 1.0)
    mesh = obj.data
    mesh.calc_loop_triangles()
    tris = np.empty(len(mesh.loop_triangles) * 3, dtype=np.int32)
    mesh.loop_triangles.foreach_get("vertices", tris)
    bpy.data.objects.remove(obj, do_unlink=True)
    return {"clip": clip, "frame": int(frame),
            "label": label or f"{clip} {frame}",
            "position": b64(v.astype(np.float32)),
            "index": b64(tris.astype(np.uint32)),
            "count": int(len(v))}


def main():
    refs = []
    argv = []
    it = iter(sys.argv[1:])
    for a in it:
        if a == "--ref":
            refs.append(next(it))
        else:
            argv.append(a)
    sys.argv = [sys.argv[0]] + argv
    actor, out = sys.argv[1], sys.argv[2]
    mod = importlib.import_module(actor)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    data = mod.current()
    rig = mod.rig()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    model, _base = mod.build()
    assert model.skins, f"{actor} has no skinned mesh; the editor needs one"
    s = model.skins[0]
    path = mod.MODEL if os.path.isabs(mod.MODEL) else os.path.join(REPO, mod.MODEL)
    with open(path, "rb") as f:
        model_id = hashlib.sha1(f.read()).hexdigest()[:12]
    payload = {
        "name": mod.NAME, "model_id": model_id, "height": mod.HEIGHT,
        "elevation": forge.CAMERA_ELEVATION_DEG,
        "max_reach": getattr(rig, "max_reach", 0.997),
        "joints": [{"name": n, "parent": model.parent_of[n],
                    "rest": [round(float(c), 5) for c in model.rest_world(n)]}
                   for n in model.joints],
        "tips": tips(model, mod.FIT),
        "mesh": mesh_payload(s),
        "texture": texture_uri(s),
        "anim": data,
        "refs": [ref_payload(r) for r in refs],
    }
    if "--check" in sys.argv:
        payload["check"] = {}
        for name, c in data["controls"].items():
            c = {k: tuple(v) for k, v in c.items()}
            pose, off = rig.solve(c)
            payload["check"][name] = {j: [round(float(x), 5) for x in p]
                                      for j, (_r, p) in rig.fk(pose, off).items()}
    with open(os.path.join(HERE, "pose_editor.html")) as f:
        page = f.read()
    assert "/*POSE_DATA*/null" in page, "pose_editor.html lost its data slot"
    page = page.replace("/*POSE_DATA*/null", json.dumps(payload, separators=(",", ":")))
    page = page.replace("<title>Pose Editor</title>", f"<title>{mod.NAME} Pose Editor</title>")
    with open(out, "w") as f:
        f.write(page)
    print(f"wrote {out}: {os.path.getsize(out) / 1e6:.1f} MB, "
          f"{payload['mesh']['count']} vertices, {len(data['controls'])} controls, "
          f"{len(data['clips'])} clips")


if __name__ == "__main__":
    main()
