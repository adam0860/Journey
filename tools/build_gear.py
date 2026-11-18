#!/usr/bin/env python3
"""
Journey - smithed weapons, shields, pickaxes and armour, in four metals (Blender / bpy)

For each metal (bronze, iron, steel, mithril) this builds small flat-coloured models:

  assets/models/character/held_<metal>_<dagger|sword|axe|mace|pickaxe|shield>.glb   in the hero's hand (dagger..pickaxe: right fist, shield: left forearm)
  assets/models/character/gear_<metal>_helm.glb                                      worn on the head
  assets/models/character/gear_<metal>_body_torso_<m|f>.glb, gear_<metal>_body_arm_<l|r>.glb     platebody + shoulder/arm plates
  assets/models/character/gear_<metal>_legs_hips_<m|f>.glb, gear_<metal>_legs_leg_<l|r>.glb     platelegs
  assets/models/props/drop_<metal>_<dagger|sword|axe|mace|helm|shield|legs|body|pickaxe>.glb    lying on the ground

Item space (held tools): same axes as the character - y up, +z = forward, +x = the hero's left. The origin is the
middle of the fist; with the arm hanging down a handle runs along -y, so the blade is below the hand. Blades are thin in x
(their broad faces point sideways) and wide in z, which is the direction of the swing - so the edge leads the strike.
Worn parts are in "pivot space" (origin = the joint they swing about), exactly like the clothes in build_character.py.

Run:  blender -b --python tools/build_gear.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model as _Model

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_CHAR = os.path.join(ROOT, 'assets', 'models', 'character')
OUT_PROPS = os.path.join(ROOT, 'assets', 'models', 'props')

BASE = {'wood': '#9c6c3e', 'wood_dark': '#5e3d1f', 'leather': '#5b3a20', 'leather_dark': '#3e2715', 'gold': '#d6ac45'}
TIERS = {
    'bronze':  {'metal': '#b8733a', 'metal_dark': '#8a5126', 'metal_light': '#e2a869', 'trim': '#5b3a20'},
    'iron':    {'metal': '#7e828c', 'metal_dark': '#585b64', 'metal_light': '#a9aeb9', 'trim': '#2f3238'},
    'steel':   {'metal': '#b4bcc8', 'metal_dark': '#858d9a', 'metal_light': '#e4eaf3', 'trim': '#4a505c'},
    'mithril': {'metal': '#4a6fd8', 'metal_dark': '#2f4ca6', 'metal_light': '#93b4ff', 'trim': '#1f2f73'},
}

# the character's skeleton (same numbers as build_character.py / catalog.txt)
J = {'torso': (0.0, 0.92, 0.0), 'head': (0.0, 1.50, 0.0), 'arm_l': (0.235, 1.44, 0.0), 'arm_r': (-0.235, 1.44, 0.0),
     'leg_l': (0.10, 0.92, 0.0), 'leg_r': (-0.10, 0.92, 0.0)}
# body rings: (y, half-width, half-depth, centre-z) - the same table the bare torso is lofted from
TORSO = {
    'm': [(0.86, .180, .108, 0), (0.92, .188, .114, 0), (1.00, .172, .108, 0), (1.08, .158, .102, 0), (1.20, .172, .108, 0),
          (1.32, .192, .118, .004), (1.40, .205, .112, 0), (1.455, .170, .095, 0), (1.49, .085, .075, 0)],
    'f': [(0.86, .185, .112, 0), (0.92, .200, .120, 0), (1.00, .168, .108, 0), (1.08, .130, .092, 0), (1.20, .140, .098, .002),
          (1.28, .152, .118, .012), (1.34, .160, .116, .010), (1.40, .182, .104, 0), (1.455, .150, .088, 0), (1.49, .080, .070, 0)],
}
LEG = [(0.97, .086), (0.92, .094), (0.80, .092), (0.60, .076), (0.50, .068), (0.34, .066), (0.14, .042), (0.08, .040)]


def Model(name, tier, origin=(0.0, 0.0, 0.0)):
    pal = dict(BASE)
    pal.update(TIERS[tier])
    return _Model(name, origin=origin, palette=pal)


def ring(y, rx, rz, cz=0.0, cx=0.0, n=12):
    return [(cx + rx * math.cos(2 * math.pi * i / n), y, cz + rz * math.sin(2 * math.pi * i / n)) for i in range(n)]


def loft(m, rings, mat, shade=1.0):
    """A solid made of one convex slice between each pair of neighbouring rings."""
    for a, b in zip(rings, rings[1:]):
        m.hull(a + b, mat, shade)


# ------------------------------------------------------------------------------------------------ weapons
def dagger(m):
    m.rod((0, 0.075, 0), (0, -0.075, 0), 0.017, 0.017, 'leather', n=8)
    m.ellipsoid((0, 0.092, 0), (0.024, 0.02, 0.024), 'metal_dark', rows=3, cols=8)
    m.box((0, -0.088, 0), (0.012, 0.012, 0.056), 'metal_dark')
    m.hull([(sx * 0.007, -0.10, sz * 0.030) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.005, -0.24, sz * 0.021) for sx in (-1, 1) for sz in (-1, 1)] + [(0, -0.345, 0)], 'metal')
    for sx in (-1, 1):
        m.box((sx * 0.0075, -0.19, 0), (0.0012, 0.08, 0.006), 'metal_light')


def sword(m):
    m.rod((0, 0.075, 0), (0, -0.105, 0), 0.019, 0.019, 'leather', n=8)
    m.ellipsoid((0, 0.095, 0), (0.027, 0.022, 0.027), 'metal_dark', rows=3, cols=8)
    m.box((0, -0.118, 0), (0.015, 0.014, 0.098), 'metal_dark')
    m.ellipsoid((0, -0.118, 0), (0.026, 0.026, 0.03), 'metal_light', rows=3, cols=8)
    m.hull([(sx * 0.008, -0.13, sz * 0.034) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.006, -0.58, sz * 0.029) for sx in (-1, 1) for sz in (-1, 1)] +
           [(sx * 0.004, -0.71, sz * 0.014) for sx in (-1, 1) for sz in (-1, 1)] + [(0, -0.78, 0)], 'metal')
    for sx in (-1, 1):
        m.box((sx * 0.0085, -0.37, 0), (0.0012, 0.22, 0.007), 'metal_light')


def battleaxe(m):
    m.rod((0, 0.10, 0), (0, -0.80, 0), 0.022, 0.019, 'wood', n=8)
    m.rod((0, 0.075, 0), (0, -0.15, 0), 0.026, 0.026, 'leather', n=8)                 # wrapped grip
    m.ellipsoid((0, 0.105, 0), (0.03, 0.026, 0.03), 'metal_dark', rows=3, cols=8)
    m.rod((0, -0.50, 0), (0, -0.78, 0), 0.034, 0.034, 'metal_dark', n=8)               # socket the blades sit on
    for sz in (-1, 1):
        m.hull([(sx * 0.018, y, sz * 0.03) for sx in (-1, 1) for y in (-0.545, -0.735)] +
               [(sx * 0.005, y, sz * 0.205) for sx in (-1, 1) for y in (-0.44, -0.84)], 'metal')
        m.box((0, -0.64, sz * 0.205), (0.0045, 0.20, 0.006), 'metal_light')            # the bright cutting edge
    m.rod((0, -0.49, 0), (0, -0.40, 0), 0.026, 0.004, 'metal_dark', n=6)               # spike on top


def mace(m):
    m.rod((0, 0.09, 0), (0, -0.50, 0), 0.021, 0.019, 'wood', n=8)
    m.rod((0, 0.07, 0), (0, -0.14, 0), 0.026, 0.026, 'leather', n=8)
    m.ellipsoid((0, 0.10, 0), (0.028, 0.024, 0.028), 'metal_dark', rows=3, cols=8)
    m.ellipsoid((0, -0.58, 0), (0.058, 0.08, 0.058), 'metal', rows=4, cols=8)
    for k in range(6):                                                                  # six flanges round the head
        a = 2 * math.pi * k / 6
        rx, rz, tx, tz = math.cos(a), math.sin(a), -math.sin(a), math.cos(a)
        pts = []
        for t, r0, r1 in ((-0.012, 0.04, 0.115), (0.012, 0.04, 0.115)):
            for r, y0, y1 in ((r0, -0.64, -0.52), (r1, -0.625, -0.545)):
                for y in (y0, y1):
                    pts.append((rx * r + tx * t * (1.0 if r == r0 else 0.6), y, rz * r + tz * t * (1.0 if r == r0 else 0.6)))
        m.hull(pts, 'metal_dark' if k % 2 else 'metal')
    m.rod((0, -0.64, 0), (0, -0.74, 0), 0.034, 0.004, 'metal_light', n=6)              # spike below
    m.rod((0, -0.50, 0), (0, -0.46, 0), 0.04, 0.03, 'metal_dark', n=8)


def pickaxe(m):
    m.rod((0, 0.10, 0), (0, -0.72, 0), 0.021, 0.018, 'wood', n=8)
    m.rod((0, 0.075, 0), (0, -0.15, 0), 0.025, 0.025, 'leather', n=8)
    m.ellipsoid((0, 0.105, 0), (0.028, 0.024, 0.028), 'wood_dark', rows=3, cols=8)
    m.box((0, -0.68, 0), (0.028, 0.036, 0.05), 'metal_dark')                           # the eye the haft goes through
    for sz in (-1, 1):                                                                  # two curved points, tips swung down
        m.hull([(sx * 0.015, y, sz * 0.045) for sx in (-1, 1) for y in (-0.655, -0.705)] +
               [(sx * 0.010, y, sz * 0.19) for sx in (-1, 1) for y in (-0.665, -0.715)], 'metal')
        m.hull([(sx * 0.010, y, sz * 0.18) for sx in (-1, 1) for y in (-0.665, -0.715)] +
               [(0, -0.765, sz * 0.285)], 'metal')
        m.hull([(0.0035, -0.755, sz * 0.27), (-0.0035, -0.755, sz * 0.27), (0, -0.768, sz * 0.288), (0, -0.74, sz * 0.26)], 'metal_light')


def shield(m):
    """A kite shield strapped to the left forearm. The plate faces +x (outwards); the game turns it to face the enemy."""
    x0 = 0.050
    kite = [(x0, 0.20, -0.20), (x0, 0.20, 0.20), (x0, -0.06, -0.215), (x0, -0.06, 0.215), (x0, -0.52, 0)]
    m.hull([(x0 - 0.012, y, z * 1.07) for (_, y, z) in kite] + [(x0 + 0.030, y * 0.97, z * 0.92) for (_, y, z) in kite], 'metal_dark')   # rim
    m.hull([(x0 + 0.020, y * 0.96, z * 0.95) for (_, y, z) in kite] + [(x0 + 0.036, y * 0.93 + 0.002, z * 0.88) for (_, y, z) in kite], 'metal')
    m.ellipsoid((x0 + 0.050, -0.07, 0), (0.025, 0.055, 0.055), 'metal_light', rows=3, cols=8)    # boss
    m.box((x0 + 0.038, -0.12, 0), (0.004, 0.30, 0.014), 'trim')                                  # cross
    m.box((x0 + 0.038, -0.03, 0), (0.004, 0.014, 0.15), 'trim')
    m.box((0.03, 0.0, 0), (0.014, 0.05, 0.05), 'leather')                                        # straps to the arm
    m.box((0.03, -0.20, 0), (0.014, 0.03, 0.05), 'leather')


# ------------------------------------------------------------------------------------------------ worn armour
def helm(m):
    """Medium helm: a dome over the top, a back plate, cheek guards and a nose guard (the face stays open)."""
    top = [(1.69, .124, .132, 0.004), (1.72, .124, .132, 0.0), (1.76, .098, .105, -0.002), (1.795, .052, .058, -0.004), (1.815, .018, .02, -0.004)]
    loft(m, [ring(y, rx, rz, cz, n=14) for (y, rx, rz, cz) in top], 'metal')
    m.hull(ring(1.69, .134, .142, 0.004, n=14) + ring(1.715, .126, .134, 0.0, n=14), 'metal_dark')   # the brow band
    # back and side plate: short slices round the back of the head, down to the neck
    n = 9
    for i in range(n):
        a0 = math.radians(78 + i * (204.0 / n))
        a1 = math.radians(78 + (i + 1) * (204.0 / n))
        pts = []
        for a in (a0, a1):
            for y, rx, rz in ((1.695, .131, .140), (1.585, .118, .126)):
                for k in (0.0, -0.016):
                    pts.append(((rx + k) * math.sin(a), y, 0.0 + (rz + k) * math.cos(a)))
        m.hull(pts, 'metal_dark' if i % 2 else 'metal')
    for sx in (-1, 1):                                                                              # cheek guards in front of the ears
        m.hull([(sx * 0.118, 1.69, 0.06), (sx * 0.125, 1.69, 0.10), (sx * 0.106, 1.60, 0.03), (sx * 0.112, 1.60, 0.075),
                (sx * 0.128, 1.69, 0.06), (sx * 0.114, 1.60, 0.03)], 'metal')
    m.box((0, 1.665, 0.136), (0.012, 0.055, 0.008), 'metal_light')                                  # nose guard
    m.hull([(sx * 0.006, y, z) for sx in (-1, 1) for (y, z) in ((1.80, 0.05), (1.835, 0.0), (1.80, -0.075))] +
           [(sx * 0.006, 1.74, 0.13) for sx in (-1, 1)], 'trim')                                      # crest ridge
    m.ellipsoid((0, 1.715, 0.138), (0.016, 0.016, 0.01), 'metal_light', rows=3, cols=6)             # brow stud


def body_torso(m, g):
    t = [r for r in TORSO[g]]
    g1, g2 = 0.026, 0.024
    chest = [r for r in t if 1.07 <= r[0] <= 1.46]
    waist = [r for r in t if 0.89 <= r[0] <= 1.09]
    loft(m, [ring(y, rx + g1, rz + g2, cz, n=12) for (y, rx, rz, cz) in waist], 'metal_dark')
    loft(m, [ring(y, rx + g1, rz + g2, cz, n=12) for (y, rx, rz, cz) in chest], 'metal')
    # belt, collar and the bright keel down the front
    m.hull(ring(0.915, .202, .136, 0, n=12) + ring(1.0, .192, .124, 0, n=12), 'trim')
    m.hull(ring(1.45, .108, .098, 0, n=10) + ring(1.505, .072, .07, 0, n=10), 'metal_dark')
    zf = max(r[2] + r[3] for r in chest) + 0.02
    m.hull([(sx * 0.012, y, z) for sx in (-1, 1) for (y, z) in ((1.10, zf - 0.012), (1.42, zf + 0.004), (1.26, zf + 0.018))], 'metal_light')
    for sx in (-1, 1):                                                                              # rivets
        for y in (1.16, 1.30, 1.40):
            m.ellipsoid((sx * 0.07, y, zf - 0.005), (0.009, 0.009, 0.007), 'metal_light', rows=3, cols=5)


def body_arm(m, side):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.235
    m.ellipsoid((cx + sx * 0.012, 1.455, 0.0), (0.092, 0.075, 0.092), 'metal', rows=4, cols=9)       # pauldron
    m.hull(ring(1.40, 0.094, 0.094, 0.0, cx + sx * 0.012, 10) + ring(1.385, 0.088, 0.088, 0.0, cx + sx * 0.008, 10), 'metal_dark')
    m.rod((cx, 1.40, 0.004), (cx, 1.20, 0.012), 0.066, 0.058, 'metal', n=10)                        # upper-arm plate
    m.rod((cx, 1.215, 0.012), (cx, 1.175, 0.012), 0.066, 0.066, 'metal_dark', n=10)                 # cuff


def legs_hips(m, g):
    m.hull(ring(0.915, .204, .138, 0, n=12) + ring(1.02, .194, .128, 0, n=12), 'metal_dark')          # waist band
    for sx in (-1, 1):                                                                              # hip guards
        m.hull([(sx * 0.20, 1.0, -0.075), (sx * 0.20, 1.0, 0.075), (sx * 0.225, 0.80, -0.06), (sx * 0.225, 0.80, 0.07),
                (sx * 0.17, 1.0, 0.1), (sx * 0.19, 0.82, 0.10)], 'metal')
    m.hull([(sx * 0.115, 1.0, 0.125) for sx in (-1, 1)] + [(sx * 0.095, 0.80, 0.138) for sx in (-1, 1)] +
           [(sx * 0.115, 1.0, 0.105) for sx in (-1, 1)] + [(sx * 0.09, 0.80, 0.115) for sx in (-1, 1)], 'metal_light')   # front fauld
    m.hull([(sx * 0.12, 1.0, -0.12) for sx in (-1, 1)] + [(sx * 0.1, 0.80, -0.135) for sx in (-1, 1)] +
           [(sx * 0.12, 1.0, -0.10) for sx in (-1, 1)] + [(sx * 0.09, 0.80, -0.11) for sx in (-1, 1)], 'metal')          # back fauld


def legs_leg(m, side):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    m.rod((cx, 0.985, 0.0), (cx, 0.54, 0.0), 0.112, 0.088, 'metal', n=10)                           # thigh plate
    m.ellipsoid((cx, 0.505, 0.022), (0.078, 0.062, 0.074), 'metal_dark', rows=4, cols=8)            # knee cop
    m.rod((cx, 0.46, 0.0), (cx, 0.15, 0.0), 0.086, 0.062, 'metal', n=10)                            # greave
    m.rod((cx, 0.16, 0.0), (cx, 0.125, 0.0), 0.066, 0.066, 'metal_dark', n=10)
    m.hull([(cx + k, 0.92, 0.088) for k in (-0.012, 0.012)] + [(cx + k, 0.55, 0.095) for k in (-0.012, 0.012)] +
           [(cx + k, 0.92, 0.09) for k in (-0.008, 0.008)], 'metal_light')                          # shin-front highlight on the thigh


# ------------------------------------------------------------------------------------------------ exporting
def lying(shift_x, shift_y=0.03):
    """item space (blade along -y, thin in x) -> lying flat on the ground (blade along +x)."""
    return lambda p: (-p[1] - shift_x, p[0] + shift_y, p[2])


WEAPONS = {'dagger': (dagger, 0.13), 'sword': (sword, 0.33), 'axe': (battleaxe, 0.36), 'mace': (mace, 0.26), 'pickaxe': (pickaxe, 0.31)}


def build_tier(tier):
    # ---- held in the hand
    for key, (fn, _) in WEAPONS.items():
        m = Model('held_%s_%s' % (tier, key), tier)
        fn(m)
        m.finish(OUT_CHAR)
    m = Model('held_%s_shield' % tier, tier)
    shield(m)
    m.finish(OUT_CHAR)
    # ---- worn
    m = Model('gear_%s_helm' % tier, tier, J['head'])
    helm(m)
    m.finish(OUT_CHAR)
    for g in 'mf':
        m = Model('gear_%s_body_torso_%s' % (tier, g), tier, J['torso'])
        body_torso(m, g)
        m.finish(OUT_CHAR)
        m = Model('gear_%s_legs_hips_%s' % (tier, g), tier, J['torso'])
        legs_hips(m, g)
        m.finish(OUT_CHAR)
    for side in 'lr':
        m = Model('gear_%s_body_arm_%s' % (tier, side), tier, J['arm_' + side])
        body_arm(m, side)
        m.finish(OUT_CHAR)
        m = Model('gear_%s_legs_leg_%s' % (tier, side), tier, J['leg_' + side])
        legs_leg(m, side)
        m.finish(OUT_CHAR)
    # ---- lying on the ground (ground items)
    for key, (fn, shift) in WEAPONS.items():
        m = Model('drop_%s_%s' % (tier, key), tier)
        m.scale = 0.8
        m.xf = lying(shift)
        fn(m)
        m.finish(OUT_PROPS)
    m = Model('drop_%s_shield' % tier, tier)
    m.scale = 0.8
    m.xf = lambda p: (-p[1] + 0.16, p[0] - 0.03, p[2])
    shield(m)
    m.finish(OUT_PROPS)
    m = Model('drop_%s_helm' % tier, tier)
    m.scale = 1.0
    m.xf = lambda p: (p[0], p[1] - 1.60, p[2])
    helm(m)
    m.finish(OUT_PROPS)
    m = Model('drop_%s_body' % tier, tier)
    m.scale = 0.8
    m.xf = lambda p: (p[0], p[2] + 0.13, -(p[1] - 1.2))
    body_torso(m, 'm')
    m.finish(OUT_PROPS)
    m = Model('drop_%s_legs' % tier, tier)
    m.scale = 0.8
    for side in 'lr':
        sx = 1 if side == 'l' else -1
        m.xf = (lambda s_: (lambda p: (p[0] - s_ * 0.10 + s_ * 0.13, p[2] + 0.09, -(p[1] - 0.55))))(sx)
        legs_leg(m, side)
    m.finish(OUT_PROPS)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for t in TIERS:
        build_tier(t)
    print('gear built')
