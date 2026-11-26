#!/usr/bin/env python3
"""
Journey - archery and magic models (Blender / bpy)

  assets/models/character/held_<bow>.glb            a bow in the hero's LEFT hand  (6: shortbow, longbow, oak_*, yew_*)
  assets/models/character/held_staff[_air|_water|_earth|_fire].glb      a staff in the right fist
  assets/models/character/gear_wizard_hat.glb       worn on the head
  assets/models/character/gear_wizard_robe_torso_<m|f>.glb, gear_wizard_robe_arm_<l|r>.glb      the robe top
  assets/models/character/gear_wizard_skirt_hips_<m|f>.glb, gear_wizard_skirt_leg_<l|r>.glb     the robe skirt
  assets/models/props/drop_<bow>.glb, drop_<metal>_arrows.glb, drop_rune_<element>.glb, drop_staff*.glb, drop_wizard_*.glb

Item space is the same as build_gear.py: y up, +z = forward (the way he faces), +x = his left, origin = the middle of the hand.
A bow is held upright: its limbs run along y, its belly (the grip) points forward (+z) and the string, with the tips bent back
towards the archer, is at -z. A staff runs along y with its orb above the hand and the foot of the staff at the ground.

Run:  blender -b --python tools/build_arcana.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_gear import Model, TIERS, J, TORSO, ring, loft, lying, OUT_CHAR, OUT_PROPS

WOODS = {'': '#b98a54', 'oak_': '#8c5a2e', 'yew_': '#9a3d28'}
STRING = '#ece4c8'
CLOTH, CLOTH_DARK, CLOTH_TRIM = '#3652b8', '#26397f', '#e3c36b'
ORBS = {'': None, '_air': '#e8f2f6', '_water': '#3a7cff', '_earth': '#7c5a2c', '_fire': '#ff6a1a'}
RUNES = {'air': '#dfe9ef', 'water': '#3d7bff', 'earth': '#8a6a34', 'fire': '#ff6420', 'mind': '#e070d0', 'chaos': '#c0202c'}


def M(name, origin=(0.0, 0.0, 0.0)):
    return Model(name, 'bronze', origin)


# ------------------------------------------------------------------------------------------------ bows
def bow(m, h, d, wood):
    """Limbs along y (half length h), bent back by d at the tips; a leather grip in the middle; the string at the tips."""
    n = 8
    pts = []
    for i in range(n + 1):
        y = -h + 2.0 * h * i / n
        pts.append((0.0, y, -d * (y / h) ** 2))
    for a, b in zip(pts, pts[1:]):
        r0 = 0.026 - 0.015 * abs(a[1]) / h
        r1 = 0.026 - 0.015 * abs(b[1]) / h
        m.rod(a, b, r0, r1, wood, n=6)
    m.rod((0, -0.075, 0.0), (0, 0.075, 0.0), 0.034, 0.034, 'leather', n=8)
    for s in (-1, 1):
        m.ellipsoid((0, s * h, -d), (0.02, 0.026, 0.02), 'wood_dark', rows=3, cols=6)
    m.rod((0, -h, -d), (0, h, -d), 0.004, 0.004, STRING, n=4)


BOWS = {'shortbow': (0.50, 0.15, ''), 'longbow': (0.72, 0.20, ''),
        'oak_shortbow': (0.50, 0.15, 'oak_'), 'oak_longbow': (0.72, 0.20, 'oak_'),
        'yew_shortbow': (0.50, 0.16, 'yew_'), 'yew_longbow': (0.72, 0.21, 'yew_')}


# ------------------------------------------------------------------------------------------------ staffs
def staff(m, orb):
    m.rod((0, -0.86, 0), (0, 0.64, 0), 0.022, 0.027, 'wood', n=8)
    m.rod((0, 0.09, 0), (0, -0.13, 0), 0.031, 0.031, 'leather', n=8)                  # wrapped grip
    m.rod((0, -0.86, 0), (0, -0.80, 0), 0.03, 0.026, 'metal_dark', n=8)               # iron foot
    for k in range(3):                                                                # the cradle round the orb
        a = 2 * math.pi * k / 3 + 0.4
        m.rod((0, 0.62, 0), (math.cos(a) * 0.075, 0.80, math.sin(a) * 0.075), 0.017, 0.010, 'wood_dark', n=6)
    if orb:
        m.ellipsoid((0, 0.755, 0), (0.07, 0.07, 0.07), orb, rows=4, cols=8)
        m.ellipsoid((0, 0.78, 0.0), (0.03, 0.03, 0.03), '#ffffff', rows=3, cols=6, shade=1.0)
    else:
        m.ellipsoid((0, 0.72, 0), (0.05, 0.06, 0.05), 'wood_dark', rows=3, cols=7)


# ------------------------------------------------------------------------------------------------ wizard robes
def wiz_hat(m):
    y0 = 1.665
    m.hull(ring(y0, .225, .225, 0.0, n=14) + ring(y0 + 0.035, .205, .205, 0.0, n=14), CLOTH_DARK)       # the brim
    rings = [(y0 + 0.035, .128, .128, 0.0), (y0 + 0.14, .104, .104, -0.015), (y0 + 0.26, .074, .074, -0.045),
             (y0 + 0.36, .044, .044, -0.09), (y0 + 0.44, .014, .014, -0.17)]
    loft(m, [ring(y, rx, rz, cz, n=12) for (y, rx, rz, cz) in rings], CLOTH)
    m.hull(ring(y0 + 0.045, .134, .134, 0.0, n=12) + ring(y0 + 0.085, .124, .124, 0.0, n=12), CLOTH_TRIM)   # the band
    m.ellipsoid((0.0, y0 + 0.07, 0.13), (0.03, 0.03, 0.012), CLOTH_TRIM, rows=3, cols=6)                  # a buckle


def robe_torso(m, g):
    t = TORSO[g]
    loft(m, [ring(y, rx + 0.03, rz + 0.028, cz, n=12) for (y, rx, rz, cz) in t if y >= 0.86], CLOTH)
    m.hull(ring(0.99, .212, .148, 0, n=12) + ring(1.045, .196, .136, 0, n=12), CLOTH_TRIM)               # a gold sash
    m.hull(ring(1.45, .112, .102, 0, n=10) + ring(1.50, .078, .074, 0, n=10), CLOTH_DARK)                  # collar
    zf = max(r[2] + r[3] for r in t if 1.07 <= r[0] <= 1.46) + 0.03
    m.hull([(sx * 0.014, y, z) for sx in (-1, 1) for (y, z) in ((1.06, zf - 0.01), (1.42, zf + 0.004), (1.24, zf + 0.012))], CLOTH_TRIM)   # trim down the front


def robe_arm(m, side):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.235
    m.ellipsoid((cx + sx * 0.012, 1.455, 0.0), (0.094, 0.078, 0.094), CLOTH, rows=4, cols=9)
    m.rod((cx, 1.42, 0.004), (cx, 0.98, 0.02), 0.074, 0.098, CLOTH, n=10)                                # a long, flared sleeve
    m.rod((cx, 1.0, 0.02), (cx, 0.955, 0.02), 0.099, 0.099, CLOTH_TRIM, n=10)                           # gold cuff


def skirt_hips(m, g):
    rings = [(1.03, .205, .142), (0.82, .25, .185), (0.62, .28, .215), (0.46, .30, .24)]
    loft(m, [ring(y, rx, rz, 0.0, n=14) for (y, rx, rz) in rings], CLOTH)
    m.hull(ring(0.99, .212, .148, 0, n=12) + ring(1.04, .2, .14, 0, n=12), CLOTH_TRIM)
    m.hull(ring(0.50, .304, .244, 0, n=14) + ring(0.43, .30, .24, 0, n=14), CLOTH_TRIM)                   # a gold hem


def skirt_leg(m, side):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    m.rod((cx, 0.44, 0.0), (cx, 0.15, 0.0), 0.074, 0.060, CLOTH_DARK, n=10)                              # trousers under the hem


# ------------------------------------------------------------------------------------------------ things on the ground
def lying_bow(m, h, d, wood):
    # limbs along x, belly towards +z, flat on the ground
    m.xf = lambda p: (p[1], p[0] + 0.03, p[2] - 0.0)
    bow(m, h, d, wood)


def arrow_bundle(m, metal):
    pal = TIERS[metal]
    for k in range(5):
        oz = (k - 2) * 0.045
        ang = (k - 2) * 0.10
        x0 = -0.22
        ex, ez = x0 + math.cos(ang) * 0.44, oz + math.sin(ang) * 0.44
        sx, sz = x0, oz
        m.rod((sx, 0.022 + 0.004 * (k % 2), sz), (ex, 0.022 + 0.004 * (k % 2), ez), 0.008, 0.008, 'wood', n=5)
        tx, tz = ex + math.cos(ang) * 0.075, ez + math.sin(ang) * 0.075
        m.rod((ex, 0.022 + 0.004 * (k % 2), ez), (tx, 0.022 + 0.004 * (k % 2), tz), 0.022, 0.002, 'metal_light', n=5)
        m.hull([(sx - 0.02, 0.02 + 0.004 * (k % 2), sz - 0.012), (sx - 0.02, 0.02 + 0.004 * (k % 2), sz + 0.012),
                (sx + 0.09, 0.02 + 0.004 * (k % 2), sz - 0.02), (sx + 0.09, 0.02 + 0.004 * (k % 2), sz + 0.02)], '#e8e8e0')
    m.rod((-0.1, 0.03, -0.12), (-0.1, 0.03, 0.12), 0.012, 0.012, 'leather', n=5)                         # a tie round the bundle


def rune_stone(m, element, col):
    m.rod((0, 0.0, 0), (0, 0.035, 0), 0.075, 0.07, '#7d808c', n=9)
    m.rod((0, 0.035, 0), (0, 0.045, 0), 0.062, 0.058, '#9a9da8', n=9)
    y = 0.05
    if element == 'air':
        for k in range(3):
            a = k * 2.1
            m.box((math.cos(a) * 0.03, y, math.sin(a) * 0.03), (0.026, 0.006, 0.006), col)
    elif element == 'water':
        m.ellipsoid((0, y + 0.012, 0), (0.03, 0.02, 0.03), col, rows=3, cols=7)
        m.hull([(0, y, -0.04), (0.012, y, -0.015), (-0.012, y, -0.015), (0, y + 0.01, -0.02)], col)
    elif element == 'earth':
        m.hull([(0, y, -0.04), (0.04, y, 0), (0, y, 0.04), (-0.04, y, 0), (0, y + 0.012, 0)], col)
    elif element == 'fire':
        m.hull([(0.03, y, 0.02), (-0.03, y, 0.02), (0, y, -0.04), (0, y + 0.018, 0)], col)
        m.hull([(0.012, y + 0.006, 0.0), (-0.012, y + 0.006, 0.0), (0, y + 0.006, -0.028), (0, y + 0.022, -0.004)], '#ffd060')
    elif element == 'mind':
        m.ellipsoid((0, y + 0.012, 0), (0.038, 0.016, 0.026), col, rows=3, cols=8)
    else:   # chaos: a cross
        m.box((0, y, 0), (0.036, 0.006, 0.008), col)
        m.box((0, y, 0), (0.008, 0.006, 0.036), col)


def build():
    # ---- held bows
    for name, (h, d, wood) in BOWS.items():
        m = M('held_' + name)
        bow(m, h, d, WOODS[wood])
        m.finish(OUT_CHAR)
        m = M('drop_' + name)
        m.scale = 0.8
        lying_bow(m, h, d, WOODS[wood])
        m.finish(OUT_PROPS)
    # ---- arrows on the ground
    for metal in TIERS:
        m = Model('drop_%s_arrows' % metal, metal)
        arrow_bundle(m, metal)
        m.finish(OUT_PROPS)
    # ---- staffs
    for suffix, orb in ORBS.items():
        m = M('held_staff' + suffix)
        staff(m, orb)
        m.finish(OUT_CHAR)
        m = M('drop_staff' + suffix)
        m.scale = 0.5
        m.xf = lying(0.0, 0.03)
        staff(m, orb)
        m.finish(OUT_PROPS)
    # ---- runes
    for element, col in RUNES.items():
        m = M('drop_rune_' + element)
        rune_stone(m, element, col)
        m.finish(OUT_PROPS)
    # ---- wizard robes: worn
    m = Model('gear_wizard_hat', 'bronze', J['head'])
    wiz_hat(m)
    m.finish(OUT_CHAR)
    for g in 'mf':
        m = Model('gear_wizard_robe_torso_%s' % g, 'bronze', J['torso'])
        robe_torso(m, g)
        m.finish(OUT_CHAR)
        m = Model('gear_wizard_skirt_hips_%s' % g, 'bronze', J['torso'])
        skirt_hips(m, g)
        m.finish(OUT_CHAR)
    for side in 'lr':
        m = Model('gear_wizard_robe_arm_%s' % side, 'bronze', J['arm_' + side])
        robe_arm(m, side)
        m.finish(OUT_CHAR)
        m = Model('gear_wizard_skirt_leg_%s' % side, 'bronze', J['leg_' + side])
        skirt_leg(m, side)
        m.finish(OUT_CHAR)
    # ---- wizard robes: on the ground
    m = M('drop_wizard_hat')
    m.xf = lambda p: (p[0], p[1] - 1.665 + 0.0, p[2])
    m.scale = 0.9
    wiz_hat(m)
    m.finish(OUT_PROPS)
    m = M('drop_wizard_robe')
    m.scale = 0.8
    m.xf = lambda p: (p[0], p[2] + 0.13, -(p[1] - 1.2))
    robe_torso(m, 'm')
    m.finish(OUT_PROPS)
    m = M('drop_wizard_skirt')
    m.scale = 0.8
    m.xf = lambda p: (p[0], p[2] + 0.12, -(p[1] - 0.7))
    skirt_hips(m, 'm')
    m.finish(OUT_PROPS)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    build()
    print('arcana built')
