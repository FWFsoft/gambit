#!/usr/bin/env python3
"""Cut a heavy generated model down to what the forge wants, for free.

    <forge python> tools/sprite_forge/shrink.py big.glb art/models/Wade.glb
    <forge python> tools/sprite_forge/shrink.py big.glb out.glb --triangles 40000 --texture 2048

Image-to-3D tools hand back meshes of half a million triangles with 4K PBR
textures. The forge renders small sprites and adds its own light, so it
wants 20,000 to 60,000 triangles and one base colour texture
(art/models/README.md). This does that locally in Blender: merge the
seams, decimate, keep only the base colour and scale it down.

It costs nothing, unlike `meshy.py remesh`. Look at the result with fit.py:
decimating keeps the texture where it was, but very thin parts (fingers,
straps) can lose shape.
"""

import argparse
import os
import sys

import bpy


def shrink(src, dst, triangles, texture):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=src)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    assert meshes, f"{src}: no mesh in the file"
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    for other in list(bpy.context.scene.objects):
        if other is not obj and other.type != "MESH":
            bpy.data.objects.remove(other)

    # glTF splits vertices at every UV seam; weld them so decimate sees one
    # surface (UVs live on face corners in Blender, so they survive).
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.remove_doubles(threshold=1e-6)
    bpy.ops.mesh.quads_convert_to_tris()
    bpy.ops.object.mode_set(mode="OBJECT")

    before = len(obj.data.polygons)
    if before > triangles:
        mod = obj.modifiers.new("shrink", "DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = triangles / before
        mod.delimit = {"UV"}                  # do not smear across UV islands
        bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.ops.object.shade_smooth()
    after = len(obj.data.polygons)

    # Base colour only, at `texture` pixels.
    for mat in obj.data.materials:
        if mat is None or not mat.use_nodes:
            continue
        tree = mat.node_tree
        bsdf = next((n for n in tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
        if bsdf is None:
            continue
        for socket in bsdf.inputs:
            if socket.name != "Base Color":
                for link in list(socket.links):
                    tree.links.remove(link)
        bsdf.inputs["Metallic"].default_value = 0.0
        bsdf.inputs["Roughness"].default_value = 1.0
        for link in bsdf.inputs["Base Color"].links:
            image = getattr(link.from_node, "image", None)
            if image and max(image.size) > texture:
                scale = texture / max(image.size)
                image.scale(max(1, round(image.size[0] * scale)),
                            max(1, round(image.size[1] * scale)))
    bpy.ops.outliner.orphans_purge(do_recursive=True)

    os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=dst, export_format="GLB", export_image_format="JPEG",
        export_jpeg_quality=92, export_animations=False, export_skins=False,
        export_tangents=False, export_yup=True)
    size = os.path.getsize(dst)
    print(f"{before:,} -> {after:,} triangles, {size / 1e6:.1f} MB: {dst}")
    assert size <= 15 * 1024 * 1024, "still over 15 MB: lower --texture"


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("src")
    p.add_argument("dst")
    p.add_argument("--triangles", type=int, default=40_000)
    p.add_argument("--texture", type=int, default=2048, help="longest side, px")
    p.add_argument("--force", action="store_true", help="replace dst")
    a = p.parse_args()
    assert os.path.isfile(a.src), f"no such file: {a.src}"
    if os.path.exists(a.dst) and not a.force:
        sys.exit(f"{a.dst} exists. Pass --force to replace it.")
    shrink(a.src, a.dst, a.triangles, a.texture)


if __name__ == "__main__":
    main()
