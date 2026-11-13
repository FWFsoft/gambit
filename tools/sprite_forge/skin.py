"""Skinned, textured meshes for the forge.

Code-built blobs cannot carry a character design: nobody sculpts a face or
cloth folds by typing coordinates. The drawing has to come from the concept
art. An image-to-3D tool (Meshy, Tripo, Hunyuan3D, ...) turns a splash or a
turnaround into a textured mesh; this module puts that mesh on the forge's
joints so every existing clip, direction, socket and shading rule applies
to it.

    body = skin.attach(m, "art/models/Wade.glb", height=1.95, fit=FIT)

What attach() does:

  1. imports the file, merges it into one mesh, stands it on the ground,
     scales it to `height` and centres it (model faces -Y, its left is +X);
  2. places the joints from `fit` (positions in that normalised space, in
     the pose the mesh was generated in, usually an A-pose). Missing joints
     are estimated; see auto_fit() and tools/sprite_forge/fit.py;
  3. computes skin weights (Blender's bone-heat on a voxel proxy, so
     non-manifold AI meshes work), cached next to the model file;
  4. re-poses the mesh into the forge rest pose (arms and legs straight
     down), so poses written for the body kit mean the same thing here;
  5. creates the forge joints and registers the mesh on the Model. From
     then on Model.pose() deforms it.

The mesh keeps its own texture. The renderer adds an albedo pass for it
and the "ink" style shades it (see forge.shade_ink).
"""

import hashlib
import math
import os

import bpy
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

# The body kit's skeleton (actors/_body.py): (joint, parent).
HUMANOID = [
    ("hips", None), ("abdomen", "hips"), ("chest", "abdomen"),
    ("neck", "chest"), ("head", "neck"),
    ("shoulder_L", "chest"), ("elbow_L", "shoulder_L"), ("wrist_L", "elbow_L"),
    ("shoulder_R", "chest"), ("elbow_R", "shoulder_R"), ("wrist_R", "elbow_R"),
    ("hip_L", "hips"), ("knee_L", "hip_L"), ("ankle_L", "knee_L"),
    ("hip_R", "hips"), ("knee_R", "hip_R"), ("ankle_R", "knee_R"),
]
# Where each joint's bone ends, for weighting: another joint or a tip point.
HUMANOID_TAILS = {
    "hips": "abdomen", "abdomen": "chest", "chest": "neck", "neck": "head",
    "head": "head_top",
    "shoulder_L": "elbow_L", "elbow_L": "wrist_L", "wrist_L": "hand_L",
    "shoulder_R": "elbow_R", "elbow_R": "wrist_R", "wrist_R": "hand_R",
    "hip_L": "knee_L", "knee_L": "ankle_L", "ankle_L": "toe_L",
    "hip_R": "knee_R", "knee_R": "ankle_R", "ankle_R": "toe_R",
}
# Bones that point straight down in the forge rest pose.
HUMANOID_DOWN = {
    "shoulder_L": "elbow_L", "elbow_L": "wrist_L",
    "shoulder_R": "elbow_R", "elbow_R": "wrist_R",
    "hip_L": "knee_L", "knee_L": "ankle_L",
    "hip_R": "knee_R", "knee_R": "ankle_R",
}
# Joints that keep their orientation while their parents straighten (feet
# stay flat on the floor).
HUMANOID_LEVEL = ("ankle_L", "ankle_R")

MAX_TEXTURE = 1024      # px; sprites are ~100 px tall, more only aliases
VOXEL_DIVISIONS = 110   # proxy voxels per model height
WEIGHTS_PER_VERTEX = 4


# --------------------------------------------------------------------------
# Loading
# --------------------------------------------------------------------------

def _activate(obj):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def _coords(mesh):
    co = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", co)
    return co.reshape(-1, 3).astype(np.float64)


def _set_coords(mesh, co):
    mesh.vertices.foreach_set("co", np.ascontiguousarray(co, dtype=np.float32).ravel())
    mesh.update()


def load_mesh(path, height, yaw_deg=0.0):
    """Import a model file as one mesh object, normalised: feet on z = 0,
    `height` tall, centred, facing -Y. Returns (object, vertices Nx3)."""
    assert os.path.exists(path), f"model file not found: {path}"
    before = set(bpy.data.objects)
    ext = os.path.splitext(path)[1].lower()
    if ext in (".glb", ".gltf"):
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == ".obj":
        bpy.ops.wm.obj_import(filepath=path)
    else:
        raise AssertionError(f"unsupported model format: {ext}")
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH" and len(o.data.vertices) > 0]
    assert meshes, f"{path}: no mesh in the file"
    bpy.context.view_layer.update()

    for o in meshes:                       # bake transforms, drop any rig
        world = o.matrix_world.copy()
        o.parent = None
        for mod in list(o.modifiers):
            o.modifiers.remove(mod)
        o.matrix_world = world
    bpy.context.view_layer.update()
    obj = meshes[0]
    if len(meshes) > 1:
        for o in bpy.context.view_layer.objects:
            o.select_set(False)
        for o in meshes:
            o.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.join()
    obj.data.transform(obj.matrix_world)
    obj.matrix_world.identity()
    obj.vertex_groups.clear()
    for o in new:                          # armatures, empties, cameras
        if o is not obj and o.name in bpy.data.objects and o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)

    mesh = obj.data
    # Imported custom normals would go stale as the mesh deforms.
    attr = mesh.attributes.get("custom_normal")
    if attr is not None:
        mesh.attributes.remove(attr)
    for poly in mesh.polygons:
        poly.use_smooth = True

    v = _coords(mesh)
    if yaw_deg:
        a = math.radians(yaw_deg)
        c, s = math.cos(a), math.sin(a)
        v = np.stack([v[:, 0] * c - v[:, 1] * s, v[:, 0] * s + v[:, 1] * c, v[:, 2]], 1)
    v[:, 2] -= v[:, 2].min()
    tall = v[:, 2].max()
    assert tall > 1e-6, f"{path}: the mesh is flat"
    v *= height / tall
    torso = (v[:, 2] > 0.45 * height) & (v[:, 2] < 0.75 * height)
    core = v[torso] if torso.sum() > 10 else v
    v[:, 0] -= (core[:, 0].min() + core[:, 0].max()) / 2
    v[:, 1] -= np.median(core[:, 1])
    _set_coords(mesh, v)

    for img in {n.image for m in mesh.materials if m and m.node_tree
                for n in m.node_tree.nodes if n.type == "TEX_IMAGE" and n.image}:
        w, h = img.size
        if max(w, h) > MAX_TEXTURE:
            k = MAX_TEXTURE / max(w, h)
            img.scale(max(1, round(w * k)), max(1, round(h * k)))
    return obj, v


# --------------------------------------------------------------------------
# Fitting the skeleton
# --------------------------------------------------------------------------

def auto_fit(v, height):
    """A first guess at joint positions for a humanoid standing in an A- or
    T-pose. It is a seed: look at it with fit.py and correct it by hand."""
    H = height

    def band(z, half=0.012):
        return v[np.abs(v[:, 2] - z) < half * H]

    def centre_y(z, max_x=0.10):
        b = band(z, 0.02)
        b = b[np.abs(b[:, 0]) < max_x * H]
        return float(np.median(b[:, 1])) if len(b) else 0.0

    # Neck: the narrowest slice between the shoulders and the skull.
    best, neck_z = 1e9, 0.86 * H
    for z in np.linspace(0.80 * H, 0.92 * H, 25):
        b = band(z)
        b = b[np.abs(b[:, 0]) < 0.12 * H]
        if len(b) > 4:
            width = b[:, 0].max() - b[:, 0].min()
            if width < best:
                best, neck_z = width, float(z)
    # Crotch: the highest slice with a gap in the middle.
    crotch_z = 0.47 * H
    for z in np.linspace(0.56 * H, 0.30 * H, 53):
        b = band(z, 0.006)
        if len(b) > 4 and not (np.abs(b[:, 0]) < 0.012 * H).any():
            crotch_z = float(z)
            break
    hips_z = crotch_z + 0.06 * H
    sh_z = neck_z - 0.035 * H

    fit = {
        "hips": (0, centre_y(hips_z), hips_z),
        "abdomen": (0, centre_y(hips_z + 0.08 * H), hips_z + 0.08 * H),
        "chest": (0, centre_y(hips_z + 0.17 * H), hips_z + 0.17 * H),
        "neck": (0, centre_y(neck_z, 0.05), neck_z),
        "head": (0, centre_y(neck_z + 0.04 * H, 0.05), neck_z + 0.04 * H),
        "head_top": (0, centre_y(0.95 * H, 0.08), H),
    }
    for side, sx in (("L", 1.0), ("R", -1.0)):
        half = v[v[:, 0] * sx > 0]
        tip = half[np.argmax(half[:, 0] * sx)]                 # fingertips
        shoulder = np.array((sx * 0.105 * H, centre_y(sh_z), sh_z))
        wrist = tip + (shoulder - tip) * (0.095 * H / max(1e-6, np.linalg.norm(shoulder - tip)))
        elbow = shoulder + (wrist - shoulder) * 0.52
        fit[f"shoulder_{side}"] = tuple(shoulder)
        fit[f"elbow_{side}"] = tuple(elbow)
        fit[f"wrist_{side}"] = tuple(wrist)
        fit[f"hand_{side}"] = tuple(tip)

        def leg_centre(z, fallback_x):
            b = half[np.abs(half[:, 2] - z) < 0.02 * H]
            b = b[np.abs(b[:, 0]) < 0.22 * H]                  # not the hands
            if len(b) < 4:
                return np.array((fallback_x, 0.0, z))
            return np.array(((b[:, 0].min() + b[:, 0].max()) / 2,
                             (b[:, 1].min() + b[:, 1].max()) / 2, z))
        knee = leg_centre(0.29 * H, sx * 0.06 * H)
        ankle = leg_centre(0.055 * H, sx * 0.07 * H)
        foot = half[half[:, 2] < 0.04 * H]
        toe_y = float(foot[:, 1].min()) if len(foot) else ankle[1] - 0.1 * H
        ankle[1] = max(ankle[1], toe_y + 0.07 * H) if len(foot) else ankle[1]
        fit[f"hip_{side}"] = (sx * 0.055 * H, fit["hips"][1], crotch_z + 0.045 * H)
        fit[f"knee_{side}"] = tuple(knee)
        fit[f"ankle_{side}"] = tuple(ankle)
        fit[f"toe_{side}"] = (ankle[0], toe_y, 0.0)
    return {k: tuple(round(float(c), 3) for c in p) for k, p in fit.items()}


def complete_fit(fit, guess):
    """Fill in what the actor did not specify: the right side mirrors the
    left, and anything still missing comes from the automatic guess."""
    out = dict(fit or {})
    for name, p in list(out.items()):
        if name.endswith("_L") and name[:-2] + "_R" not in out:
            out[name[:-2] + "_R"] = (-p[0], p[1], p[2])
    for name, p in guess.items():
        out.setdefault(name, p)
    return {k: tuple(float(c) for c in p) for k, p in out.items()}


# --------------------------------------------------------------------------
# Weights
# --------------------------------------------------------------------------

def _segment_distance(v, a, b):
    ab = b - a
    t = np.clip(((v - a) @ ab) / max(1e-9, ab @ ab), 0, 1)
    return np.linalg.norm(v - (a + t[:, None] * ab), axis=1)


def _distance_weights(v, segments):
    d = np.stack([_segment_distance(v, a, b) for a, b in segments], 1)
    w = 1.0 / (d ** 4 + 1e-9)
    return w / w.sum(1, keepdims=True)


def _bone_heat(obj, skeleton, segments, height):
    """Blender's automatic weights, computed on a watertight voxel copy of
    the mesh and carried back to the real vertices. Returns NxJ or None."""
    names = [n for n, _ in skeleton]
    proxy = bpy.data.objects.new("forge_proxy", obj.data.copy())
    bpy.context.scene.collection.objects.link(proxy)
    arm = None
    try:
        _activate(proxy)
        mod = proxy.modifiers.new("remesh", "REMESH")
        mod.mode = "VOXEL"
        mod.voxel_size = height / VOXEL_DIVISIONS
        bpy.ops.object.modifier_apply(modifier=mod.name)
        if len(proxy.data.vertices) < 50:
            return None

        arm = bpy.data.objects.new("forge_bind", bpy.data.armatures.new("forge_bind"))
        bpy.context.scene.collection.objects.link(arm)
        _activate(arm)
        bpy.ops.object.mode_set(mode="EDIT")
        for (name, parent), (a, b) in zip(skeleton, segments):
            bone = arm.data.edit_bones.new(name)
            bone.head = Vector(a)
            bone.tail = Vector(b)
            if parent is not None:
                bone.parent = arm.data.edit_bones[parent]
        bpy.ops.object.mode_set(mode="OBJECT")

        for o in bpy.context.view_layer.objects:
            o.select_set(False)
        proxy.select_set(True)
        arm.select_set(True)
        bpy.context.view_layer.objects.active = arm
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")

        index = {g.index: names.index(g.name) for g in proxy.vertex_groups
                 if g.name in names}
        pw = np.zeros((len(proxy.data.vertices), len(names)))
        for vert in proxy.data.vertices:
            for g in vert.groups:
                if g.group in index:
                    pw[vert.index, index[g.group]] = g.weight
        good = pw.sum(1) > 1e-6
        if good.mean() < 0.9:
            return None
        pv = _coords(proxy.data)[good]
        pw = pw[good]
        tree = KDTree(len(pv))
        for i, p in enumerate(pv):
            tree.insert(p, i)
        tree.balance()
        v = _coords(obj.data)
        out = np.zeros((len(v), len(names)))
        for i, p in enumerate(v):
            for _, j, dist in tree.find_n(p, 3):
                out[i] += pw[j] / (dist + 1e-4)
        return out / out.sum(1, keepdims=True)
    except RuntimeError as err:
        print(f"  skin: bone heat failed ({err})")
        return None
    finally:
        if bpy.context.object and bpy.context.object.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        for o in (proxy, arm):
            if o is not None:
                bpy.data.objects.remove(o, do_unlink=True)


def bind_weights(obj, v, skeleton, tails, fit, height, cache_path=None, log=print):
    """NxJ skin weights for the mesh in its imported pose."""
    names = [n for n, _ in skeleton]
    segments = []
    for name, parent in skeleton:
        a = np.array(fit[name], dtype=np.float64)
        tail = tails.get(name)
        if tail in fit:
            b = np.array(fit[tail], dtype=np.float64)
        elif parent is not None:       # no tip given: continue the parent bone
            b = a + (a - np.array(fit[parent])) * 0.5
        else:
            b = a + np.array((0, 0, 0.1 * height))
        if np.linalg.norm(b - a) < 1e-3:
            b = a + np.array((0, 0, 0.02 * height))
        segments.append((a, b))

    key = hashlib.sha1()
    key.update(np.round(v, 4).astype(np.float32).tobytes())
    key.update(repr([(n, np.round(a, 4).tolist(), np.round(b, 4).tolist())
                     for n, (a, b) in zip(names, segments)]).encode())
    key.update(f"{VOXEL_DIVISIONS}".encode())
    digest = key.hexdigest()
    if cache_path and os.path.exists(cache_path):
        data = np.load(cache_path, allow_pickle=False)
        if str(data["key"]) == digest:
            return data["weights"].astype(np.float64)

    w = _bone_heat(obj, skeleton, segments, height)
    if w is None:
        log("  skin: automatic weights unavailable; using distance weights "
            "(check the weights panel in fit.py)")
        w = _distance_weights(v, segments)
    # Keep the strongest few influences per vertex.
    if w.shape[1] > WEIGHTS_PER_VERTEX:
        cut = np.sort(w, axis=1)[:, -WEIGHTS_PER_VERTEX][:, None]
        w = np.where(w >= cut, w, 0.0)
    w = w / w.sum(1, keepdims=True)
    if cache_path:
        np.savez_compressed(cache_path, key=digest, weights=w.astype(np.float32))
    return w


# --------------------------------------------------------------------------
# Rest pose
# --------------------------------------------------------------------------

def _rotation_between(a, b):
    return np.array(Vector(a).rotation_difference(Vector(b)).to_matrix())


def straighten(skeleton, fit, down, level):
    """Transforms that take the imported pose to the forge rest pose: every
    bone in `down` points straight down, joints in `level` keep their
    orientation. Returns ({joint: 4x4}, {joint: new position})."""
    rot, pos, xf = {}, {}, {}
    for name, parent in skeleton:
        p = np.array(fit[name], dtype=np.float64)
        if parent is None:
            r_parent, new = np.eye(3), p.copy()
        else:
            r_parent = rot[parent]
            new = pos[parent] + r_parent @ (p - np.array(fit[parent]))
        r = r_parent
        if name in down:
            d = r_parent @ (np.array(fit[down[name]]) - p)
            n = np.linalg.norm(d)
            if n > 1e-6:
                r = _rotation_between(d / n, (0, 0, -1)) @ r_parent
        if name in level:
            r = np.eye(3)
        rot[name], pos[name] = r, new
        m = np.eye(4)
        m[:3, :3] = r
        m[:3, 3] = new - r @ p
        xf[name] = m
    return xf, pos


def _apply(v, idx, w, mats):
    """Linear blend skinning. idx, w: NxK; mats: Jx4x4."""
    vh = np.concatenate([v, np.ones((len(v), 1))], 1)
    out = np.zeros((len(v), 3))
    for k in range(idx.shape[1]):
        m = mats[idx[:, k]]
        out += w[:, k, None] * np.einsum("nab,nb->na", m[:, :3, :], vh)
    return out


# --------------------------------------------------------------------------
# The skin on a Model
# --------------------------------------------------------------------------

def _texture_emission(source, name):
    """A flat, unlit copy of a material: its base colour texture (or vertex
    colours, or plain colour) as emission."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(em.outputs[0], out.inputs[0])
    image, colour = None, (0.5, 0.5, 0.5, 1.0)
    if source is not None and source.node_tree:
        nodes = source.node_tree.nodes
        bsdf = next((n for n in nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is not None:
            base = bsdf.inputs["Base Color"]
            colour = tuple(base.default_value)
            seen, stack = set(), [l.from_node for l in base.links]
            while stack and image is None:
                n = stack.pop()
                if n in seen:
                    continue
                seen.add(n)
                if n.type == "TEX_IMAGE" and n.image:
                    image = n.image
                stack.extend(l.from_node for i in n.inputs for l in i.links)
        if image is None:
            image = next((n.image for n in nodes if n.type == "TEX_IMAGE" and n.image), None)
    if image is not None:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = image
        tex.interpolation = "Linear"
        nt.links.new(tex.outputs["Color"], em.inputs["Color"])
        return mat, "texture"
    em.inputs["Color"].default_value = colour
    return mat, "colour"


class Skin:
    """A textured mesh bound to a Model's joints."""

    def __init__(self, obj, rest, joints, rest_pos, weights, id_material):
        self.obj = obj
        self.rest = rest                                  # Nx3, forge rest pose
        self.joints = joints                              # joint names
        self.rest_pos = np.array([rest_pos[j] for j in joints])
        order = np.argsort(-weights, axis=1)[:, :WEIGHTS_PER_VERTEX]
        self.idx = order
        self.w = np.take_along_axis(weights, order, axis=1)
        self.w = self.w / self.w.sum(1, keepdims=True)
        mesh = obj.data
        if not mesh.materials:
            mesh.materials.append(None)
        self.albedo_mats, kinds = [], []
        for i, src in enumerate(mesh.materials):
            mat, kind = _texture_emission(src, f"{obj.name}_albedo_{i}")
            # Vertex colours, when the file has them and no texture.
            if kind == "colour" and mesh.color_attributes:
                nt = mat.node_tree
                em = next(n for n in nt.nodes if n.type == "EMISSION")
                attr = nt.nodes.new("ShaderNodeAttribute")
                attr.attribute_name = mesh.color_attributes[0].name
                nt.links.new(attr.outputs["Color"], em.inputs["Color"])
                kind = "vertex colours"
            self.albedo_mats.append(mat)
            kinds.append(kind)
        self.colour_source = ", ".join(sorted(set(kinds)))
        self.id_mats = [id_material] * len(self.albedo_mats)
        self.show_albedo(False)

    def show_albedo(self, on):
        mats = self.albedo_mats if on else self.id_mats
        for i, mat in enumerate(mats):
            self.obj.data.materials[i] = mat

    def deform(self, model):
        mats = np.empty((len(self.joints), 4, 4))
        for k, name in enumerate(self.joints):
            m = np.array(model.joints[name].matrix_world)
            m[:3, 3] -= m[:3, :3] @ self.rest_pos[k]
            mats[k] = m
        _set_coords(self.obj.data, _apply(self.rest, self.idx, self.w, mats))


def prepare(path, height, fit=None, yaw=0.0, skeleton=HUMANOID,
            tails=HUMANOID_TAILS, down=HUMANOID_DOWN, level=HUMANOID_LEVEL,
            cache=True, log=print):
    """Everything attach() does short of touching a Model. Returns a dict:
    obj, imported (Nx3), fit, weights, rest (Nx3), rest_pos."""
    obj, v = load_mesh(path, height, yaw)
    guess = auto_fit(v, height) if skeleton is HUMANOID else {}
    full = complete_fit(fit, guess)
    missing = [n for n, _ in skeleton if n not in full]
    assert not missing, f"{path}: fit has no position for {missing}"
    cache_path = path + ".weights.npz" if cache else None
    w = bind_weights(obj, v, skeleton, tails, full, height, cache_path, log)
    xf, rest_pos = straighten(skeleton, full, down or {}, level or ())
    names = [n for n, _ in skeleton]
    order = np.argsort(-w, axis=1)[:, :WEIGHTS_PER_VERTEX]
    ww = np.take_along_axis(w, order, axis=1)
    rest = _apply(v, order, ww / ww.sum(1, keepdims=True),
                  np.array([xf[n] for n in names]))
    lift = -rest[:, 2].min()                 # straight legs reach lower
    rest[:, 2] += lift
    rest_pos = {n: p + np.array((0, 0, lift)) for n, p in rest_pos.items()}
    return {"obj": obj, "imported": v, "fit": full, "weights": w,
            "rest": rest, "rest_pos": rest_pos, "names": names}


def attach(model, path, height, fit=None, material="mesh", **options):
    """Put a model file on `model` as its body. Creates the skeleton's joints
    (do not also call _body.skeleton) and returns the Skin. Add props and
    costume parts to the joints afterwards, as with any forge model."""
    skeleton = options.get("skeleton", HUMANOID)
    data = prepare(path, height, fit, **options)
    for name, parent in skeleton:
        model.joint(name, parent, tuple(float(c) for c in data["rest_pos"][name]))
    model.material(material, (128, 128, 128))
    model.materials[material].textured = True
    s = Skin(data["obj"], data["rest"], data["names"], data["rest_pos"],
             data["weights"], model.materials[material].bmat)
    s.obj.name = f"{model.name}_skin"
    _set_coords(s.obj.data, s.rest)
    model.skins.append(s)
    return s
