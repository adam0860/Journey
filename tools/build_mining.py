#!/usr/bin/env python3
"""
Journey - mining and smithing models (Blender / bpy)

  assets/models/props/rock_<copper|tin|iron|coal|mithril>.glb   a mineable rock with ore showing in it (origin = middle of its tile)
  assets/models/props/rock_empty.glb                            what is left after you mine it
  assets/models/props/furnace.glb, anvil.glb                    the smith's workstations (front = +z; the game draws the flames)
  assets/models/props/drop_ore_<kind>.glb, drop_bar_<metal>.glb, drop_hammer.glb   items lying on the ground
  assets/models/character/held_hammer.glb                       the smith's hammer in the hero's right fist (same space as build_gear.py)

Run:  blender -b --python tools/build_mining.py
"""
import bpy, math, os, random, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model as _Model

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_CHAR = os.path.join(ROOT, 'assets', 'models', 'character')
OUT_PROPS = os.path.join(ROOT, 'assets', 'models', 'props')

PAL = {
    'stone': '#9a9ca2', 'stone_dark': '#74767d', 'stone_light': '#c0bfb8', 'stone_warm': '#a29c92', 'sootstone': '#5e5a57',
    'wood': '#9c6c3e', 'wood_dark': '#5e3d1f', 'leather': '#5b3a20', 'iron': '#3a3c43', 'iron_dark': '#26282d', 'steel': '#b9c0cb',
    'coal': '#1d1a18', 'ember': '#ff8a22', 'ember_dark': '#c4470f',
    'copper': '#c4682f', 'copper_light': '#ec9d60', 'tin': '#cfd5dc', 'tin_light': '#f4f7fa', 'rust': '#8c4b2b', 'rust_light': '#c17541',
    'coalrock': '#2a2623', 'coalrock_light': '#4b4540', 'mithril': '#4a6fd8', 'mithril_light': '#9dbfff',
    'gold': '#e6b73a', 'gold_light': '#ffd966', 'gold_dark': '#a8801f', 'crust': '#b9803f', 'crust_light': '#d49c52', 'crust_dark': '#8a5a2a',
    'bronze': '#b8733a', 'bronze_light': '#e2a869', 'iron_bar': '#7e828c', 'iron_bar_light': '#a9aeb9',
    'steel_bar': '#b4bcc8', 'steel_bar_light': '#e4eaf3', 'mithril_bar': '#4a6fd8', 'mithril_bar_light': '#93b4ff',
    'clay': '#a8805f', 'clay_light': '#c9a07c', 'silverrock': '#c9cfdb', 'silver_light': '#f1f4fa', 'silver_bar': '#cfd5e0', 'silver_bar_light': '#f4f7fb',
    'gold_bar': '#e6b73a', 'gold_bar_light': '#ffe27a', 'sapphire': '#3b66e8', 'sapphire_light': '#92b0ff', 'emerald': '#27b862', 'emerald_light': '#8af0b4',
    'ruby': '#e02a42', 'ruby_light': '#ff8e9a',
    'adamant': '#2f8a55', 'adamant_light': '#93efb5', 'adamant_bar': '#3f9a62', 'adamant_bar_light': '#8fe6ae',
}
ORE = {  # kind -> (ore colour, highlight colour)
    'copper': ('copper', 'copper_light'), 'tin': ('tin', 'tin_light'), 'iron': ('rust', 'rust_light'),
    'coal': ('coalrock', 'coalrock_light'), 'mithril': ('mithril', 'mithril_light'), 'adamant': ('adamant', 'adamant_light'),
    'clay': ('clay', 'clay_light'), 'silver': ('silverrock', 'silver_light'), 'gold': ('gold', 'gold_light'),
    'gems': ('sapphire', 'sapphire_light'),
}
BAR = {'bronze': ('bronze', 'bronze_light'), 'iron': ('iron_bar', 'iron_bar_light'), 'steel': ('steel_bar', 'steel_bar_light'),
       'mithril': ('mithril_bar', 'mithril_bar_light'), 'adamant': ('adamant_bar', 'adamant_bar_light'), 'silver': ('silver_bar', 'silver_bar_light'), 'gold': ('gold_bar', 'gold_bar_light')}


def Model(name, origin=(0.0, 0.0, 0.0)):
    return _Model(name, origin=origin, palette=PAL)


def unit(rnd):
    """a random direction on the upper hemisphere"""
    while True:
        v = (rnd.uniform(-1, 1), rnd.uniform(0.05, 1), rnd.uniform(-1, 1))
        l = math.sqrt(sum(c * c for c in v))
        if 0.2 < l <= 1.0:
            return tuple(c / l for c in v)


def crystal(m, p, n, r, h, mat, mat_light):
    """A pointed prism standing on the surface at p, along the surface normal n."""
    nx, ny, nz = n
    ref = (0, 0, 1) if abs(nz) < 0.9 else (1, 0, 0)
    ux, uy, uz = ny * ref[2] - nz * ref[1], nz * ref[0] - nx * ref[2], nx * ref[1] - ny * ref[0]
    l = math.sqrt(ux * ux + uy * uy + uz * uz)
    ux, uy, uz = ux / l, uy / l, uz / l
    vx, vy, vz = ny * uz - nz * uy, nz * ux - nx * uz, nx * uy - ny * ux
    pts = []
    for i in range(5):
        a = 2 * math.pi * i / 5
        pts.append((p[0] + (ux * math.cos(a) + vx * math.sin(a)) * r - nx * 0.02, p[1] + (uy * math.cos(a) + vy * math.sin(a)) * r - ny * 0.02,
                    p[2] + (uz * math.cos(a) + vz * math.sin(a)) * r - nz * 0.02))
    pts.append((p[0] + nx * h, p[1] + ny * h, p[2] + nz * h))
    m.hull(pts, mat, 1.0)


# ------------------------------------------------------------------------------------------------ rocks
def rock_body(m, rnd, scale=1.0, low=1.0):
    """The stone itself: a main boulder with two smaller ones against it. Returns the main boulder (centre, radii)."""
    main = ((0.0, 0.34 * low, 0.0), (0.50 * scale, 0.42 * low * scale, 0.44 * scale))
    m.ellipsoid(main[0], main[1], 'stone', rot=(0.0, rnd.uniform(0, 3), 0.0), rows=4, cols=8, shade=0.98)
    m.ellipsoid((0.27 * scale, 0.20 * low, 0.22 * scale), (0.30 * scale, 0.24 * low, 0.28 * scale), 'stone_dark',
                rot=(0.2, rnd.uniform(0, 3), 0.1), rows=3, cols=7, shade=0.95)
    m.ellipsoid((-0.30 * scale, 0.16 * low, -0.18 * scale), (0.25 * scale, 0.19 * low, 0.23 * scale), 'stone_warm',
                rot=(0.0, rnd.uniform(0, 3), 0.3), rows=3, cols=7, shade=0.97)
    m.ellipsoid((0.0, 0.05, 0.43 * scale), (0.22, 0.11, 0.14), 'stone_dark', rows=3, cols=6, shade=0.9)
    return main


def build_rock(kind, seed):
    rnd = random.Random(seed)
    m = Model('rock_' + kind)
    (cx, cy, cz), (rx, ry, rz) = rock_body(m, rnd)
    c0, c1 = ORE[kind]
    for i in range(7):
        n = unit(rnd)
        p = (cx + rx * n[0] * 0.93, cy + ry * n[1] * 0.93, cz + rz * n[2] * 0.93)
        if kind in ('mithril', 'adamant'):
            crystal(m, p, n, 0.055 + 0.02 * (i % 2), 0.15 + 0.07 * (i % 3) / 2.0, c0 if i % 2 else c1, c1)
        elif kind == 'gems':   # crystals of three colours growing out of the stone
            gm = [('sapphire', 'sapphire_light'), ('emerald', 'emerald_light'), ('ruby', 'ruby_light')][i % 3]
            crystal(m, p, n, 0.055 + 0.02 * (i % 2), 0.13 + 0.06 * (i % 3) / 2.0, gm[0], gm[1])
        else:
            r = 0.075 + 0.025 * (i % 3)
            m.ellipsoid(p, (r, r * 0.85, r), c0 if i % 2 else c1, rot=(rnd.uniform(0, 1), rnd.uniform(0, 3), 0), rows=3, cols=6,
                        shade=0.97 if kind != 'coal' else 1.0)
    # a few ore flecks on the lower lumps too
    for p in ((0.34, 0.3, 0.32), (-0.34, 0.24, -0.2), (0.1, 0.18, 0.46)):
        m.ellipsoid(p, (0.05, 0.045, 0.05), c1, rows=3, cols=6)
    m.finish(OUT_PROPS)


def build_rock_empty():
    rnd = random.Random(99)
    m = Model('rock_empty')
    rock_body(m, rnd, 0.92, 0.62)
    for (x, z, r) in ((0.42, -0.30, 0.07), (-0.40, 0.34, 0.06), (0.05, -0.46, 0.05), (0.46, 0.30, 0.045)):   # rubble
        m.ellipsoid((x, 0.04, z), (r, r * 0.7, r), 'stone_dark', rows=3, cols=6)
    m.finish(OUT_PROPS)


# ------------------------------------------------------------------------------------------------ furnace and anvil
def build_furnace():
    m = Model('furnace')
    m.box((0, 0.06, 0), (0.49, 0.06, 0.46), 'stone_dark')                                              # plinth
    m.hull([(sx * 0.44, 0.12, sz * 0.40) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.36, 0.95, sz * 0.34) for sx in (-1, 1) for sz in (-1, 1)], 'stone')                  # the kiln
    for i, y in enumerate((0.26, 0.42, 0.58, 0.74)):                                                      # courses of stone
        k = 0.44 - (y - 0.12) / 0.83 * 0.08
        m.box((0, y, 0), (k + 0.012, 0.007, (k - 0.04) + 0.012), 'stone_dark' if i % 2 else 'stone_warm')
    m.hull([(sx * 0.36, 0.95, sz * 0.34) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.22, 1.30, sz * 0.22) for sx in (-1, 1) for sz in (-1, 1)], 'stone_dark')              # shoulder
    m.hull([(sx * 0.21, 1.30, sz * 0.21) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.17, 2.10, sz * 0.17) for sx in (-1, 1) for sz in (-1, 1)], 'sootstone')               # chimney
    m.box((0, 2.14, 0), (0.23, 0.05, 0.23), 'stone')                                                      # chimney cap
    m.box((0, 2.20, 0), (0.15, 0.015, 0.15), 'coal')
    # the mouth: a dark recess with an ember bed, framed in light stone
    m.box((0, 0.40, 0.385), (0.20, 0.17, 0.025), 'coal')
    m.box((0, 0.30, 0.395), (0.16, 0.07, 0.012), 'ember_dark')
    m.box((0, 0.27, 0.402), (0.12, 0.035, 0.008), 'ember')
    m.box((0, 0.64, 0.385), (0.27, 0.045, 0.045), 'stone_light')
    for sx in (-1, 1):
        m.box((sx * 0.245, 0.43, 0.385), (0.042, 0.22, 0.045), 'stone_light')
    m.box((0, 0.17, 0.40), (0.27, 0.025, 0.05), 'stone_light')                                            # hearth step
    # iron bands round the kiln
    for y in (0.32, 0.80):
        k = 0.44 - (y - 0.12) / 0.83 * 0.08
        m.hull([(sx * (k + 0.014), y - 0.025, sz * (k - 0.03)) for sx in (-1, 1) for sz in (-1, 1)] +
               [(sx * (k + 0.014), y + 0.025, sz * (k - 0.03)) for sx in (-1, 1) for sz in (-1, 1)], 'iron')
    # a bucket of coal and a poker leaning on the side
    m.rod((0.34, 0.0, 0.42), (0.34, 0.17, 0.42), 0.095, 0.115, 'wood_dark', n=8)
    m.rod((0.34, 0.15, 0.42), (0.34, 0.185, 0.42), 0.10, 0.10, 'iron', n=8)
    for (dx, dz, r) in ((0.0, 0.0, 0.05), (0.05, 0.03, 0.04), (-0.05, 0.02, 0.045)):
        m.ellipsoid((0.34 + dx, 0.20, 0.42 + dz), (r, r * 0.8, r), 'coalrock', rows=3, cols=6)
    m.rod((-0.46, 0.0, 0.22), (-0.40, 1.0, 0.12), 0.012, 0.012, 'iron', n=5)
    m.finish(OUT_PROPS)


def build_anvil():
    m = Model('anvil')
    m.rod((0, 0, 0), (0, 0.44, 0), 0.30, 0.27, 'wood', n=9)                                               # the stump it stands on
    m.rod((0, 0.04, 0), (0, 0.10, 0), 0.305, 0.305, 'wood_dark', n=9)
    m.rod((0, 0.38, 0), (0, 0.44, 0), 0.275, 0.275, 'iron', n=9)
    m.box((0, 0.47, 0), (0.19, 0.03, 0.115), 'iron_dark')                                                 # foot
    m.hull([(sx * 0.10, 0.50, sz * 0.075) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.13, 0.66, sz * 0.095) for sx in (-1, 1) for sz in (-1, 1)], 'iron')                    # waist
    m.box((0, 0.715, 0), (0.27, 0.05, 0.105), 'iron')                                                     # body
    m.box((0, 0.766, 0), (0.255, 0.008, 0.095), 'steel')                                                  # the polished face
    m.hull([(0.27, 0.665, -0.1), (0.27, 0.665, 0.1), (0.27, 0.765, -0.1), (0.27, 0.765, 0.1), (0.46, 0.72, 0.0)], 'iron')   # horn
    m.box((-0.31, 0.735, 0), (0.04, 0.03, 0.07), 'iron_dark')                                             # heel
    m.box((-0.17, 0.772, 0), (0.02, 0.003, 0.02), 'coal')                                                 # hardy hole
    m.finish(OUT_PROPS)


# ------------------------------------------------------------------------------------------------ items
def build_ore_drop(kind, seed):
    rnd = random.Random(seed)
    c0, c1 = ORE[kind]
    m = Model('drop_ore_' + kind)
    m.ellipsoid((0.0, 0.07, 0.0), (0.13, 0.085, 0.11), 'stone', rot=(0, 0.6, 0), rows=3, cols=7, shade=0.97)
    m.ellipsoid((0.10, 0.05, 0.08), (0.08, 0.05, 0.07), 'stone_dark', rot=(0, 1.2, 0), rows=3, cols=6)
    for i in range(4):
        n = unit(rnd)
        p = (0.13 * n[0] * 0.9, 0.07 + 0.085 * n[1] * 0.9, 0.11 * n[2] * 0.9)
        if kind in ('mithril', 'adamant'):
            crystal(m, p, n, 0.03, 0.09, c0 if i % 2 else c1, c1)
        else:
            m.ellipsoid(p, (0.04, 0.034, 0.04), c0 if i % 2 else c1, rows=3, cols=6)
    m.finish(OUT_PROPS)


def ingot(m, c, yaw, mat, mat_light, y0=0.0):
    dx, dz = math.cos(yaw), math.sin(yaw)
    def pt(a, b, y):
        return (c[0] + dx * a - dz * b, y, c[2] + dz * a + dx * b)
    m.hull([pt(sa * 0.145, sb * 0.065, y0) for sa in (-1, 1) for sb in (-1, 1)] +
           [pt(sa * 0.115, sb * 0.048, y0 + 0.055) for sa in (-1, 1) for sb in (-1, 1)], mat)
    m.hull([pt(sa * 0.108, sb * 0.04, y0 + 0.054) for sa in (-1, 1) for sb in (-1, 1)] +
           [pt(sa * 0.104, sb * 0.037, y0 + 0.058) for sa in (-1, 1) for sb in (-1, 1)], mat_light)


def build_bar_drop(tier):
    m = Model('drop_bar_' + tier)
    c0, c1 = BAR[tier]
    ingot(m, (0.0, 0, -0.075), 0.0, c0, c1)
    ingot(m, (0.0, 0, 0.075), 0.0, c0, c1)
    ingot(m, (0.0, 0, 0.0), 0.0, c0, c1, 0.058)
    m.finish(OUT_PROPS)


def hammer(m):
    m.rod((0, 0.06, 0), (0, -0.28, 0), 0.017, 0.015, 'wood', n=7)
    m.rod((0, 0.06, 0), (0, -0.09, 0), 0.021, 0.021, 'leather', n=7)
    m.ellipsoid((0, 0.075, 0), (0.025, 0.02, 0.025), 'wood_dark', rows=3, cols=7)
    m.box((0, -0.30, 0), (0.034, 0.034, 0.072), 'iron')
    for sz in (-1, 1):
        m.box((0, -0.30, sz * 0.074), (0.038, 0.038, 0.007), 'steel')


def build_coins():
    m = Model('drop_coins')
    rnd = random.Random(5)
    for i in range(9):   # a little heap of gold coins
        a = rnd.uniform(0, 6.28)
        r = rnd.uniform(0.0, 0.11)
        h = 0.012 + 0.014 * (i // 3)
        cx, cz = math.cos(a) * r, math.sin(a) * r
        m.rod((cx, h, cz), (cx, h + 0.012, cz), 0.055, 0.055, 'gold' if i % 2 else 'gold_light', n=9)
        m.rod((cx, h + 0.012, cz), (cx, h + 0.016, cz), 0.04, 0.04, 'gold_dark', n=9)
    m.rod((0.17, 0.0, 0.05), (0.17, 0.014, 0.05), 0.055, 0.055, 'gold', n=9)
    m.finish(OUT_PROPS)


def build_bread():
    m = Model('drop_bread')
    m.ellipsoid((0, 0.05, 0), (0.16, 0.055, 0.085), 'crust', rows=4, cols=9)
    m.ellipsoid((0, 0.075, 0), (0.12, 0.03, 0.06), 'crust_light', rows=3, cols=8)
    for dx in (-0.07, 0.0, 0.07):
        m.box((dx, 0.093, 0.0), (0.012, 0.004, 0.05), 'crust_dark')
    m.finish(OUT_PROPS)


def build_hammer():
    m = Model('held_hammer')
    hammer(m)
    m.finish(OUT_CHAR)
    m = Model('drop_hammer')
    m.xf = lambda p: (-p[1] - 0.11, p[0] + 0.036, p[2])
    hammer(m)
    m.finish(OUT_PROPS)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for i, k in enumerate(ORE):
        build_rock(k, 100 + i * 7)
        if k != 'gems':
            build_ore_drop(k, 300 + i)
    build_rock_empty()
    build_furnace()
    build_anvil()
    for t in BAR:
        build_bar_drop(t)
    build_hammer()
    build_coins()
    build_bread()
    print('mining models built')
