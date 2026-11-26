#!/usr/bin/env python3
"""
Journey - models for round 10, the workshop round (Blender / bpy)

  assets/models/props/drop_<name>.glb          things lying on the ground: the kitchen (flour, jug, cheese, tomato, potato, apple, onion, cabbage,
                                               berries, milk, dough, pies, stew, cake, pizza), tools (knife, spade, candle, key, book, studs,
                                               bowstring, unstrung bow) and the logs of the new trees (maple, teak, mahogany, magic)
  assets/models/character/gear_<hard|studded>_<cap|body|chaps>*.glb      hard and studded leather armour (the leather armour shapes of
                                               build_crafting.py in a darker brown; studded ones get little steel studs)

Everything is a convex hull (see lowpoly.py), flat colours, no textures. Items lie flat on the ground, no bigger than about 0.4 m.

Run:  blender -b --python tools/build_workshop.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model as _Model
import build_crafting as bc

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_CHAR = os.path.join(ROOT, 'assets', 'models', 'character')
OUT_PROPS = os.path.join(ROOT, 'assets', 'models', 'props')

PAL = {
    'wood': '#9c6c3e', 'wood_dark': '#5e3d1f', 'steel': '#b9c0cb', 'steel_dark': '#7d838c', 'iron': '#4a4c54', 'rope': '#c9b27a', 'string': '#e8e0be',
    'sack': '#d9c9a0', 'sack_dark': '#b8a678', 'flour': '#f6f1e2', 'jug': '#b46a46', 'jug_dark': '#8e4f33', 'water': '#4a86c8',
    'cheese': '#f0c64a', 'cheese_dark': '#d6a52f', 'tomato': '#d03a2e', 'tomato_dark': '#a32a22', 'leaf': '#4c8a3a', 'potato': '#b08a56', 'potato_dark': '#8a6a3e',
    'apple': '#7cae3c', 'apple_dark': '#5d8a2a', 'stem': '#5d3b21', 'milk': '#f8f8f4', 'bucket': '#8a8f9a', 'bucket_dark': '#666a74',
    'onion': '#d9b46a', 'onion_dark': '#b99244', 'cabbage': '#7cbf5a', 'cabbage_dark': '#5aa043', 'berry': '#c4243c', 'berry_dark': '#8a1a2c',
    'dough': '#ecd9a8', 'dough_dark': '#d2bc84',
    'crust_raw': '#e2cea0', 'crust_raw_dark': '#c9b27e', 'crust': '#c8863f', 'crust_dark': '#9a6228', 'dish': '#b8734a', 'dish_dark': '#8e4f30',
    'stew': '#8e5428', 'stew_chunk': '#c48248', 'stew_raw': '#c4be98', 'stew_raw_chunk': '#d66e60', 'bowl': '#bf724c', 'bowl_dark': '#8e4f30',
    'sponge': '#d6a660', 'sponge_dark': '#b88440', 'icing': '#f6ece6', 'cherry': '#c8283a', 'tin': '#78808c', 'batter': '#ecdcae',
    'pizza_crust': '#d69e54', 'pizza_sauce': '#c8462f', 'pizza_cheese': '#f4cc50', 'pizza_raw_crust': '#e2d0a4', 'pizza_raw_sauce': '#d66e60', 'pizza_raw_cheese': '#f0e2a0',
    'burnt': '#2f2925', 'burnt_dark': '#1f1b19',
    'candle': '#f0e4c4', 'candle_dark': '#d2c4a0', 'flame': '#ffb028', 'flame_core': '#fff0a0', 'key': '#7f8a5e', 'key_dark': '#5e6a42',
    'book': '#603a28', 'book_dark': '#3f2418', 'page': '#dccfa4', 'ribbon': '#c43a3a', 'studs': '#aab0b8',
    'maple': '#9a4a2c', 'maple_cut': '#e8b078', 'teak': '#96764a', 'teak_cut': '#eed6a0', 'mahogany': '#602822', 'mahogany_cut': '#b2625a', 'magic': '#6046a0', 'magic_cut': '#c8b0f0',
}


def M(name):
    return _Model(name, palette=PAL)


def done(m):
    m.finish(OUT_PROPS)


def blob(m, c, r, mat, rows=4, cols=8, rot=(0, 0, 0)):
    m.ellipsoid(c, r, mat, rot=rot, rows=rows, cols=cols)


# ------------------------------------------------------------------------------------------------ tools
def knife():
    m = M('drop_knife')
    m.rod((-0.13, 0.022, 0.0), (-0.02, 0.022, 0.0), 0.022, 0.020, 'wood', n=7)
    m.box((-0.015, 0.022, 0.0), (0.008, 0.026, 0.016), 'steel_dark')
    m.hull([(-0.01, 0.014, 0.0), (-0.01, 0.034, 0.0), (0.16, 0.014, 0.0), (0.10, 0.034, 0.0)] + [(-0.01, 0.024, 0.008), (-0.01, 0.024, -0.008)], 'steel')
    done(m)


def spade():
    m = M('drop_spade')
    m.rod((-0.20, 0.025, 0.0), (0.12, 0.025, 0.0), 0.016, 0.016, 'wood', n=7)
    m.rod((-0.20, 0.025, -0.05), (-0.20, 0.025, 0.05), 0.014, 0.014, 'wood_dark', n=6)         # the T handle
    m.hull([(0.10, 0.012, sz * 0.055) for sz in (-1, 1)] + [(0.10, 0.034, sz * 0.055) for sz in (-1, 1)] +
           [(0.27, 0.012, sz * 0.04) for sz in (-1, 1)] + [(0.27, 0.030, sz * 0.04) for sz in (-1, 1)] + [(0.31, 0.018, 0.0)], 'steel')
    done(m)


def candle():
    m = M('drop_candle')
    m.rod((0, 0.0, 0), (0, 0.012, 0), 0.06, 0.06, 'wood_dark', n=10)                           # a little saucer
    m.rod((0, 0.012, 0), (0, 0.16, 0), 0.026, 0.024, 'candle', n=9)
    m.rod((0, 0.158, 0), (0, 0.165, 0), 0.016, 0.012, 'candle_dark', n=7)
    m.ellipsoid((0, 0.195, 0), (0.014, 0.03, 0.014), 'flame', rows=3, cols=6)
    m.ellipsoid((0, 0.188, 0), (0.007, 0.018, 0.007), 'flame_core', rows=3, cols=5)
    done(m)


def key():
    m = M('drop_key')
    for k in range(10):
        a = 2 * math.pi * k / 10
        m.ellipsoid((-0.10 + math.cos(a) * 0.04, 0.014, math.sin(a) * 0.04), (0.013, 0.012, 0.013), 'key', rows=3, cols=5)
    m.rod((-0.06, 0.014, 0.0), (0.14, 0.014, 0.0), 0.012, 0.012, 'key', n=6)
    for x in (0.09, 0.125):
        m.box((x, 0.014, 0.022), (0.011, 0.012, 0.022), 'key_dark')
    done(m)


def book():
    m = M('drop_book')
    m.box((0, 0.03, 0), (0.12, 0.03, 0.09), 'book')
    m.box((0.01, 0.034, 0.0), (0.108, 0.024, 0.082), 'page')
    m.box((-0.113, 0.03, 0.0), (0.014, 0.032, 0.092), 'book_dark')
    m.box((0.02, 0.063, -0.03), (0.03, 0.004, 0.012), 'ribbon')
    done(m)


def studs():
    m = M('drop_studs')
    pts = [(-0.08, -0.04), (0.0, -0.06), (0.08, -0.03), (-0.04, 0.03), (0.05, 0.04), (0.0, 0.0), (-0.1, 0.05), (0.11, 0.05)]
    for x, z in pts:
        m.ellipsoid((x, 0.016, z), (0.026, 0.016, 0.026), 'studs', rows=3, cols=6)
    done(m)


def bowstring():
    m = M('drop_bowstring')
    for k in range(4):
        r = 0.05 + 0.02 * k
        pts = [(math.cos(2 * math.pi * i / 12) * r, 0.012 + 0.004 * k, math.sin(2 * math.pi * i / 12) * r) for i in range(12)]
        for i in range(12):
            m.rod(pts[i], pts[(i + 1) % 12], 0.006, 0.006, 'string', n=4)
    m.rod((0.10, 0.02, 0.05), (0.2, 0.012, 0.11), 0.005, 0.004, 'string', n=4)
    done(m)


def bow_unstrung():
    m = M('drop_bow_unstrung')
    prev = None
    for i in range(0, 13):
        t = -1.0 + i / 6.0
        p = (-0.10 + 0.20 * t * t - 0.0, 0.03, t * 0.30)
        if prev:
            m.rod(prev, p, 0.016, 0.016, 'wood', n=6)
        prev = p
    m.rod((-0.10, 0.03, -0.04), (-0.10, 0.03, 0.04), 0.022, 0.022, 'wood_dark', n=7)
    done(m)


# ------------------------------------------------------------------------------------------------ the kitchen
def flour():
    m = M('drop_flour')
    blob(m, (0, 0.085, 0), (0.115, 0.085, 0.09), 'sack', rows=4, cols=8)
    m.rod((0, 0.14, 0), (0, 0.17, 0), 0.06, 0.04, 'sack', n=8)
    m.rod((0, 0.145, 0), (0, 0.155, 0), 0.052, 0.052, 'rope', n=8)
    blob(m, (0.0, 0.176, 0.0), (0.055, 0.014, 0.055), 'flour', rows=3, cols=7)
    done(m)


def jug():
    m = M('drop_jug')
    m.rod((0, 0.0, 0), (0, 0.10, 0), 0.07, 0.085, 'jug', n=10)
    m.rod((0, 0.10, 0), (0, 0.15, 0), 0.085, 0.045, 'jug', n=10)
    m.rod((0, 0.15, 0), (0, 0.19, 0), 0.045, 0.05, 'jug_dark', n=10)
    m.rod((0, 0.185, 0), (0, 0.192, 0), 0.04, 0.04, 'water', n=8)
    for a in (-1, 1):
        m.ellipsoid((0.095, 0.11, 0.0), (0.022, 0.05, 0.012), 'jug_dark', rows=3, cols=5)
        break
    done(m)


def milk():
    m = M('drop_milk')
    m.rod((0, 0.0, 0), (0, 0.14, 0), 0.08, 0.105, 'bucket', n=10)
    m.rod((0, 0.14, 0), (0, 0.15, 0), 0.11, 0.11, 'bucket_dark', n=10)
    m.rod((0, 0.148, 0), (0, 0.156, 0), 0.095, 0.095, 'milk', n=10)
    done(m)


def cheese():
    m = M('drop_cheese')
    m.hull([(-0.12, 0.0, -0.09), (-0.12, 0.0, 0.09), (0.12, 0.0, 0.0), (-0.12, 0.07, -0.09), (-0.12, 0.07, 0.09), (0.12, 0.025, 0.0)], 'cheese')
    m.box((-0.12, 0.035, 0.0), (0.004, 0.026, 0.07), 'cheese_dark')
    blob(m, (-0.123, 0.04, -0.03), (0.006, 0.012, 0.012), 'cheese_dark', rows=3, cols=5)
    blob(m, (-0.123, 0.025, 0.04), (0.006, 0.008, 0.008), 'cheese_dark', rows=3, cols=5)
    done(m)


def tomato():
    m = M('drop_tomato')
    blob(m, (0, 0.06, 0), (0.07, 0.058, 0.07), 'tomato', rows=4, cols=9)
    blob(m, (0, 0.118, 0), (0.035, 0.012, 0.035), 'leaf', rows=3, cols=6)
    done(m)


def potato():
    m = M('drop_potato')
    blob(m, (0, 0.045, 0), (0.095, 0.05, 0.065), 'potato', rows=4, cols=8, rot=(0, 0.4, 0))
    blob(m, (0.07, 0.04, 0.05), (0.05, 0.035, 0.04), 'potato_dark', rows=3, cols=6)
    done(m)


def apple():
    m = M('drop_apple')
    blob(m, (0, 0.062, 0), (0.07, 0.062, 0.07), 'apple', rows=4, cols=9)
    m.rod((0, 0.115, 0), (0.01, 0.15, 0), 0.007, 0.005, 'stem', n=5)
    blob(m, (0.03, 0.14, 0.0), (0.025, 0.006, 0.014), 'leaf', rows=3, cols=5)
    done(m)


def onion():
    m = M('drop_onion')
    blob(m, (0, 0.058, 0), (0.07, 0.056, 0.07), 'onion', rows=4, cols=9)
    m.hull([(0.0, 0.17, 0.0)] + [(math.cos(2 * math.pi * k / 6) * 0.03, 0.10, math.sin(2 * math.pi * k / 6) * 0.03) for k in range(6)], 'onion_dark')
    done(m)


def cabbage():
    m = M('drop_cabbage')
    blob(m, (0, 0.085, 0), (0.105, 0.085, 0.105), 'cabbage_dark', rows=4, cols=10)
    blob(m, (0.0, 0.10, 0.0), (0.08, 0.07, 0.08), 'cabbage', rows=4, cols=8)
    blob(m, (0.0, 0.13, 0.0), (0.04, 0.04, 0.04), 'cabbage', rows=3, cols=6)
    done(m)


def berries():
    m = M('drop_berries')
    for x, z, y in ((-0.05, 0.0, 0.032), (0.045, 0.02, 0.032), (0.0, -0.045, 0.032), (0.0, 0.03, 0.072)):
        blob(m, (x, y, z), (0.036, 0.034, 0.036), 'berry', rows=3, cols=7)
    blob(m, (0.0, 0.12, 0.0), (0.045, 0.008, 0.03), 'leaf', rows=3, cols=6)
    done(m)


def dough():
    m = M('drop_dough')
    blob(m, (0, 0.04, 0), (0.12, 0.045, 0.09), 'dough_dark', rows=3, cols=8)
    blob(m, (-0.02, 0.06, 0.0), (0.09, 0.05, 0.07), 'dough', rows=3, cols=8)
    blob(m, (0.06, 0.05, 0.03), (0.05, 0.03, 0.04), 'dough', rows=3, cols=6)
    done(m)


def pie(name, raw, burnt=False):
    m = M(name)
    top, dark = ('burnt', 'burnt_dark') if burnt else (('crust_raw', 'crust_raw_dark') if raw else ('crust', 'crust_dark'))
    m.rod((0, 0.0, 0), (0, 0.045, 0), 0.115, 0.14, 'dish' if not burnt else 'burnt_dark', n=12)       # the pie dish
    m.rod((0, 0.045, 0), (0, 0.075, 0), 0.14, 0.125, top, n=12)                                        # the crust
    blob(m, (0, 0.078, 0), (0.12, 0.022, 0.12), top, rows=3, cols=10)
    for k in range(3):                                                                                 # a lattice on top
        o = -0.05 + 0.05 * k
        m.box((o, 0.098, 0), (0.008, 0.005, 0.1 - abs(o) * 0.6), dark)
        m.box((0, 0.098, o), (0.1 - abs(o) * 0.6, 0.005, 0.008), dark)
    done(m)


def stew(name, raw):
    m = M(name)
    m.rod((0, 0.0, 0), (0, 0.06, 0), 0.07, 0.115, 'bowl', n=12)
    m.rod((0, 0.06, 0), (0, 0.068, 0), 0.118, 0.118, 'bowl_dark', n=12)
    m.rod((0, 0.066, 0), (0, 0.074, 0), 0.098, 0.098, 'stew_raw' if raw else 'stew', n=12)
    for x, z in ((-0.04, 0.02), (0.03, -0.03), (0.04, 0.04), (-0.02, -0.04)):
        blob(m, (x, 0.08, z), (0.022, 0.016, 0.022), 'stew_raw_chunk' if raw else 'stew_chunk', rows=3, cols=5)
    done(m)


def cake(name, raw):
    m = M(name)
    if raw:
        m.rod((0, 0.0, 0), (0, 0.07, 0), 0.115, 0.125, 'tin', n=12)
        m.rod((0, 0.066, 0), (0, 0.074, 0), 0.105, 0.105, 'batter', n=12)
    else:
        m.rod((0, 0.0, 0), (0, 0.085, 0), 0.115, 0.115, 'sponge', n=12)
        m.rod((0, 0.04, 0), (0, 0.047, 0), 0.118, 0.118, 'sponge_dark', n=12)
        m.rod((0, 0.085, 0), (0, 0.105, 0), 0.118, 0.112, 'icing', n=12)
        for k in range(6):
            a = 2 * math.pi * k / 6
            m.ellipsoid((math.cos(a) * 0.115, 0.082, math.sin(a) * 0.115), (0.022, 0.026, 0.022), 'icing', rows=3, cols=5)
        m.ellipsoid((0, 0.13, 0), (0.026, 0.026, 0.026), 'cherry', rows=3, cols=6)
    done(m)


def pizza(name, raw):
    m = M(name)
    crust, sauce, cheese = ('pizza_raw_crust', 'pizza_raw_sauce', 'pizza_raw_cheese') if raw else ('pizza_crust', 'pizza_sauce', 'pizza_cheese')
    m.rod((0, 0.0, 0), (0, 0.022, 0), 0.17, 0.17, crust, n=14)
    m.rod((0, 0.02, 0), (0, 0.032, 0), 0.145, 0.145, sauce, n=14)
    for k in range(6):
        a = 2 * math.pi * k / 6
        m.ellipsoid((math.cos(a) * 0.07, 0.036, math.sin(a) * 0.07), (0.03, 0.012, 0.03), cheese, rows=3, cols=6)
    m.ellipsoid((0, 0.038, 0), (0.032, 0.012, 0.032), cheese, rows=3, cols=6)
    done(m)


# ------------------------------------------------------------------------------------------------ hard and studded leather
def leather_variant(name_prefix, studded):
    pal = dict(bc.PAL)
    pal.update({'leather': '#6e4628', 'leather_dark': '#43281a', 'leather_light': '#8a5c36', 'gold': '#aab0b8' if studded else '#cfa24a'})
    J, TORSO = bc.J, bc.TORSO

    def mk(name, origin):
        return _Model(name, origin=origin, palette=pal)

    m = mk('gear_%s_cap' % name_prefix, J['head'])
    bc.leather_cap(m)
    if studded:
        for k in range(8):
            a = 2 * math.pi * k / 8
            m.ellipsoid((math.cos(a) * 0.136, 1.675, math.sin(a) * 0.144), (0.012, 0.012, 0.012), 'steel', rows=3, cols=5)
    m.finish(OUT_CHAR)
    for g in 'mf':
        m = mk('gear_%s_body_torso_%s' % (name_prefix, g), J['torso'])
        bc.leather_torso(m, g)
        if studded:
            zf = max(r[2] + r[3] for r in TORSO[g] if 1.07 <= r[0] <= 1.46) + 0.024
            for y in (1.12, 1.20, 1.28, 1.36, 1.43):
                for x in (-0.13, -0.065, 0.065, 0.13):
                    m.ellipsoid((x, y, zf - 0.004), (0.011, 0.011, 0.008), 'steel', rows=3, cols=5)
        m.finish(OUT_CHAR)
        m = mk('gear_%s_chaps_hips_%s' % (name_prefix, g), J['torso'])
        bc.chaps_hips(m, g)
        m.finish(OUT_CHAR)
    for side in 'lr':
        m = mk('gear_%s_body_arm_%s' % (name_prefix, side), J['arm_' + side])
        bc.leather_arm(m, side)
        if studded:
            sx = 1 if side == 'l' else -1
            for y in (1.36, 1.31):
                m.ellipsoid((sx * 0.235 + sx * 0.062, y, 0.0), (0.008, 0.01, 0.01), 'steel', rows=3, cols=5)
        m.finish(OUT_CHAR)
        m = mk('gear_%s_chaps_leg_%s' % (name_prefix, side), J['leg_' + side])
        bc.chaps_leg(m, side)
        if studded:
            sx = 1 if side == 'l' else -1
            for y in (0.9, 0.78, 0.66, 0.46, 0.34):
                m.ellipsoid((sx * 0.10, y, 0.088), (0.011, 0.011, 0.008), 'steel', rows=3, cols=5)
        m.finish(OUT_CHAR)


def build():
    knife(); spade(); candle(); key(); book(); studs(); bowstring(); bow_unstrung()
    flour(); jug(); milk(); cheese(); tomato(); potato(); apple(); onion(); cabbage(); berries(); dough()
    pie('drop_pie', False); pie('drop_pie_raw', True); pie('drop_pie_burnt', False, burnt=True)
    stew('drop_stew', False); stew('drop_stew_raw', True)
    cake('drop_cake', False); cake('drop_cake_raw', True)
    pizza('drop_pizza', False); pizza('drop_pizza_raw', True)
    # logs of the new trees: the same pile as the other logs, in their own colours
    bc.PAL.update({k: PAL[k] for k in ('maple', 'maple_cut', 'teak', 'teak_cut', 'mahogany', 'mahogany_cut', 'magic', 'magic_cut')})
    for kind in ('maple', 'teak', 'mahogany', 'magic'):
        bc.logs('drop_logs_' + kind, kind, kind + '_cut')
    leather_variant('hard', False)
    leather_variant('studded', True)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    build()
    print('workshop built')
