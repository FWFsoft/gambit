#!/usr/bin/env python3
"""Animated GIF of every clip of baked actors, for reviewing animation.

  python3 tools/sprite_forge/showcase.py out.gif Wade:1 Toxlith:7
(name:direction, direction 0-7 = S SE E NE N NW W SW)
"""
import sys

from PIL import Image, ImageDraw

SCALE = 2
GIF_FPS = 20


def load(manifest="assets/sprites/actors/manifest.txt"):
    actors = {}
    for line in open(manifest):
        p = line.split()
        if p[0] == "actor":
            actors[p[1]] = {"img": Image.open(p[2]).convert("RGBA"),
                            "w": int(p[3]), "h": int(p[4]), "cols": int(p[7]),
                            "anims": []}
        elif p[0] == "anim":
            actors[p[1]]["anims"].append(
                (p[2], int(p[3]), int(p[4]), int(p[5]), p[6] == "1"))
    return actors


def main():
    out = sys.argv[1]
    actors = load()
    tiles = []
    for arg in sys.argv[2:]:
        name, d = arg.split(":")
        a = actors[name]
        for clip, start, frames, fps, loop in a["anims"]:
            tiles.append((name, int(d), a, clip, start, frames, fps, loop))
    cols = 5
    tw = max(t[2]["w"] for t in tiles) * SCALE
    th = max(t[2]["h"] for t in tiles) * SCALE + 14
    rows = (len(tiles) + cols - 1) // cols
    seconds = 2.4
    frames_out = []
    for i in range(int(seconds * GIF_FPS)):
        t = i / GIF_FPS
        canvas = Image.new("RGBA", (cols * tw, rows * th), (74, 88, 62, 255))
        draw = ImageDraw.Draw(canvas)
        for k, (name, d, a, clip, start, frames, fps, loop) in enumerate(tiles):
            n = int(t * fps)
            if loop:
                f = n % frames
            else:
                cycle = frames + int(fps * 0.5)  # hold the end, then replay
                f = min(frames - 1, n % cycle)
            cell = start + d * frames + f
            x0 = (cell % a["cols"]) * a["w"]
            y0 = (cell // a["cols"]) * a["h"]
            sprite = a["img"].crop((x0, y0, x0 + a["w"], y0 + a["h"]))
            sprite = sprite.resize((a["w"] * SCALE, a["h"] * SCALE),
                                   Image.NEAREST)
            ox = (k % cols) * tw + (tw - sprite.width) // 2
            oy = (k // cols) * th + 14 + (th - 14 - sprite.height)
            canvas.alpha_composite(sprite, (ox, oy))
            draw.text(((k % cols) * tw + 4, (k // cols) * th + 2),
                      f"{name} {clip}", fill=(255, 240, 160))
        frames_out.append(canvas.convert("RGB"))
    frames_out[0].save(out, save_all=True, append_images=frames_out[1:],
                       duration=int(1000 / GIF_FPS), loop=0, optimize=True)
    print(out, frames_out[0].size, len(frames_out))


if __name__ == "__main__":
    main()
