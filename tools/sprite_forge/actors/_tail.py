"""A tail (or any long, bendy appendage) from its own model file, on a chain
of joints.

Generated tails come curled (Namora's is an upright S). This finds the
tail's centreline, straightens the mesh along it, and binds the straight
tail to a chain of joints from base to tip, so poses curl it however the
moment wants: trailing on the floor, swaying in the run, whipping round.

    import _tail
    _tail.attach(m, "art/models/NamoraTail.glb", parent="hips",
                 base=(0, 0.12, 0.95), direction=(0, 1, -0.6),
                 length=1.4, joints=8)
    # joints are tail_0 (base) .. tail_7 (tip)

The base end is the thicker one. Look at debug() before trusting a new
model: it draws the centreline it found over the model.
"""

import heapq
import os

import bpy
import numpy as np

import skin

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))


def _graph(obj, v):
    """Vertex adjacency with seams welded: positions that coincide are one
    node (glTF splits vertices at every UV seam)."""
    key = np.round(v, 5)
    _, node = np.unique(key, axis=0, return_inverse=True)
    node = node.reshape(-1)
    n = node.max() + 1
    pos = np.zeros((n, 3))
    pos[node] = v
    adj = [[] for _ in range(n)]
    for e in obj.data.edges:
        a, b = node[e.vertices[0]], node[e.vertices[1]]
        if a != b:
            d = float(np.linalg.norm(pos[a] - pos[b]))
            adj[a].append((b, d))
            adj[b].append((a, d))
    return node, pos, adj


def _dijkstra(adj, src):
    dist = np.full(len(adj), np.inf)
    prev = np.full(len(adj), -1)
    dist[src] = 0.0
    q = [(0.0, src)]
    while q:
        d, a = heapq.heappop(q)
        if d > dist[a]:
            continue
        for b, w in adj[a]:
            if d + w < dist[b]:
                dist[b] = d + w
                prev[b] = a
                heapq.heappush(q, (d + w, b))
    return dist, prev


def centreline(obj, v, samples=64):
    """(points, per-vertex arc parameter s in 0..1, ends) for the tail.

    The two ends are the farthest pair along the surface. The path between
    them hugs one side of the tail, so each sample is replaced by the
    centroid of the vertices nearest it: the middle of that cross-section.
    Returned base first: the end whose cross-section is thicker."""
    node, pos, adj = _graph(obj, v)
    big = max(range(len(adj)), key=lambda i: len(adj[i]))
    d0, _ = _dijkstra(adj, big)
    a = int(np.argmax(np.where(np.isfinite(d0), d0, -1)))
    da, prev = _dijkstra(adj, a)
    b = int(np.argmax(np.where(np.isfinite(da), da, -1)))
    path = [b]
    while path[-1] != a:
        path.append(int(prev[path[-1]]))
    p = pos[path]
    seg = np.linalg.norm(np.diff(p, axis=0), axis=1)
    arc = np.concatenate([[0.0], np.cumsum(seg)]) / seg.sum()
    pts = np.array([[np.interp(t, arc, p[:, k]) for k in range(3)]
                    for t in np.linspace(0, 1, samples)])
    # Nearest sample for every vertex, then centroids per sample.
    near = np.argmin(((v[:, None, :] - pts[None, :, :]) ** 2).sum(2), axis=1)
    cen = np.array([v[near == i].mean(0) if (near == i).any() else pts[i]
                    for i in range(samples)])
    radius = np.array([np.linalg.norm(v[near == i] - cen[i], axis=1).mean()
                       if (near == i).any() else 0.0 for i in range(samples)])
    # Smooth the centreline a little.
    k = np.array([0.25, 0.5, 0.25])
    for _ in range(3):
        cen[1:-1] = k[0] * cen[:-2] + k[1] * cen[1:-1] + k[2] * cen[2:]
    if radius[:samples // 4].mean() < radius[-samples // 4:].mean():
        cen, radius = cen[::-1].copy(), radius[::-1].copy()
    return cen, radius


def straighten(v, cen):
    """Unbend the mesh: each vertex keeps its offset from the nearest point
    of the centreline, measured in that point's frame, and is laid out
    along a straight line instead. Returns (straight vertices along +x
    from the base at the origin, arc length of each vertex, total length)."""
    seg = np.linalg.norm(np.diff(cen, axis=0), axis=1)
    arc = np.concatenate([[0.0], np.cumsum(seg)])
    tang = np.gradient(cen, axis=0)
    tang /= np.linalg.norm(tang, axis=1, keepdims=True)
    # The tail is drawn in one plane: its normal is the smallest spread.
    c = cen - cen.mean(0)
    plane_n = np.linalg.svd(c, full_matrices=False)[2][2]
    side = np.cross(plane_n, tang)
    side /= np.linalg.norm(side, axis=1, keepdims=True)
    near = np.argmin(((v[:, None, :] - cen[None, :, :]) ** 2).sum(2), axis=1)
    d = v - cen[near]
    along = arc[near] + (d * tang[near]).sum(1)
    out = np.stack([along, (d * plane_n).sum(1), (d * side[near]).sum(1)], 1)
    return out, along, arc[-1]


def attach(model, path, parent, base, direction, length, joints=8,
           material="tail"):
    """Straighten the tail in `path`, scale it to `length`, lay it from
    `base` (world, rest) along `direction`, and bind it to joints
    tail_0..tail_{joints-1} under `parent`. Returns the joint names."""
    obj, v = skin.load_mesh(os.path.join(REPO, path), 1.0)
    cen, _ = centreline(obj, v)
    flat, along, total = straighten(v, cen)
    k = length / total
    flat *= k
    along = np.clip(along * k, 0, length)
    axis = np.array(direction, dtype=float)
    axis /= np.linalg.norm(axis)
    up = np.array((0.0, 0.0, 1.0))
    if abs(axis @ up) > 0.95:
        up = np.array((1.0, 0.0, 0.0))
    side = np.cross(up, axis)
    side /= np.linalg.norm(side)
    normal = np.cross(axis, side)
    base = np.array(base, dtype=float)
    world = base + flat[:, :1] * axis + flat[:, 1:2] * side + flat[:, 2:3] * normal

    names = [f"tail_{i}" for i in range(joints)]
    step = length / joints
    for i, name in enumerate(names):
        model.joint(name, parent if i == 0 else names[i - 1],
                    tuple(base + axis * step * i))
    # Each vertex blends between the two joints whose segments it spans.
    u = along / step
    lo = np.clip(np.floor(u - 0.5).astype(int), 0, joints - 1)
    hi = np.clip(lo + 1, 0, joints - 1)
    t = np.clip(u - 0.5 - lo, 0, 1)
    w = np.zeros((len(v), joints))
    w[np.arange(len(v)), lo] += 1 - t
    w[np.arange(len(v)), hi] += t
    model.material(material, (200, 200, 200))
    model.materials[material].textured = True
    rest = {n: np.array(model.rest_world(n)) for n in names}
    s = skin.Skin(obj, world, names, rest, w, model.materials[material].bmat)
    s.obj.name = f"{model.name}_{material}"
    skin._set_coords(s.obj.data, world)
    model.skins.append(s)
    return names


def debug(path, out):
    """Draw the model's centreline over it (front, x-z) for a human to check."""
    from PIL import Image, ImageDraw
    obj, v = skin.load_mesh(os.path.join(REPO, path), 1.0)
    cen, radius = centreline(obj, v)
    flat, _, total = straighten(v, cen)
    W, H, pad = 900, 900, 40
    lo, hi = v.min(0), v.max(0)
    sc = (W - 2 * pad) / max(hi[0] - lo[0], hi[2] - lo[2])
    img = Image.new("RGB", (W, H + 300), (40, 44, 56))
    dr = ImageDraw.Draw(img)

    def p(x, z):
        return (pad + (x - lo[0]) * sc, H - pad - (z - lo[2]) * sc)
    for x, _, z in v[::3]:
        dr.point(p(x, z), fill=(120, 170, 120))
    for i in range(len(cen) - 1):
        dr.line([p(cen[i][0], cen[i][2]), p(cen[i + 1][0], cen[i + 1][2])],
                fill=(255, 80, 80), width=3)
    bx, bz = p(cen[0][0], cen[0][2])
    dr.ellipse([bx - 8, bz - 8, bx + 8, bz + 8], outline=(255, 255, 0), width=3)
    dr.text((bx + 10, bz), "base", fill=(255, 255, 0))
    # The straightened tail underneath.
    s2 = (W - 2 * pad) / total
    for x, _, z in flat[::3]:
        dr.point((pad + x * s2, H + 150 - z * s2), fill=(170, 170, 120))
    dr.text((pad, H + 20), "straightened (base left)", fill=(255, 255, 255))
    img.save(out)
