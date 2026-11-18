#!/usr/bin/env python3
"""Procedural tileable textures for Journey style B.

All textures are float RGB arrays (sRGB-encoded 0..1) of shape (N, N, 3) that tile
seamlessly. Every generator is deterministic (seeded).  Run directly to write PNGs
into assets/textures/ and a contact sheet to build/tex_sheet.png.
"""
import os, sys
import numpy as np
from PIL import Image

N = 256


# ---------------------------------------------------------------- noise primitives
def _smooth(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


def value_noise(n, cells, rng):
    """Periodic value noise, n x n, `cells` lattice cells per side."""
    lat = rng.random((cells, cells))
    xs = np.linspace(0, cells, n, endpoint=False)
    xi = np.floor(xs).astype(int)
    xf = _smooth(xs - xi)
    x0, x1 = xi % cells, (xi + 1) % cells
    a = lat[np.ix_(x0, x0)]
    b = lat[np.ix_(x0, x1)]
    c = lat[np.ix_(x1, x0)]
    d = lat[np.ix_(x1, x1)]
    fx = xf[None, :]
    fy = xf[:, None]
    top = a * (1 - fx) + b * fx
    bot = c * (1 - fx) + d * fx
    return top * (1 - fy) + bot * fy


def fbm(n, base, octaves, rng, gain=0.5):
    out = np.zeros((n, n))
    amp, tot = 1.0, 0.0
    c = base
    for _ in range(octaves):
        out += amp * value_noise(n, c, rng)
        tot += amp
        amp *= gain
        c *= 2
    return out / tot


def voronoi(n, cells, rng, jitter=0.9):
    """Periodic voronoi. Returns (f1, f2, cell_id_value)."""
    pts = (np.stack(np.meshgrid(np.arange(cells), np.arange(cells), indexing='ij'), -1)
           + 0.5 + (rng.random((cells, cells, 2)) - 0.5) * jitter)
    vals = rng.random((cells, cells))
    g = (np.arange(n) + 0.5) / n * cells
    gy, gx = np.meshgrid(g, g, indexing='ij')
    f1 = np.full((n, n), 1e9)
    f2 = np.full((n, n), 1e9)
    val = np.zeros((n, n))
    ci = np.floor(gy).astype(int)
    cj = np.floor(gx).astype(int)
    for di in (-1, 0, 1):
        for dj in (-1, 0, 1):
            ii = ci + di
            jj = cj + dj
            p = pts[ii % cells, jj % cells]
            py = p[..., 0] + (ii - ii % cells)
            px = p[..., 1] + (jj - jj % cells)
            d = np.hypot(py - gy, px - gx)
            v = vals[ii % cells, jj % cells]
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            val = np.where(closer, v, val)
            f1 = np.where(closer, d, f1)
    return f1, f2, val


def lerp3(c0, c1, t):
    c0 = np.array(c0, float) / 255.0
    c1 = np.array(c1, float) / 255.0
    return c0[None, None, :] * (1 - t[..., None]) + c1[None, None, :] * t[..., None]


def hexc(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def finish(img):
    return np.clip(img, 0, 1)


def normalize(a):
    lo, hi = a.min(), a.max()
    return (a - lo) / (hi - lo + 1e-9)


# ---------------------------------------------------------------- textures
def grass(seed=1):
    rng = np.random.default_rng(seed)
    big = normalize(fbm(N, 3, 4, rng))
    mid = normalize(fbm(N, 16, 3, rng))
    fine = rng.random((N, N))
    # blade streaks: vertical-ish stretched noise
    blades = normalize(fbm(N, 64, 2, rng))
    t = 0.45 * big + 0.30 * mid + 0.15 * blades + 0.10 * fine
    t = normalize(t)
    img = lerp3(hexc('3d6b2a'), hexc('7fae48'), t)
    # sparse dry-patch / clover variation
    patch = normalize(fbm(N, 5, 3, rng))
    dry = np.clip((patch - 0.68) * 4, 0, 1) * 0.35
    img = img * (1 - dry[..., None]) + np.array(hexc('9a9a52')) / 255.0 * dry[..., None]
    # dark specks (shadowed blade bases)
    sp = (rng.random((N, N)) > 0.965).astype(float) * 0.18
    img = img * (1 - sp[..., None])
    return finish(img)


def dirt(seed=2):
    rng = np.random.default_rng(seed)
    big = normalize(fbm(N, 4, 4, rng))
    fine = normalize(fbm(N, 48, 2, rng))
    t = 0.65 * big + 0.35 * fine
    img = lerp3(hexc('6b4a2e'), hexc('a67c52'), normalize(t))
    f1, f2, v = voronoi(N, 20, rng, 1.0)
    peb = (f1 < 0.22).astype(float)
    shade = 0.78 + 0.35 * v
    img = img * (1 - 0.55 * peb[..., None]) + (np.array(hexc('8f8678')) / 255.0)[None, None, :] * shade[..., None] * 0.55 * peb[..., None]
    return finish(img)


def gravel(seed=3):
    rng = np.random.default_rng(seed)
    f1, f2, v = voronoi(N, 28, rng, 1.0)
    edge = np.clip((f2 - f1) * 7, 0, 1)
    base = lerp3(hexc('8d8a82'), hexc('c9c3b4'), v)
    shade = 0.55 + 0.45 * edge
    n = normalize(fbm(N, 24, 3, rng))
    img = base * shade[..., None] * (0.85 + 0.25 * n[..., None])
    return finish(img)


def sand(seed=4):
    rng = np.random.default_rng(seed)
    big = normalize(fbm(N, 6, 3, rng))
    fine = rng.random((N, N))
    ripple = 0.5 + 0.5 * np.sin((np.arange(N)[None, :] / N * 14 + big * 4) * 2 * np.pi)
    t = 0.5 * big + 0.15 * fine + 0.35 * ripple
    return finish(lerp3(hexc('c9b27c'), hexc('e8d9a4'), normalize(t)))


def cobble(seed=5):
    rng = np.random.default_rng(seed)
    f1, f2, v = voronoi(N, 9, rng, 0.75)
    edge = np.clip((f2 - f1) * 5.5, 0, 1)
    dome = np.clip(f1 * 3.0, 0, 1)  # darker toward centre? invert: lighter in middle
    base = lerp3(hexc('7a766e'), hexc('b3aca0'), 0.2 + 0.8 * v)
    n = normalize(fbm(N, 32, 3, rng))
    lit = (0.92 - 0.30 * dome + 0.18 * n)[..., None]
    img = base * lit * (0.30 + 0.70 * edge[..., None] ** 0.8)
    return finish(img)


def flagstone(seed=6):
    """Large square-ish paving slabs with mortar lines (for plaza / throne hall)."""
    rng = np.random.default_rng(seed)
    cells = 4
    img = np.zeros((N, N, 3))
    s = N // cells
    mort = np.zeros((N, N))
    base_n = normalize(fbm(N, 24, 4, rng))
    for i in range(cells):
        for j in range(cells):
            tone = 0.80 + rng.random() * 0.25
            k = rng.uniform(-6, 6); tint = np.array([k, k * 0.9, k * 0.8]) / 255.0
            y0, x0 = i * s, j * s
            img[y0:y0 + s, x0:x0 + s] = (np.array(hexc('a6a398')) / 255.0 + tint) * tone
    # mortar lines + bevel
    yy, xx = np.mgrid[0:N, 0:N]
    dy = np.minimum(yy % s, s - 1 - yy % s)
    dx = np.minimum(xx % s, s - 1 - xx % s)
    d = np.minimum(dy, dx).astype(float)
    mort = np.clip(1 - d / 2.0, 0, 1)
    bevel = np.clip(d / 7.0, 0, 1)
    img = img * (0.88 + 0.12 * bevel[..., None]) * (0.85 + 0.30 * base_n[..., None])
    img = img * (1 - mort[..., None]) + (np.array(hexc('5e5a52')) / 255.0)[None, None, :] * mort[..., None]
    return finish(img)


def stone_bricks(seed=7):
    """Running-bond castle blocks."""
    rng = np.random.default_rng(seed)
    rows, per = 8, 4
    bh = N // rows
    bw = N // per
    img = np.zeros((N, N, 3))
    mort = np.zeros((N, N))
    n = normalize(fbm(N, 32, 4, rng))
    for r in range(rows):
        off = (bw // 2) * (r % 2)
        for c in range(per + 1):
            x0 = c * bw - off
            tone = 0.82 + rng.random() * 0.28
            k = rng.uniform(-8, 8); tint = np.array([k, k * 0.9, k * 0.8]) / 255.0
            col = np.clip(np.array(hexc('9b978e')) / 255.0 + tint, 0, 1) * tone
            for x in range(x0, x0 + bw):
                img[r * bh:(r + 1) * bh, x % N] = col
    yy, xx = np.mgrid[0:N, 0:N]
    dy = np.minimum(yy % bh, bh - 1 - yy % bh).astype(float)
    row = yy // bh
    xs = (xx + (bw // 2) * (row % 2)) % bw
    dx = np.minimum(xs, bw - 1 - xs).astype(float)
    d = np.minimum(dy, dx)
    mort = np.clip(1 - d / 1.8, 0, 1)
    bevel = np.clip(d / 5.0, 0, 1)
    img = img * (0.84 + 0.16 * bevel[..., None]) * (0.88 + 0.24 * n[..., None])
    # grime near bottoms of courses
    grime = (1 - yy % bh / bh) ** 3 * 0.12
    img *= (1 - grime[..., None])
    img = img * (1 - mort[..., None]) + (np.array(hexc('55524b')) / 255.0)[None, None, :] * mort[..., None]
    return finish(img)


def plaster(seed=8):
    rng = np.random.default_rng(seed)
    big = normalize(fbm(N, 4, 4, rng))
    fine = normalize(fbm(N, 64, 2, rng))
    t = 0.7 * big + 0.3 * fine
    img = lerp3(hexc('d9ceb6'), hexc('f1e9d5'), normalize(t))
    # hairline cracks / stains
    st = np.clip((normalize(fbm(N, 6, 3, rng)) - 0.72) * 3, 0, 1) * 0.10
    img = img * (1 - st[..., None])
    return finish(img)


def planks(seed=9):
    """Vertical-run planks (horizontal grain via axis choice at UV time)."""
    rng = np.random.default_rng(seed)
    cnt = 6
    pw = N // cnt
    img = np.zeros((N, N, 3))
    grain_base = fbm(N, 6, 3, rng)
    yy, xx = np.mgrid[0:N, 0:N]
    for i in range(cnt):
        tone = 0.78 + rng.random() * 0.32
        g_off = rng.random() * 40
        # stretched grain: sample noise along y at high freq, x at low freq
        gx = value_noise(N, 3, rng)
        gy = value_noise(N, 40, rng)
        grain = 0.5 * gx + 0.5 * (0.5 + 0.5 * np.sin((xx / N * 60 + gx * 9 + g_off)))
        col = lerp3(hexc('6b4a2b'), hexc('a67a46'), normalize(grain))
        img[:, i * pw:(i + 1) * pw] = (col * tone)[:, i * pw:(i + 1) * pw]
    d = np.minimum(xx % pw, pw - 1 - xx % pw).astype(float)
    gap = np.clip(1 - d / 1.6, 0, 1)
    img = img * (1 - 0.75 * gap[..., None])
    # nail dots
    for i in range(cnt):
        for y in (14, N // 2 + 14):
            cx = i * pw + 6
            m = ((xx - cx) ** 2 + (yy - y) ** 2) < 3.0
            img[m] = img[m] * 0.45
    return finish(img)


def roof_tiles(seed=10):
    """Overlapping scalloped clay tiles."""
    rng = np.random.default_rng(seed)
    rows, per = 8, 8
    th, tw = N // rows, N // per
    yy, xx = np.mgrid[0:N, 0:N].astype(float)
    row = (yy // th).astype(int)
    xs = (xx + (tw / 2) * (row % 2))
    col = (xs // tw).astype(int)
    fx = (xs % tw) / tw
    fy = (yy % th) / th
    # per-tile tone
    tone = np.zeros((N, N))
    rr = rng.random((rows, per + 2))
    tone = rr[row, col % per] * 0.35 + 0.80
    # scallop: bottom edge curve
    scallop = fy + 0.18 * (1 - (2 * fx - 1) ** 2)
    shade = 0.62 + 0.55 * np.clip(1.0 - scallop * 0.9, 0, 1)
    edge = np.clip((1.05 - scallop) * 6, 0, 1)
    n = normalize(fbm(N, 28, 3, rng))
    base = lerp3(hexc('9c3d2a'), hexc('d06a45'), n)
    img = base * (tone * shade)[..., None]
    img *= (0.45 + 0.55 * edge)[..., None]
    return finish(img)


def slate_tiles(seed=11):
    rng = np.random.default_rng(seed)
    t = roof_tiles(seed)
    g = t.mean(-1, keepdims=True)
    n = normalize(fbm(N, 20, 3, rng))
    base = np.array(hexc('5c6a78')) / 255.0
    img = (0.35 + g * 1.1) * base[None, None, :] * (0.9 + 0.2 * n[..., None])
    return finish(img)


def thatch(seed=12):
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:N, 0:N]
    streak = value_noise(N, 3, rng)
    s2 = 0.5 + 0.5 * np.sin(xx / N * 2 * np.pi * 48 + streak * 14)
    n = normalize(fbm(N, 16, 3, rng))
    t = 0.5 * s2 + 0.5 * n
    return finish(lerp3(hexc('8a6a2e'), hexc('d1b060'), normalize(t)))


def bark(seed=13):
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:N, 0:N]
    w = value_noise(N, 4, rng)
    ridge = 0.5 + 0.5 * np.sin(xx / N * 2 * np.pi * 14 + w * 8)
    n = normalize(fbm(N, 8, 4, rng))
    t = 0.55 * ridge + 0.45 * n
    return finish(lerp3(hexc('2e2016'), hexc('7a5a3c'), normalize(t)))


def leaves(seed=14):
    rng = np.random.default_rng(seed)
    f1, f2, v = voronoi(N, 14, rng, 1.0)
    n = normalize(fbm(N, 20, 3, rng))
    base = lerp3(hexc('2c5a20'), hexc('7fb048'), 0.15 + 0.85 * v)
    shade = 0.84 + 0.30 * np.clip(f1 * 2.2, 0, 1) ** 0.7
    img = base * shade[..., None] * (0.9 + 0.2 * n[..., None])
    return finish(img)


def rock(seed=15):
    rng = np.random.default_rng(seed)
    n = normalize(fbm(N, 6, 5, rng))
    f1, f2, v = voronoi(N, 6, rng, 1.0)
    crack = np.clip((f2 - f1) * 14, 0, 1)
    t = 0.7 * n + 0.3 * v
    base = lerp3(hexc('5d5b57'), hexc('a8a49b'), normalize(t))
    return finish(base * (0.55 + 0.45 * crack[..., None]))


def water(seed=16):
    rng = np.random.default_rng(seed)
    a = normalize(fbm(N, 5, 4, rng))
    b = normalize(fbm(N, 9, 3, rng))
    caust = 1 - np.abs(2 * normalize(a * 0.6 + b * 0.4) - 1)
    caust = caust ** 6
    base = lerp3(hexc('2c6f9a'), hexc('4fa3c8'), normalize(a))
    img = base + caust[..., None] * np.array([0.25, 0.30, 0.30])[None, None, :]
    return finish(img)


def carpet(seed=17):
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:N, 0:N]
    weave = 0.5 + 0.5 * np.sin(xx * 2 * np.pi / 4) * np.sin(yy * 2 * np.pi / 4)
    n = normalize(fbm(N, 16, 3, rng))
    t = 0.7 + 0.15 * weave + 0.15 * n
    base = np.array(hexc('9b1c24')) / 255.0
    return finish(base[None, None, :] * t[..., None])


TEXTURES = {
    'grass': grass, 'dirt': dirt, 'gravel': gravel, 'sand': sand, 'cobble': cobble,
    'flagstone': flagstone, 'stone_bricks': stone_bricks, 'plaster': plaster,
    'planks': planks, 'roof_tiles': roof_tiles, 'slate_tiles': slate_tiles,
    'thatch': thatch, 'bark': bark, 'leaves': leaves, 'rock': rock, 'water': water,
    'carpet': carpet,
}


def to_png(arr, path):
    Image.fromarray((np.clip(arr, 0, 1) * 255 + 0.5).astype(np.uint8)).save(path)


def generate_all(outdir):
    os.makedirs(outdir, exist_ok=True)
    out = {}
    for name, fn in TEXTURES.items():
        a = fn()
        to_png(a, os.path.join(outdir, name + '.png'))
        out[name] = a
    return out


def contact_sheet(tex, path, cols=6, tile=192, tiled=False):
    names = list(tex)
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (tile + 6) + 6, rows * (tile + 6) + 6), (24, 24, 28))
    for i, n in enumerate(names):
        im = Image.fromarray((tex[n] * 255 + 0.5).astype(np.uint8))
        if tiled:  # show a 2x2 tiling to check seams
            big = Image.new('RGB', (N * 2, N * 2))
            for a in (0, 1):
                for b in (0, 1):
                    big.paste(im, (a * N, b * N))
            im = big
        im = im.resize((tile, tile), Image.LANCZOS)
        sheet.paste(im, (6 + (i % cols) * (tile + 6), 6 + (i // cols) * (tile + 6)))
    sheet.save(path)


if __name__ == '__main__':
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, 'assets', 'textures')
    tex = generate_all(out)
    os.makedirs(os.path.join(root, 'build'), exist_ok=True)
    contact_sheet(tex, os.path.join(root, 'build', 'tex_sheet.png'), tiled=('--tiled' in sys.argv))
    print('wrote', len(tex), 'textures to', out)
