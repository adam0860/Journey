#!/usr/bin/env python3
"""
Journey - models for the skilling and crafting items (Blender / bpy)

  assets/models/props/drop_<name>.glb       items lying on the ground (logs of other trees, fishing gear, gems, leather, jewellery, pots...)
  assets/models/props/<tanner|jeweller|wheel>.glb       the crafting workstations (front = +z)
  assets/models/character/held_<metal>_hatchet.glb      the woodcutter's axe in the hero's right fist (same space as build_gear.py)
  assets/models/props/drop_<metal>_hatchet.glb
  assets/models/character/gear_leather_cap.glb, gear_leather_body_*.glb, gear_leather_chaps_*.glb      leather armour worn on the body

Run:  blender -b --python tools/build_crafting.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model as _Model
from build_gear import TIERS, J, TORSO, ring, loft, lying

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_CHAR = os.path.join(ROOT, 'assets', 'models', 'character')
OUT_PROPS = os.path.join(ROOT, 'assets', 'models', 'props')

PAL = {
    'wood': '#9c6c3e', 'wood_dark': '#5e3d1f', 'trunk': '#6b4527', 'oak': '#8c5f33', 'willow': '#6e7440', 'yew': '#7a3a2c',
    'oak_cut': '#e2bc82', 'willow_cut': '#cfcd80', 'yew_cut': '#c4806a', 'leather': '#a06a3c', 'leather_dark': '#6f4524', 'leather_light': '#c58d58',
    'iron': '#4a4c54', 'steel': '#b9c0cb', 'rope': '#c9b27a', 'wicker': '#b08a4e', 'wicker_dark': '#7a5a2c', 'feather': '#eee9dc', 'feather_b': '#c97a4c',
    'clay': '#a8805f', 'clay_dark': '#8a6446', 'grey': '#8b8d95', 'grey_dark': '#5e6068', 'gold': '#e6b73a', 'gold_light': '#ffd966', 'silver': '#cfd5e0',
    'sapphire': '#3b66e8', 'sapphire_light': '#92b0ff', 'emerald': '#27b862', 'emerald_light': '#8af0b4', 'ruby': '#e02a42', 'ruby_light': '#ff8e9a', 'diamond': '#dff2ff', 'diamond_light': '#ffffff', 'dragonstone': '#8a3ad6', 'dragonstone_light': '#d9a0ff',
    'rough': '#8d8e95', 'thread': '#d24646', 'needle': '#d9dde6', 'pot': '#bf724c', 'pot_dark': '#8e4f30', 'pot_light': '#dd9168',
    'cloth': '#7a5a3a', 'string': '#e6dec0', 'blue': '#3d5fb0', 'parchment': '#e8d9b0', 'parchment_dark': '#c8b484', 'ribbon': '#c43a3a', 'brick': '#9a5a44', 'glow': '#ff8a2a',
}


def M(name, origin=(0.0, 0.0, 0.0)):
    return _Model(name, origin=origin, palette=PAL)


def finish(m, folder=OUT_PROPS):
    m.finish(folder)


# ------------------------------------------------------------------------------------------------ logs of the bigger trees
def logs(name, bark, cut):
    m = M(name)
    for (x0, z0, y0) in ((-0.0, -0.06, 0.055), (0.0, 0.06, 0.055)):
        m.rod((-0.26, y0, z0), (0.26, y0, z0), 0.055, 0.055, bark, n=7)
        for sx in (-1, 1):
            m.rod((sx * 0.258, y0, z0), (sx * 0.262, y0, z0), 0.045, 0.045, cut, n=7)
    m.rod((-0.25, 0.150, 0.0), (0.25, 0.150, 0.0), 0.055, 0.055, bark, n=7)
    finish(m)


# ------------------------------------------------------------------------------------------------ tools
def hatchet(m):
    """Held with the arm hanging: the haft runs down -y from the fist, the blade is at the bottom and swings forward (+z)."""
    m.rod((0, 0.08, 0), (0, -0.52, 0), 0.020, 0.018, 'wood', n=8)
    m.rod((0, 0.065, 0), (0, -0.12, 0), 0.024, 0.024, 'leather_dark', n=8)
    m.ellipsoid((0, 0.09, 0), (0.026, 0.022, 0.026), 'wood_dark', rows=3, cols=7)
    m.box((0, -0.46, 0.0), (0.020, 0.07, 0.03), 'metal_dark')
    m.hull([(sx * 0.012, y, z) for sx in (-1, 1) for (y, z) in ((-0.40, 0.02), (-0.53, 0.02), (-0.36, 0.18), (-0.56, 0.17))], 'metal')   # one broad blade
    m.box((0, -0.36, 0.17), (0.005, 0.005, 0.005), 'metal_light')
    m.hull([(sx * 0.004, -0.57, 0.14) for sx in (-1, 1)] + [(sx * 0.004, -0.36, 0.185) for sx in (-1, 1)] + [(0, -0.46, 0.19)], 'metal_light')   # the bright edge


def fishing_rod():
    m = M('drop_fishing_rod')
    m.rod((-0.34, 0.03, 0.0), (0.34, 0.07, 0.02), 0.012, 0.006, 'wood', n=6)
    m.rod((-0.16, 0.04, 0.03), (-0.12, 0.04, 0.03), 0.035, 0.035, 'iron', n=8)
    m.rod((0.34, 0.07, 0.02), (0.36, 0.01, 0.05), 0.003, 0.003, 'string', n=4)
    m.ellipsoid((0.36, 0.015, 0.05), (0.02, 0.012, 0.02), 'feather_b', rows=3, cols=5)
    finish(m)


def harpoon():
    m = M('drop_harpoon')
    m.rod((-0.3, 0.03, 0.0), (0.26, 0.03, 0.0), 0.016, 0.016, 'wood', n=6)
    m.hull([(0.26, 0.03, 0.0), (0.38, 0.03, 0.0), (0.30, 0.03, 0.05), (0.30, 0.03, -0.05)], 'steel')
    m.hull([(0.26, 0.03, 0.0), (0.33, 0.07, 0.0), (0.30, 0.03, 0.0)], 'steel')
    finish(m)


def lobster_pot():
    m = M('drop_lobster_pot')
    m.rod((0, 0.0, 0), (0, 0.22, 0), 0.17, 0.15, 'wicker', n=10)
    for k in range(6):
        a = 2 * math.pi * k / 6
        m.rod((math.cos(a) * 0.16, 0.0, math.sin(a) * 0.16), (math.cos(a) * 0.14, 0.23, math.sin(a) * 0.14), 0.012, 0.012, 'wicker_dark', n=4)
    m.rod((0, 0.22, 0), (0, 0.24, 0), 0.10, 0.06, 'wicker_dark', n=8)
    finish(m)


def feathers():
    m = M('drop_feathers')
    for k, (x, a, mat) in enumerate(((-0.06, 0.2, 'feather'), (0.0, 0.0, 'feather_b'), (0.07, -0.2, 'feather'))):
        m.ellipsoid((x, 0.012 + 0.005 * k, 0.0), (0.02, 0.006, 0.13), mat, rot=(0, a, 0), rows=3, cols=6)
        m.rod((x, 0.016, -0.12), (x + 0.0, 0.016, 0.10), 0.004, 0.003, 'string', n=4)
    finish(m)


def chisel():
    m = M('drop_chisel')
    m.rod((-0.12, 0.025, 0.0), (0.04, 0.025, 0.0), 0.022, 0.018, 'wood', n=7)
    m.hull([(0.04, 0.025, -0.014), (0.04, 0.025, 0.014), (0.16, 0.02, 0.0), (0.04, 0.04, 0.0)], 'steel')
    finish(m)


def needle():
    m = M('drop_needle')
    m.rod((-0.11, 0.012, 0.0), (0.11, 0.012, 0.02), 0.005, 0.002, 'needle', n=5)
    m.rod((-0.12, 0.012, -0.002), (-0.10, 0.012, 0.0), 0.012, 0.012, 'needle', n=6)
    finish(m)


def thread():
    m = M('drop_thread')
    m.rod((0, 0.0, 0), (0, 0.075, 0), 0.05, 0.05, 'thread', n=9)
    for y0, y1 in ((0.0, 0.01), (0.065, 0.075)):
        m.rod((0, y0, 0), (0, y1, 0), 0.062, 0.062, 'wood_dark', n=9)
    finish(m)


def leather():
    m = M('drop_leather')
    m.hull([(sx * 0.17, y, sz * 0.12) for sx in (-1, 1) for sz in (-1, 1) for y in (0.0, 0.03)], 'leather')
    m.hull([(sx * 0.15, 0.03, sz * 0.10) for sx in (-1, 1) for sz in (-1, 1)] + [(sx * 0.12, 0.045, sz * 0.08) for sx in (-1, 1) for sz in (-1, 1)], 'leather_light')
    finish(m)


def mould():
    m = M('drop_mould')
    m.box((0, 0.025, 0), (0.15, 0.025, 0.11), 'grey')
    m.rod((0, 0.05, 0), (0, 0.056, 0), 0.07, 0.07, 'grey_dark', n=12)
    m.rod((0, 0.056, 0), (0, 0.058, 0), 0.034, 0.034, 'grey', n=10)
    finish(m)


def ring_gold():
    m = M('drop_ring_gold')
    for k in range(10):
        a = 2 * math.pi * k / 10
        m.ellipsoid((math.cos(a) * 0.045, 0.018, math.sin(a) * 0.045), (0.016, 0.014, 0.016), 'gold', rows=3, cols=5)
    m.ellipsoid((0.045, 0.034, 0.0), (0.02, 0.016, 0.02), 'gold_light', rows=3, cols=6)
    finish(m)


def amulet_gold():
    m = M('drop_amulet_gold')
    for k in range(14):
        a = math.pi * k / 13
        m.ellipsoid((math.cos(a) * 0.09 - 0.0, 0.01, -0.06 - math.sin(a) * 0.07), (0.01, 0.008, 0.01), 'gold', rows=3, cols=5)
    m.rod((0, 0.012, 0.05), (0, 0.03, 0.05), 0.05, 0.045, 'gold', n=10)
    m.ellipsoid((0, 0.035, 0.05), (0.03, 0.014, 0.03), 'gold_light', rows=3, cols=7)
    finish(m)


def gem(name, base, light, rough=False):
    m = M(name)
    if rough:
        m.ellipsoid((0, 0.045, 0), (0.07, 0.05, 0.06), 'rough', rows=3, cols=7)
        m.ellipsoid((0.02, 0.07, 0.0), (0.03, 0.025, 0.03), base, rows=3, cols=6)
    else:
        pts = [(math.cos(2 * math.pi * k / 6) * 0.06, 0.07, math.sin(2 * math.pi * k / 6) * 0.06) for k in range(6)]
        m.hull(pts + [(0, 0.0, 0)], base)
        m.hull(pts + [(math.cos(2 * math.pi * k / 6) * 0.035, 0.095, math.sin(2 * math.pi * k / 6) * 0.035) for k in range(6)], light)
    finish(m)


def pot(name='drop_pot'):
    m = M(name)
    m.rod((0, 0.0, 0), (0, 0.06, 0), 0.07, 0.10, 'pot', n=10)
    m.rod((0, 0.06, 0), (0, 0.12, 0), 0.10, 0.07, 'pot_light', n=10)
    m.rod((0, 0.12, 0), (0, 0.13, 0), 0.075, 0.075, 'pot_dark', n=10)
    finish(m)


def scroll():
    m = M('drop_scroll')
    m.rod((-0.12, 0.05, 0), (0.12, 0.05, 0), 0.045, 0.045, 'parchment', n=10)
    for sx in (-1, 1):
        m.rod((sx * 0.12, 0.05, 0), (sx * 0.135, 0.05, 0), 0.05, 0.05, 'parchment_dark', n=10)
    m.rod((-0.02, 0.05, 0), (0.02, 0.05, 0), 0.052, 0.052, 'ribbon', n=10)
    finish(m)


def clay():
    m = M('drop_ore_clay')
    m.ellipsoid((0, 0.06, 0), (0.13, 0.07, 0.10), 'clay', rows=3, cols=7)
    m.ellipsoid((0.1, 0.045, 0.06), (0.07, 0.045, 0.06), 'clay_dark', rows=3, cols=6)
    finish(m)


def leather_drops():
    def body_drop():
        m = M('drop_leather_body')
        m.hull([(sx * 0.17, y, sz * 0.09) for sx in (-1, 1) for sz in (-1, 1) for y in (0.0, 0.05)], 'leather')
        m.hull([(sx * 0.07, 0.05, 0.0) for sx in (-1, 1)] + [(sx * 0.17, 0.05, sz * 0.09) for sx in (-1, 1) for sz in (-1, 1)] + [(0, 0.075, 0)], 'leather_dark')
        finish(m)
    def chaps_drop():
        m = M('drop_leather_chaps')
        for sx in (-1, 1):
            m.hull([(sx * 0.03, 0.0, z) for z in (-0.12, 0.12)] + [(sx * 0.11, 0.0, z) for z in (-0.12, 0.12)] +
                   [(sx * 0.03, 0.045, z * 0.9) for z in (-0.12, 0.12)] + [(sx * 0.11, 0.045, z * 0.9) for z in (-0.12, 0.12)], 'leather')
        finish(m)
    def cap_drop():
        m = M('drop_leather_cap')
        m.ellipsoid((0, 0.07, 0), (0.11, 0.075, 0.12), 'leather', rows=4, cols=8)
        m.rod((0, 0.02, 0), (0, 0.03, 0), 0.13, 0.13, 'leather_dark', n=10)
        finish(m)
    body_drop()
    chaps_drop()
    cap_drop()


# ------------------------------------------------------------------------------------------------ stations
def tanner():
    m = M('tanner')
    for sx in (-1, 1):
        m.box((sx * 0.62, 0.55, 0), (0.05, 0.55, 0.05), 'wood_dark')
    m.box((0, 1.08, 0), (0.7, 0.04, 0.05), 'wood')
    m.box((0, 0.62, 0), (0.7, 0.03, 0.04), 'wood')
    for k, x in enumerate((-0.35, 0.0, 0.35)):        # hides hanging to dry
        m.hull([(x - 0.17, 1.05, 0.0), (x + 0.17, 1.05, 0.0), (x - 0.19, 0.30, 0.0), (x + 0.19, 0.30, 0.0), (x, 0.2, 0.0),
                (x - 0.17, 1.05, 0.04), (x + 0.17, 1.05, 0.04), (x - 0.19, 0.30, 0.04), (x + 0.19, 0.30, 0.04), (x, 0.2, 0.04)], 'leather' if k != 1 else 'leather_light')
    m.box((0.0, 0.1, 0.5), (0.3, 0.1, 0.2), 'wood_dark')                                  # a trough of tanning liquid
    m.box((0.0, 0.21, 0.5), (0.26, 0.012, 0.16), 'oak')
    finish(m)


def jeweller():
    m = M('jeweller')
    m.box((0, 0.45, 0), (0.55, 0.45, 0.3), 'wood_dark')
    m.box((0, 0.92, 0), (0.6, 0.03, 0.34), 'wood')
    m.box((-0.3, 0.97, 0.05), (0.14, 0.02, 0.11), 'grey')            # moulds
    m.box((0.05, 0.97, 0.05), (0.12, 0.02, 0.1), 'grey_dark')
    m.rod((0.3, 0.95, 0.1), (0.3, 1.0, 0.1), 0.05, 0.045, 'gold', n=8)       # a tray of gold
    m.box((0.1, 0.96, -0.12), (0.1, 0.02, 0.05), 'gold_light')
    m.ellipsoid((-0.1, 0.98, -0.1), (0.03, 0.03, 0.03), 'ruby', rows=3, cols=6)
    m.ellipsoid((0.0, 0.98, -0.1), (0.03, 0.03, 0.03), 'sapphire', rows=3, cols=6)
    m.ellipsoid((-0.2, 0.98, -0.1), (0.03, 0.03, 0.03), 'emerald', rows=3, cols=6)
    finish(m)


def cooking_range():
    """A brick range with a chimney, a glowing firebox in front (+z), a pot and a pan on the hob."""
    m = M('range')
    m.box((0, 0.38, 0), (0.48, 0.38, 0.32), 'brick')
    m.box((0, 0.79, 0), (0.50, 0.03, 0.34), 'iron')
    m.box((0, 0.30, 0.325), (0.27, 0.19, 0.006), 'iron')
    m.box((0, 0.30, 0.33), (0.21, 0.14, 0.006), 'glow')
    m.box((0, 0.30, 0.337), (0.21, 0.012, 0.004), 'iron')
    m.box((-0.3, 1.32, -0.16), (0.11, 0.5, 0.11), 'brick')
    m.box((-0.3, 1.84, -0.16), (0.15, 0.03, 0.15), 'grey_dark')
    m.rod((0.16, 0.82, 0.0), (0.16, 0.99, 0.0), 0.13, 0.11, 'iron', n=10)
    m.rod((-0.18, 0.82, 0.06), (-0.18, 0.87, 0.06), 0.16, 0.16, 'grey_dark', n=12)
    m.box((0.0, 0.62, 0.325), (0.4, 0.015, 0.008), 'iron')
    finish(m)


def wheel():
    m = M('wheel')
    m.box((0, 0.35, 0), (0.28, 0.35, 0.28), 'wood_dark')                 # the frame
    m.rod((0, 0.72, 0), (0, 0.76, 0), 0.26, 0.26, 'grey', n=14)           # the spinning table
    m.rod((0, 0.76, 0), (0, 0.80, 0), 0.07, 0.05, 'clay', n=10)           # a lump of clay on it
    m.rod((-0.34, 0.12, 0.0), (-0.34, 0.2, 0.0), 0.2, 0.2, 'wood', n=12)  # the foot wheel
    m.ellipsoid((0.38, 0.1, 0.2), (0.09, 0.1, 0.09), 'pot', rows=3, cols=7)   # finished pots beside it
    m.ellipsoid((0.38, 0.1, -0.12), (0.07, 0.07, 0.07), 'pot_light', rows=3, cols=7)
    finish(m)


# ------------------------------------------------------------------------------------------------ worn leather armour
def leather_cap(m):
    top = [(1.66, .124, .132, 0.004), (1.70, .124, .132, 0.0), (1.75, .098, .105, -0.002), (1.78, .05, .055, -0.004), (1.795, .018, .02, -0.004)]
    loft(m, [ring(y, rx, rz, cz, n=12) for (y, rx, rz, cz) in top], 'leather')
    m.hull(ring(1.655, .134, .142, 0.004, n=12) + ring(1.69, .128, .136, 0.0, n=12), 'leather_dark')
    m.ellipsoid((0, 1.70, 0.136), (0.018, 0.018, 0.01), 'gold', rows=3, cols=6)


def leather_torso(m, g):
    t = TORSO[g]
    loft(m, [ring(y, rx + 0.022, rz + 0.02, cz, n=12) for (y, rx, rz, cz) in t if 0.9 <= y <= 1.47], 'leather')
    m.hull(ring(0.97, .202, .136, 0, n=12) + ring(1.03, .188, .126, 0, n=12), 'leather_dark')                   # a belt
    m.hull(ring(1.45, .10, .092, 0, n=10) + ring(1.50, .07, .068, 0, n=10), 'leather_dark')                      # collar
    zf = max(r[2] + r[3] for r in t if 1.07 <= r[0] <= 1.46) + 0.02
    for y in (1.14, 1.26, 1.38):                                                                                  # laces down the front
        m.box((0, y, zf), (0.03, 0.006, 0.006), 'string')
    m.box((0, 1.26, zf - 0.002), (0.004, 0.14, 0.004), 'leather_dark')


def leather_arm(m, side):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.235
    m.ellipsoid((cx + sx * 0.01, 1.452, 0.0), (0.088, 0.07, 0.088), 'leather', rows=4, cols=9)
    m.rod((cx, 1.40, 0.004), (cx, 1.27, 0.008), 0.064, 0.06, 'leather', n=10)                                    # a short sleeve
    m.rod((cx, 1.28, 0.008), (cx, 1.255, 0.008), 0.064, 0.064, 'leather_dark', n=10)


def chaps_hips(m, g):
    m.hull(ring(0.915, .204, .138, 0, n=12) + ring(1.0, .192, .126, 0, n=12), 'leather_dark')
    for sx in (-1, 1):
        m.box((sx * 0.20, 0.97, 0.0), (0.02, 0.03, 0.04), 'gold')


def chaps_leg(m, side):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    m.rod((cx, 0.985, 0.0), (cx, 0.54, 0.0), 0.108, 0.084, 'leather', n=10)
    m.rod((cx, 0.54, 0.0), (cx, 0.16, 0.0), 0.084, 0.060, 'leather', n=10)
    m.rod((cx, 0.17, 0.0), (cx, 0.14, 0.0), 0.064, 0.064, 'leather_dark', n=10)
    m.box((cx, 0.6, 0.082), (0.02, 0.012, 0.006), 'string')


def build():
    # ---- logs of the other trees
    logs('drop_logs_oak', 'oak', 'oak_cut')
    logs('drop_logs_willow', 'willow', 'willow_cut')
    logs('drop_logs_yew', 'yew', 'yew_cut')
    # ---- axes: held and lying on the ground, in the four metals
    for tier in TIERS:
        pal = dict(PAL)
        pal.update(TIERS[tier])
        pal['metal_dark'] = pal.get('metal_dark', '#555')
        m = _Model('held_%s_hatchet' % tier, palette=pal)
        hatchet(m)
        m.finish(OUT_CHAR)
        m = _Model('drop_%s_hatchet' % tier, palette=pal)
        m.scale = 0.9
        m.xf = lying(0.28, 0.03)
        hatchet(m)
        m.finish(OUT_PROPS)
    # ---- fishing, crafting tools, materials
    fishing_rod()
    harpoon()
    lobster_pot()
    feathers()
    chisel()
    needle()
    thread()
    leather()
    mould()
    ring_gold()
    amulet_gold()
    pot()
    scroll()
    for name, base, light in (('gem_blue', 'sapphire', 'sapphire_light'), ('gem_green', 'emerald', 'emerald_light'), ('gem_red', 'ruby', 'ruby_light'), ('gem_white', 'diamond', 'diamond_light'), ('gem_purple', 'dragonstone', 'dragonstone_light')):
        gem('drop_' + name, base, light)
    leather_drops()
    # ---- workstations
    tanner()
    jeweller()
    wheel()
    cooking_range()
    # ---- worn leather armour
    m = _Model('gear_leather_cap', origin=J['head'], palette=PAL)
    leather_cap(m)
    m.finish(OUT_CHAR)
    for g in 'mf':
        m = _Model('gear_leather_body_torso_%s' % g, origin=J['torso'], palette=PAL)
        leather_torso(m, g)
        m.finish(OUT_CHAR)
        m = _Model('gear_leather_chaps_hips_%s' % g, origin=J['torso'], palette=PAL)
        chaps_hips(m, g)
        m.finish(OUT_CHAR)
    for side in 'lr':
        m = _Model('gear_leather_body_arm_%s' % side, origin=J['arm_' + side], palette=PAL)
        leather_arm(m, side)
        m.finish(OUT_CHAR)
        m = _Model('gear_leather_chaps_leg_%s' % side, origin=J['leg_' + side], palette=PAL)
        chaps_leg(m, side)
        m.finish(OUT_CHAR)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    build()
    print('crafting models built')
