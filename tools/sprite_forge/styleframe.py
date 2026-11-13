"""Style frame: a mock-up of the target in-game look, composited in Python.

    python tools/sprite_forge/styleframe.py . docs/prototype/styleframe_target.png

Not the engine. It exists to show, with the game's real sprites, what the
presentation layer is worth: a dark, quiet floor, pools of light, cast
shadows, a rim light, ink blacks and glowing hits (docs/design/
ART_DIRECTION.md, "The look"). The numbers in here (floor values, light
colours, glow blurs) are the starting point for building that pass in
src/. Needs only Pillow and numpy.
"""
import sys, math, random
import numpy as np
from PIL import Image, ImageFilter, ImageDraw, ImageFont

REPO, OUT = sys.argv[1], sys.argv[2]
W, H = 640, 400
random.seed(7)
rng = np.random.default_rng(7)


def font(sz):
    try:
        return ImageFont.truetype("arialbd.ttf", sz)
    except Exception:
        return ImageFont.load_default()


# ---------------------------------------------------------------- sources
def forge_cell(name, cw, ch, cols, index):
    sheet = Image.open(f"{REPO}/assets/sprites/actors/{name}.png").convert("RGBA")
    x, y = (index % cols) * cw, (index // cols) * ch
    return sheet.crop((x, y, x + cw, y + ch))


def trim(im):
    return im.crop(im.getbbox())


def turnaround_views(path, n=8):
    im = Image.open(path).convert("RGBA")
    w = im.width / n
    return [trim(im.crop((round(i * w), 0, round((i + 1) * w), im.height))) for i in range(n)]


def scale_to_h(im, h, smooth=True):
    w = max(1, round(im.width * h / im.height))
    return im.resize((w, h), Image.LANCZOS if smooth else Image.NEAREST)


# ---------------------------------------------------------------- ground
def iso_ground():
    """Dark, cool, low-contrast floor: value lives in the characters."""
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    tw, th = 96.0, 48.0
    u = xx / tw + yy / th
    v = yy / th - xx / tw
    iu, iv = np.floor(u), np.floor(v)
    fu, fv = u - iu, v - iv
    h = (np.sin(iu * 127.1 + iv * 311.7) * 43758.5453) % 1.0
    h2 = (np.sin(iu * 269.5 + iv * 183.3) * 12345.678) % 1.0
    base = 0.085 + 0.035 * h
    # slab bevel: lighter top-left edge, darker seam
    edge = np.minimum(np.minimum(fu, 1 - fu), np.minimum(fv, 1 - fv))
    seam = np.clip(1 - edge / 0.035, 0, 1)
    bevel = np.clip(1 - np.minimum(fu, fv) / 0.09, 0, 1) * (1 - seam)
    val = base * (1 - 0.75 * seam) + 0.03 * bevel
    # broad grime
    n = rng.random((H // 40 + 2, W // 40 + 2)).astype(np.float32)
    n = np.array(Image.fromarray((n * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)) / 255.0
    val *= 0.8 + 0.4 * n
    rgb = np.stack([val * 0.82, val * 0.92, val * 1.25], -1)  # cool slate
    # toxic seams: some slabs leak Venoxarid green along their cracks
    leak = (h2 > 0.82).astype(np.float32) * seam
    leakglow = np.array(Image.fromarray((leak * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(5))) / 255.0
    rgb += leak[..., None] * np.array([0.25, 0.9, 0.35]) * 0.8
    rgb += leakglow[..., None] * np.array([0.1, 0.55, 0.2]) * 0.9
    return rgb


def light_pool(rgb, cx, cy, radius, color, strength):
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    d = np.sqrt((xx - cx) ** 2 + ((yy - cy) * 2.0) ** 2) / radius
    f = np.clip(1 - d, 0, 1) ** 2
    return rgb + rgb * f[..., None] * np.array(color) * strength + f[..., None] * np.array(color) * 0.03 * strength


def vignette(rgb, amount=0.55):
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    d = np.sqrt(((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2)
    return rgb * (1 - amount * np.clip(d - 0.35, 0, 1) ** 1.5)[..., None]


# ---------------------------------------------------------------- sprite treatment
def ink_pass(im, threshold=0.50, ink=(18, 14, 30)):
    """Push the darkest cel band to spot blacks, like the splash art inks."""
    a = np.array(im).astype(np.float32)
    rgb, al = a[..., :3], a[..., 3:]
    mx = rgb.max(-1, keepdims=True) / 255.0
    sat = (rgb.max(-1, keepdims=True) - rgb.min(-1, keepdims=True)) / (rgb.max(-1, keepdims=True) + 1)
    # shadow band = dark value; keep white wraps/hair from inking
    k = np.clip((threshold - mx) / 0.12, 0, 1) * np.clip(sat * 3 + 0.2, 0, 1)
    rgb = rgb * (1 - k) + np.array(ink) * k
    # more punch in the lights
    rgb = np.clip((rgb - 128) * 1.12 + 136, 0, 255)
    return Image.fromarray(np.concatenate([rgb, al], -1).astype(np.uint8))


def rim(im, color, dx=-2, dy=-2, strength=0.9):
    a = np.array(im).astype(np.float32)
    al = a[..., 3] / 255.0
    sh = np.roll(np.roll(al, -dx, 1), -dy, 0)
    edge = np.clip(al - sh, 0, 1)
    edge = np.array(Image.fromarray((edge * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6))) / 255.0 * al
    a[..., :3] = np.clip(a[..., :3] + edge[..., None] * np.array(color) * strength, 0, 255)
    return Image.fromarray(a.astype(np.uint8))


def cast_shadow(canvas, im, fx, fy, light=(-0.9, -0.45), opacity=0.6):
    """Blob contact shadow plus a sheared silhouette shadow away from the key light."""
    al = im.split()[3]
    sw, sh = im.width, max(2, int(im.height * 0.42))
    sil = al.resize((sw, sh), Image.BILINEAR)
    shear = 0.9
    big = Image.new("L", (sw + int(sh * shear) + 8, sh + 8), 0)
    arr = np.zeros((big.height, big.width), np.float32)
    s = np.array(sil).astype(np.float32)
    for y in range(sh):
        off = int((sh - 1 - y) * shear)
        arr[y + 4, off + 4: off + 4 + sw] = s[y]
    arr = arr[::-1]  # flip: shadow falls down-right of feet
    big = Image.fromarray(arr.astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.2))
    layer = Image.new("L", (W, H), 0)
    layer.paste(big, (int(fx - sw / 2), int(fy - 4)))
    d = ImageDraw.Draw(layer)
    ell = Image.new("L", (W, H), 0)
    ImageDraw.Draw(ell).ellipse((fx - sw * 0.33, fy - 7, fx + sw * 0.33, fy + 7), fill=255)
    ell = ell.filter(ImageFilter.GaussianBlur(3))
    m = np.maximum(np.array(layer), np.array(ell)).astype(np.float32) / 255.0 * opacity
    return canvas * (1 - m[..., None])


def blit(canvas, im, fx, fy):
    """canvas float rgb 0..1; im RGBA; feet at (fx, fy)."""
    x0, y0 = int(fx - im.width / 2), int(fy - im.height)
    a = np.array(im).astype(np.float32) / 255.0
    x1, y1 = max(0, x0), max(0, y0)
    x2, y2 = min(W, x0 + im.width), min(H, y0 + im.height)
    sub = a[y1 - y0:y2 - y0, x1 - x0:x2 - x0]
    reg = canvas[y1:y2, x1:x2]
    canvas[y1:y2, x1:x2] = reg * (1 - sub[..., 3:]) + sub[..., :3] * sub[..., 3:]
    return canvas


# ---------------------------------------------------------------- vfx (additive)
def add_glow(canvas, layer_rgb, blur=(2, 8, 22), gains=(1.0, 0.8, 0.6)):
    out = canvas + layer_rgb
    for b, g in zip(blur, gains):
        im = Image.fromarray(np.clip(layer_rgb * 255, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(b))
        out = out + np.array(im).astype(np.float32) / 255.0 * g
    return out


def punch_fx(cx, cy, ang, color=(1.0, 0.55, 0.12)):
    L = Image.new("RGB", (W, H), 0)
    d = ImageDraw.Draw(L)
    c = tuple(int(255 * v) for v in color)
    hot = (255, 244, 214)
    # impact star
    for i in range(11):
        a = ang + (i - 5) * 0.34 + random.uniform(-0.1, 0.1)
        r1, r2 = random.uniform(5, 10), random.uniform(26, 58) * (1.25 - abs(i - 5) / 7)
        d.line((cx + math.cos(a) * r1, cy + math.sin(a) * r1 * 0.6,
                cx + math.cos(a) * r2, cy + math.sin(a) * r2 * 0.6), fill=c, width=3)
        d.line((cx + math.cos(a) * r1, cy + math.sin(a) * r1 * 0.6,
                cx + math.cos(a) * r2 * 0.6, cy + math.sin(a) * r2 * 0.36), fill=hot, width=1)
    d.ellipse((cx - 9, cy - 7, cx + 9, cy + 7), fill=hot)
    # swing arc behind the fist
    bx, by = cx - math.cos(ang) * 46, cy - math.sin(ang) * 26
    for k in range(3):
        r = 40 + k * 5
        d.arc((bx - r, by - r * 0.62, bx + r, by + r * 0.62),
              math.degrees(ang) - 62, math.degrees(ang) + 48, fill=c if k else hot, width=4 - k)
    # sparks
    for _ in range(16):
        a = ang + random.uniform(-0.9, 0.9)
        r = random.uniform(30, 90)
        x, y = cx + math.cos(a) * r, cy + math.sin(a) * r * 0.6 - random.uniform(0, 14)
        d.line((x, y, x + math.cos(a) * 5, y + math.sin(a) * 3), fill=c, width=2)
    return np.array(L).astype(np.float32) / 255.0


def poison_ring(cx, cy, r):
    L = Image.new("RGB", (W, H), 0)
    d = ImageDraw.Draw(L)
    for i in range(14):
        a0 = i * 360 / 14
        d.arc((cx - r, cy - r * 0.5, cx + r, cy + r * 0.5), a0, a0 + 15, fill=(70, 255, 110), width=2)
    return np.array(L).astype(np.float32) / 255.0


def hit_flash(im, amount=0.75):
    a = np.array(im).astype(np.float32)
    a[..., :3] = a[..., :3] * (1 - amount) + 255 * amount
    return Image.fromarray(a.astype(np.uint8))


def hp_bar(canvas, cx, y, frac, w=44):
    im = Image.fromarray(np.clip(canvas * 255, 0, 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    d.rectangle((cx - w / 2 - 1, y - 1, cx + w / 2 + 1, y + 5), fill=(8, 8, 14))
    d.rectangle((cx - w / 2, y, cx - w / 2 + w * frac, y + 4), fill=(110, 240, 90))
    return np.array(im).astype(np.float32) / 255.0


def grade(canvas):
    c = np.clip(canvas, 0, 4)
    c = c / (1 + c * 0.18) * 1.12          # soft shoulder
    c = np.clip(c, 0, 1) ** 0.94
    lum = c.mean(-1, keepdims=True)
    c = lum + (c - lum) * 1.12             # a touch more saturation
    return np.clip(c, 0, 1)


# ---------------------------------------------------------------- panels
def panel_today():
    g = Image.open(f"{REPO}/docs/prototype/wade_combat.gif")
    best, score = None, -1
    for i in range(0, g.n_frames, 6):
        g.seek(i)
        f = g.convert("RGB")
        s = np.array(f).std()
        if s > score:
            best, score = f.copy(), s
    f = best
    k = max(W / f.width, H / f.height)
    f = f.resize((round(f.width * k), round(f.height * k)), Image.NEAREST)
    x, y = (f.width - W) // 2, (f.height - H) // 2
    return np.array(f.crop((x, y, x + W, y + H))).astype(np.float32) / 255.0


def panel_target(hero, enemy, hero_is_forge):
    c = iso_ground()
    px, py = 270, 265
    ex, ey = 392, 236
    c = light_pool(c, px + 30, py - 6, 250, (1.0, 0.62, 0.32), 5.0)   # warm key around the fight
    c = light_pool(c, ex, ey, 90, (0.3, 1.0, 0.4), 2.0)               # enemy's own colour
    c = vignette(c)
    for im, x, y in ((enemy, ex, ey), (hero, px, py)):
        c = cast_shadow(c, im, x, y)
    c = add_glow(c, poison_ring(ex, ey, 34) * 0.55, blur=(1, 5), gains=(0.6, 0.5))
    e = rim(hit_flash(enemy, 0.55), (120, 255, 140))
    c = blit(c, e, ex, ey)
    h = ink_pass(hero) if hero_is_forge else hero
    h = rim(h, (255, 150, 60))
    c = blit(c, h, px, py)
    c = add_glow(c, punch_fx(ex - 20, ey - 34, -0.25))
    c = hp_bar(c, px, py - hero.height - 12, 0.8)
    c = hp_bar(c, ex, ey - enemy.height - 10, 0.35, 34)
    return grade(c)


def label(arr, text, sub):
    im = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(im)
    d.rectangle((0, 0, W, 44), fill=(6, 6, 10))
    d.text((12, 5), text, fill=(255, 255, 255), font=font(17))
    d.text((12, 26), sub, fill=(170, 175, 190), font=font(11))
    return im


wade_forge = trim(forge_cell("Wade", 176, 156, 26, 176 + 8 * 2 + 4))   # jab, mid-strike
tox = trim(forge_cell("Toxlith", 96, 88, 16, 0))
views = turnaround_views(f"{REPO}/assets/turnarounds/Wade.png")
print("turnaround views:", len(views), [v.size for v in views])
print("forge wade", wade_forge.size, "tox", tox.size)
hero_h = 104
wf = scale_to_h(wade_forge, hero_h)
wt = scale_to_h(views[1], hero_h)
tx = scale_to_h(tox, 52, smooth=False)

a = label(panel_today(), "A. Today", "forge sprite, flat ground, no lighting")
b = label(panel_target(wf, tx, True), "B. Same sprites + presentation pass",
          "dark floor, light pools, cast shadows, rim, ink blacks, glow")
c = label(panel_target(wt, tx, False), "C. AI-drawn source + presentation pass",
          "Wade from the existing turnaround sheet, same treatment")
sheet = Image.new("RGB", (W * 3 + 16, H), (0, 0, 0))
for i, p in enumerate((a, b, c)):
    sheet.paste(p, (i * (W + 8), 0))
sheet.save(OUT)
# close-up of the three Wades at 3x for the character-quality comparison
cu = Image.new("RGB", (3 * 330, 380), (16, 17, 26))
for i, im in enumerate((wf, rim(ink_pass(wf), (255, 150, 60)), rim(wt, (255, 150, 60)))):
    big = im.resize((im.width * 3, im.height * 3), Image.LANCZOS)
    cu.paste(big, (i * 330 + (330 - big.width) // 2, 380 - big.height - 20), big)
cu.save(OUT.replace(".png", "_closeup.png"))
