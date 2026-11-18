#!/usr/bin/env python3
"""
Journey - held tools and campfire (Blender / bpy)

Builds small, flat-coloured models (no textures, so they look the same in art styles A and B):

  assets/models/character/held_axe.glb        bronze axe   \\  drawn in the hero's right hand
  assets/models/character/held_net.glb        small fishing net  >  (origin = the fist; the game rotates them with the arm)
  assets/models/character/held_tinderbox.glb  tinderbox    /
  assets/models/props/fire.glb                campfire logs inside a ring of stones (flames are drawn by the game)

Item space (what the game uses to put a tool in the hand): same axes as the character - y up, +z = forward, +x = the
hero's left. The origin is the middle of the fist; with the arm hanging down, a tool's handle runs along -y, so the
blade/net end is below the hand. The arm swing then moves the tool like a real overhead chop.

Every shape is convex, so each one is a convex hull of a few points (simple, and the normals always come out right).

Run:  blender -b --python tools/build_items.py
"""
import bpy, math, os, random, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model as _Model

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_CHAR = os.path.join(ROOT, 'assets', 'models', 'character')
OUT_PROPS = os.path.join(ROOT, 'assets', 'models', 'props')

PAL = {
    'bronze': '#b8733a', 'wood': '#9c6c3e', 'wood_dark': '#5e3d1f', 'iron': '#3d3f46', 'steel': '#aab0b8',
    'rope': '#c9b27a', 'net': '#e6dfc2', 'rock': '#8e9095', 'rock_dark': '#6e7076', 'coal': '#2b2623', 'trunk': '#6b4527',
    'bone': '#efe8d2', 'meat_raw': '#d95f6e', 'meat_raw_dark': '#bd4658', 'meat_cooked': '#a65a2c', 'meat_cooked_dark': '#7e4220',
    'fat': '#f1dcc9', 'char': '#2c211b', 'burnt': '#2f2925', 'burnt_dark': '#1f1b19',
    'shrimp_raw': '#dcaaa8', 'shrimp_raw_dark': '#c28b8b', 'shrimp_cooked': '#ee7449', 'shrimp_cooked_dark': '#d25a35',
    'fish_raw': '#7a98b6', 'fish_raw_belly': '#cdd9e4', 'fish_raw_fin': '#62809f', 'fish_cooked': '#c98d4a',
    'fish_cooked_belly': '#e9c991', 'fish_cooked_fin': '#a8702f', 'eye': '#151515', 'egg': '#f2e8d3',
    'hide': '#8a5a34', 'hide_dark': '#6c4527', 'hide_patch': '#2a2623', 'sack': '#a88556',
}




def Model(name):
    return _Model(name, palette=PAL)


# ------------------------------------------------------------------------------------------------ axe
def build_axe():
    m = Model('held_axe')
    # wooden handle from just above the fist down to the head, with a darker grip where the hand is
    m.rod((0, 0.09, 0), (0, -0.62, 0), 0.020, 0.017, 'wood', 8)
    m.rod((0, 0.07, 0), (0, -0.13, 0), 0.026, 0.026, 'wood_dark', 8)
    m.rod((0, 0.095, 0), (0, 0.075, 0), 0.030, 0.030, 'wood_dark', 8)      # butt knob
    # bronze head: thick at the handle, thin and flared at the cutting edge (which faces -z)
    m.hull([(sx * 0.024, y, z) for sx in (-1, 1) for (y, z) in ((-0.505, 0.034), (-0.415, 0.034), (-0.505, -0.020), (-0.415, -0.020))] +
           [(sx * 0.0055, y, -0.175) for sx in (-1, 1) for y in (-0.595, -0.325)], 'bronze')
    m.box((0, -0.46, 0.043), (0.030, 0.052, 0.020), 'bronze', 0.85)         # the poll (back of the head)
    m.finish(OUT_CHAR)


# ------------------------------------------------------------------------------------------------ net
def build_net():
    m = Model('held_net')
    m.rod((0, 0.07, 0), (0, -0.66, 0), 0.016, 0.013, 'wood', 8)
    m.rod((0, 0.05, 0), (0, -0.12, 0), 0.021, 0.021, 'wood_dark', 8)       # grip
    cy, rad, n = -0.67, 0.125, 14                                           # hoop at the end of the pole
    ring = [(math.cos(2 * math.pi * i / n) * rad, cy, math.sin(2 * math.pi * i / n) * rad) for i in range(n)]
    for i in range(n):
        m.rod(ring[i], ring[(i + 1) % n], 0.011, 0.011, 'wood_dark', 5)
    # the net bag hanging below the hoop: a pale cone with darker cords over it
    apex = (0, cy - 0.25, 0)
    m.hull([(x * 0.985, y, z * 0.985) for (x, y, z) in ring] + [apex], 'net', 0.95)
    for i in range(0, n, 2):
        x, y, z = ring[i]
        m.rod((x * 1.02, y, z * 1.02), (apex[0], apex[1] - 0.004, apex[2]), 0.0045, 0.0030, 'rope', 4)
    for k in (0.35, 0.65):                                                  # cross cords
        ry = cy + (apex[1] - cy) * k
        rr = rad * (1 - k) * 1.02
        pts = [(math.cos(2 * math.pi * i / 10) * rr, ry, math.sin(2 * math.pi * i / 10) * rr) for i in range(10)]
        for i in range(10):
            m.rod(pts[i], pts[(i + 1) % 10], 0.0045, 0.0045, 'rope', 4)
    m.finish(OUT_CHAR)


# ------------------------------------------------------------------------------------------------ tinderbox
def build_tinderbox():
    m = Model('held_tinderbox')
    m.box((0, -0.005, 0.055), (0.075, 0.034, 0.052), 'wood_dark')           # the box
    m.box((0, 0.031, 0.055), (0.078, 0.008, 0.055), 'iron')                  # tin lid
    m.box((0, -0.005, 0.055), (0.080, 0.006, 0.056), 'iron', 0.9)            # metal band round the middle
    m.box((0.026, 0.046, 0.050), (0.018, 0.008, 0.014), 'steel')             # flint and steel on the lid
    m.box((-0.026, 0.044, 0.064), (0.014, 0.007, 0.016), 'rock')
    m.finish(OUT_CHAR)


# ------------------------------------------------------------------------------------------------ fire
def build_fire():
    m = Model('fire')
    m.scale = 0.72   # a campfire fits inside one tile
    rnd = random.Random(7)
    # stone ring
    for i in range(10):
        a = 2 * math.pi * i / 10 + rnd.uniform(-0.1, 0.1)
        r = 0.58 + rnd.uniform(-0.03, 0.04)
        cx, cz = math.cos(a) * r, math.sin(a) * r
        sx, sy, sz = rnd.uniform(0.09, 0.14), rnd.uniform(0.06, 0.10), rnd.uniform(0.09, 0.13)
        pts = [(cx + dx * sx * (0.8 if dy > 0 else 1.0) + rnd.uniform(-0.015, 0.015), 0.0 if dy < 0 else sy * 1.6,
                cz + dz * sz * (0.8 if dy > 0 else 1.0) + rnd.uniform(-0.015, 0.015)) for dx in (-1, 1) for dy in (-1, 1) for dz in (-1, 1)]
        m.hull(pts, 'rock' if i % 2 else 'rock_dark')
    # a dark bed of ash and coals
    m.rod((0, 0.0, 0), (0, 0.035, 0), 0.46, 0.40, 'coal', 12)
    # logs leaning together in the middle, plus two lying across the bottom
    for i in range(5):
        a = 2 * math.pi * i / 5 + 0.3
        m.rod((math.cos(a) * 0.40, 0.07, math.sin(a) * 0.40), (math.cos(a) * 0.06, 0.34, math.sin(a) * 0.06), 0.058, 0.05,
              'wood' if i % 2 else 'trunk', 7)
        m.rod((math.cos(a) * 0.075, 0.30, math.sin(a) * 0.075), (math.cos(a) * 0.04, 0.35, math.sin(a) * 0.04), 0.054, 0.052, 'coal', 7)
    for a in (0.5, 2.1):
        m.rod((math.cos(a) * 0.36, 0.075, math.sin(a) * 0.36), (-math.cos(a) * 0.36, 0.075, -math.sin(a) * 0.36), 0.06, 0.06, 'wood_dark', 7)
    m.finish(OUT_PROPS)


# ------------------------------------------------------------------------------------------------ ground items
# Little models of everything that can lie on the ground (see ItemInfo::model in Inventory.cpp). Each sits on y = 0.
def build_drops():
    def lying_bone(m, c, yaw, length=0.30):
        """a thigh-bone: a shaft with two knobbly ends, lying flat, rotated by yaw about the vertical"""
        dx, dz = math.cos(yaw) * length / 2, math.sin(yaw) * length / 2
        a, b = (c[0] - dx, 0.032, c[2] - dz), (c[0] + dx, 0.032, c[2] + dz)
        m.rod(a, b, 0.017, 0.017, 'bone', n=6)
        for end in (a, b):
            for side in (-1, 1):
                px, pz = -math.sin(yaw) * 0.022 * side, math.cos(yaw) * 0.022 * side
                m.ellipsoid((end[0] + px, 0.034, end[2] + pz), (0.026, 0.026, 0.026), 'bone', rows=3, cols=6)

    m = Model('drop_bones')
    lying_bone(m, (0.0, 0, 0.0), 0.4)
    lying_bone(m, (0.02, 0, -0.01), 2.0, 0.26)
    m.finish(OUT_PROPS)

    m = Model('drop_logs')
    m.rod((-0.26, 0.055, -0.06), (0.26, 0.055, -0.06), 0.055, 0.055, 'wood', n=7)
    m.rod((-0.26, 0.055, 0.06), (0.26, 0.055, 0.06), 0.055, 0.055, 'trunk', n=7)
    m.rod((-0.25, 0.150, 0.0), (0.25, 0.150, 0.0), 0.055, 0.055, 'wood', n=7)
    m.finish(OUT_PROPS)

    def drumstick(name, meat, meat_dark):
        m = Model(name)
        m.ellipsoid((0.0, 0.07, 0.03), (0.075, 0.065, 0.10), meat, rot=(0.0, 0.0, 0.3), rows=4, cols=8)
        m.ellipsoid((0.0, 0.065, -0.035), (0.05, 0.05, 0.06), meat_dark, rot=(0.0, 0.0, 0.3), rows=4, cols=7)
        m.rod((0.02, 0.06, -0.08), (0.04, 0.05, -0.19), 0.017, 0.015, 'bone', n=6)
        for sx in (-1, 1):
            m.ellipsoid((0.045 + sx * 0.016, 0.05, -0.205), (0.022, 0.022, 0.022), 'bone', rows=3, cols=6)
        m.finish(OUT_PROPS)
    drumstick('drop_drumstick_raw', 'meat_raw', 'meat_raw_dark')
    drumstick('drop_drumstick_cooked', 'meat_cooked', 'meat_cooked_dark')

    def steak(name, body, rim, marks=None):
        m = Model(name)
        m.ellipsoid((0.0, 0.03, 0.0), (0.135, 0.028, 0.10), body, rot=(0.0, 0.5, 0.0), rows=3, cols=9)
        m.ellipsoid((0.045, 0.035, 0.03), (0.07, 0.026, 0.055), rim, rot=(0.0, 0.5, 0.0), rows=3, cols=7)
        if marks:
            for off in (-0.04, 0.0, 0.04):
                m.box((off * math.cos(0.5), 0.057, -off * math.sin(0.5)), (0.075, 0.004, 0.011), marks)
        m.finish(OUT_PROPS)
    steak('drop_steak_raw', 'meat_raw', 'fat')
    steak('drop_steak_cooked', 'meat_cooked', 'meat_cooked_dark', 'char')
    steak('drop_steak_burnt', 'burnt', 'burnt_dark')

    def shrimp(name, body, shell):
        m = Model(name)
        n = 7
        for i in range(n):
            t = i / (n - 1)
            ang = math.pi * (0.1 + 0.85 * t)
            r = 0.062 - 0.028 * t
            m.ellipsoid((math.cos(ang) * 0.13, 0.05 + 0.02 * math.sin(t * 3), math.sin(ang) * 0.10 - 0.05), (r, r * 0.9, r),
                        shell if i % 2 else body, rows=3, cols=6)
        m.hull([(-0.14, 0.03, -0.03), (-0.17, 0.045, -0.06), (-0.18, 0.03, 0.0), (-0.15, 0.025, 0.01)], body)
        m.rod((0.15, 0.05, -0.04), (0.23, 0.06, -0.10), 0.006, 0.003, shell, n=4)
        m.finish(OUT_PROPS)
    shrimp('drop_shrimp_raw', 'shrimp_raw', 'shrimp_raw_dark')
    shrimp('drop_shrimp_cooked', 'shrimp_cooked', 'shrimp_cooked_dark')

    def fish(name, body, belly, fin):
        m = Model(name)
        m.ellipsoid((0.0, 0.055, 0.0), (0.15, 0.05, 0.055), body, rows=4, cols=8)
        m.ellipsoid((-0.01, 0.04, 0.0), (0.12, 0.035, 0.05), belly, rows=3, cols=8)
        m.hull([(0.13, 0.055, 0.0), (0.22, 0.09, 0.06), (0.22, 0.09, -0.06), (0.22, 0.02, 0.0)], fin)
        m.hull([(-0.03, 0.095, 0.0), (0.04, 0.10, 0.0), (0.0, 0.14, 0.0), (-0.02, 0.10, 0.012)], fin)
        for sx in (-1, 1):
            m.ellipsoid((-0.115, 0.07, sx * 0.04), (0.014, 0.014, 0.01), 'eye', rows=3, cols=5)
        m.finish(OUT_PROPS)
    fish('drop_fish_raw', 'fish_raw', 'fish_raw_belly', 'fish_raw_fin')
    fish('drop_fish_cooked', 'fish_cooked', 'fish_cooked_belly', 'fish_cooked_fin')
    fish('drop_fish_burnt', 'burnt', 'burnt_dark', 'burnt_dark')

    m = Model('drop_egg')
    m.ellipsoid((0.0, 0.052, 0.0), (0.038, 0.052, 0.038), 'egg', rows=4, cols=8)
    m.ellipsoid((0.07, 0.04, 0.04), (0.034, 0.045, 0.034), 'egg', rot=(0.0, 0.0, 1.3), rows=4, cols=8)
    m.finish(OUT_PROPS)

    m = Model('drop_hide')
    m.ellipsoid((0.0, 0.03, 0.0), (0.22, 0.03, 0.17), 'hide', rows=3, cols=10)
    m.ellipsoid((0.0, 0.045, 0.0), (0.18, 0.025, 0.13), 'hide_dark', rot=(0, 0.3, 0), rows=3, cols=9)
    m.ellipsoid((-0.07, 0.06, 0.03), (0.07, 0.015, 0.06), 'hide_patch', rot=(0, 0.5, 0), rows=3, cols=7)
    for sx, sz in ((-1, -1), (1, -1), (-1, 1), (1, 1)):                                   # four floppy leg corners
        m.ellipsoid((sx * 0.2, 0.02, sz * 0.15), (0.06, 0.018, 0.05), 'hide', rot=(0, sx * sz * 0.8, 0), rows=3, cols=6)
    m.finish(OUT_PROPS)

    m = Model('drop_bundle')                                                              # anything else: a little sack
    m.ellipsoid((0.0, 0.085, 0.0), (0.12, 0.09, 0.12), 'sack', rows=4, cols=8)
    m.ellipsoid((0.0, 0.17, 0.0), (0.05, 0.045, 0.05), 'sack', rows=3, cols=7)
    m.rod((0.0, 0.14, 0.0), (0.0, 0.16, 0.0), 0.062, 0.062, 'rope', n=7)
    m.finish(OUT_PROPS)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    build_axe()
    build_net()
    build_tinderbox()
    build_fire()
    build_drops()
