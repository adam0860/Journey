#!/usr/bin/env python3
"""
Journey - boots, gloves and capes (Blender / bpy), round 11

  assets/models/character/gear_boots_<leather|hard|studded>_leg_<l|r>.glb      leather boots worn on the feet (pivot = the leg joint, like trousers)
  assets/models/character/gear_<metal>_boots_leg_<l|r>.glb                      smithed boots (bronze .. adamant)
  assets/models/character/gear_gloves_<leather|hard>_arm_<l|r>.glb              gloves worn on the hands (pivot = the arm joint)
  assets/models/character/gear_<metal>_gauntlets_arm_<l|r>.glb                  smithed gauntlets
  assets/models/character/gear_cape_<skill|quest>.glb                           a cape hanging from the shoulders (pivot = the torso joint)
  assets/models/props/drop_boots_<leather|hard|studded>.glb, drop_<metal>_boots.glb, drop_gloves_<leather|hard>.glb,
  drop_<metal>_gauntlets.glb, drop_cape_<key>.glb                               the same things lying on the ground

The capes' colours are the same table as CapeInfo in src/game/Gear.cpp (keep them in step: the bag icons use that one).

Run:  blender -b --python tools/build_wardrobe.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model as _Model
from build_gear import TIERS, BASE, J, OUT_CHAR, OUT_PROPS, ring

LEATHER = {
    'leather': {'metal': '#a06a3c', 'metal_dark': '#6f4524', 'metal_light': '#c58d58', 'trim': '#3e2715', 'steel': '#b9c0cb'},
    'hard': {'metal': '#6e4628', 'metal_dark': '#43281a', 'metal_light': '#8a5c36', 'trim': '#2a1a10', 'steel': '#b9c0cb'},
    'studded': {'metal': '#6e4628', 'metal_dark': '#43281a', 'metal_light': '#8a5c36', 'trim': '#2a1a10', 'steel': '#b9c0cb'},
}

# (cloth, edging) for each cape: the same colours as CapeInfo in Gear.cpp
CAPES = {
    'attack': ('#b2261e', '#e8be46'), 'strength': ('#d4741a', '#4a2a10'), 'defence': ('#3b6fd0', '#ced4e0'), 'hitpoints': ('#ece8e0', '#d02828'),
    'ranged': ('#1f5e3a', '#9c6e3c'), 'magic': ('#5a3fb0', '#e8be46'), 'prayer': ('#f2f0e0', '#e8be46'), 'mining': ('#6b6f78', '#c67c3e'),
    'smithing': ('#4a4d55', '#ec8228'), 'fishing': ('#2fa0c0', '#f0f4f8'), 'cooking': ('#8a4b2a', '#f4e8c8'), 'woodcutting': ('#5e8e34', '#603e22'),
    'firemaking': ('#d84a1a', '#fad650'), 'crafting': ('#c0a050', '#6e4828'), 'quest': ('#1c2a5c', '#e8be46'),
}


def tier_palette(tier):
    pal = dict(BASE)
    pal.update(TIERS[tier])
    pal['steel'] = '#b9c0cb'
    return pal


# ------------------------------------------------------------------------------------------------ the pieces
def boot(m, side, studs=False):
    """One boot: a shaft up the shin, a cuff, and a foot with a darker sole. Standing on the ground (y = 0), leg joint at y = 0.92."""
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    m.rod((cx, 0.37, 0.0), (cx, 0.11, 0.0), 0.076, 0.060, 'metal', n=10)                         # the shaft
    m.rod((cx, 0.375, 0.0), (cx, 0.335, 0.0), 0.080, 0.078, 'metal_dark', n=10)                  # the cuff
    m.hull([(cx + k * 0.058, 0.115, -0.066) for k in (-1, 1)] + [(cx + k * 0.056, 0.115, 0.058) for k in (-1, 1)] +
           [(cx + k * 0.052, 0.085, 0.14) for k in (-1, 1)] + [(cx + k * 0.040, 0.035, 0.188) for k in (-1, 1)] +
           [(cx + k * 0.058, 0.014, -0.072) for k in (-1, 1)] + [(cx + k * 0.046, 0.014, 0.19) for k in (-1, 1)], 'metal')   # the foot
    m.hull([(cx + k * 0.060, y, z) for k in (-1, 1) for (y, z) in ((0.0, -0.074), (0.0, 0.194), (0.020, -0.074), (0.020, 0.194))], 'trim')   # the sole
    m.box((cx, 0.075, 0.130), (0.034, 0.013, 0.016), 'metal_dark')                               # the toe cap
    if studs:
        for y in (0.30, 0.22, 0.14):
            m.ellipsoid((cx + sx * 0.0, y, 0.066), (0.010, 0.010, 0.008), 'steel', rows=3, cols=5)


def glove(m, side, gauntlet=False, studs=False):
    """A glove: a cuff on the forearm and a mitten over the hand (the hand of the body model is at y 0.865). Gauntlets flare more."""
    sx = 1 if side == 'l' else -1
    cx = sx * 0.235
    flare = 1.18 if gauntlet else 1.0
    m.rod((cx, 1.05, 0.006), (cx, 0.94, 0.012), 0.050 * flare, 0.046 * flare, 'metal_dark', n=10)       # the cuff
    m.rod((cx, 0.945, 0.012), (cx, 0.915, 0.014), 0.050 * flare, 0.046, 'metal', n=10)
    m.ellipsoid((cx, 0.862, 0.016), (0.043, 0.082, 0.034), 'metal', rows=4, cols=8)                  # the hand
    m.ellipsoid((cx - sx * 0.040, 0.875, 0.026), (0.016, 0.034, 0.018), 'metal', rows=3, cols=6)      # the thumb
    m.box((cx, 0.835, 0.046), (0.032, 0.012, 0.007), 'metal_light' if gauntlet else 'metal_dark')       # the knuckles
    if gauntlet:
        m.box((cx, 0.99, 0.0), (0.054, 0.006, 0.054), 'metal_light')                                  # a bright band on the cuff
    if studs:
        for y in (0.97, 0.90):
            m.ellipsoid((cx, y, 0.052), (0.008, 0.008, 0.006), 'steel', rows=3, cols=5)


def cape(m):
    """A cape hanging from the shoulders down the back: slabs that widen and swing out towards the hem, an edging, a clasp, a diamond."""
    z0, t = -0.128, 0.011
    rows = [(1.42, 0.17, z0), (1.20, 0.19, -0.145), (0.98, 0.215, -0.165), (0.62, 0.255, -0.205)]
    for a, b in zip(rows, rows[1:]):
        m.hull([(sx * a[1], a[0], a[2] + k * t) for sx in (-1, 1) for k in (-1, 1)] + [(sx * b[1], b[0], b[2] + k * t) for sx in (-1, 1) for k in (-1, 1)], 'metal')
    h, w, z = rows[-1]
    m.hull([(sx * w, h - 0.0, z + k * (t + 0.006)) for sx in (-1, 1) for k in (-1, 1)] + [(sx * (w + 0.003), h + 0.04, z + k * (t + 0.006)) for sx in (-1, 1) for k in (-1, 1)], 'trim')   # the hem edging
    for sx in (-1, 1):                                                                              # edging down both sides
        for a, b in zip(rows, rows[1:]):
            m.hull([(sx * a[1], a[0], a[2] + k * (t + 0.004)) for k in (-1, 1)] + [(sx * (a[1] - 0.018), a[0], a[2] + k * (t + 0.004)) for k in (-1, 1)] +
                   [(sx * b[1], b[0], b[2] + k * (t + 0.004)) for k in (-1, 1)] + [(sx * (b[1] - 0.018), b[0], b[2] + k * (t + 0.004)) for k in (-1, 1)], 'trim')
    m.hull([(0, 1.18, -0.153 - 0.015), (0.06, 1.10, -0.158 - 0.015), (0, 1.02, -0.163 - 0.015), (-0.06, 1.10, -0.158 - 0.015),
            (0, 1.18, -0.153 + 0.0), (0.06, 1.10, -0.158 + 0.0), (0, 1.02, -0.163 + 0.0), (-0.06, 1.10, -0.158 + 0.0)], 'trim')       # a diamond on the back
    for sx in (-1, 1):                                                                              # clasps at the shoulders
        m.ellipsoid((sx * 0.15, 1.425, -0.105), (0.020, 0.020, 0.016), 'trim', rows=3, cols=6)


def cape_drop(m):
    """A cape folded and lying flat."""
    m.box((0, 0.018, 0), (0.15, 0.018, 0.19), 'metal')
    m.box((0, 0.034, 0.17), (0.152, 0.008, 0.022), 'trim')
    m.box((0, 0.034, -0.17), (0.152, 0.008, 0.022), 'trim')
    m.hull([(0, 0.038, 0.06), (0.05, 0.038, 0.0), (0, 0.038, -0.06), (-0.05, 0.038, 0.0), (0, 0.044, 0.0)], 'trim')
    m.ellipsoid((0, 0.05, 0.13), (0.018, 0.012, 0.018), 'trim', rows=3, cols=6)


def lying_gloves(m, gauntlet=False, studs=False):
    for side in 'lr':
        sx = 1 if side == 'l' else -1
        m.xf = (lambda s_: (lambda p: (p[0] - s_ * 0.235 + s_ * 0.09, p[2] + 0.04, -(p[1] - 0.96))))(sx)
        glove(m, side, gauntlet, studs)


def standing_boots(m, studs=False):
    for side in 'lr':
        boot(m, side, studs)


# ------------------------------------------------------------------------------------------------ building
def build():
    # ---- leather boots and gloves
    for kind in ('leather', 'hard', 'studded'):
        pal = dict(BASE)
        pal.update(LEATHER[kind])
        for side in 'lr':
            m = _Model('gear_boots_%s_leg_%s' % (kind, side), origin=J['leg_' + side], palette=pal)
            boot(m, side, studs=(kind == 'studded'))
            m.finish(OUT_CHAR)
        m = _Model('drop_boots_' + kind, palette=pal)
        m.scale = 0.6
        standing_boots(m, studs=(kind == 'studded'))
        m.finish(OUT_PROPS)
    for kind in ('leather', 'hard'):
        pal = dict(BASE)
        pal.update(LEATHER[kind])
        for side in 'lr':
            m = _Model('gear_gloves_%s_arm_%s' % (kind, side), origin=J['arm_' + side], palette=pal)
            glove(m, side)
            m.finish(OUT_CHAR)
        m = _Model('drop_gloves_' + kind, palette=pal)
        m.scale = 0.8
        lying_gloves(m)
        m.finish(OUT_PROPS)
    # ---- smithed boots and gauntlets
    for tier in TIERS:
        pal = tier_palette(tier)
        for side in 'lr':
            m = _Model('gear_%s_boots_leg_%s' % (tier, side), origin=J['leg_' + side], palette=pal)
            boot(m, side)
            m.finish(OUT_CHAR)
            m = _Model('gear_%s_gauntlets_arm_%s' % (tier, side), origin=J['arm_' + side], palette=pal)
            glove(m, side, gauntlet=True)
            m.finish(OUT_CHAR)
        m = _Model('drop_%s_boots' % tier, palette=pal)
        m.scale = 0.6
        standing_boots(m)
        m.finish(OUT_PROPS)
        m = _Model('drop_%s_gauntlets' % tier, palette=pal)
        m.scale = 0.8
        lying_gloves(m, gauntlet=True)
        m.finish(OUT_PROPS)
    # ---- capes
    for key, (cloth, edge) in CAPES.items():
        pal = {'metal': cloth, 'trim': edge}
        m = _Model('gear_cape_' + key, origin=J['torso'], palette=pal)
        cape(m)
        m.finish(OUT_CHAR)
        m = _Model('drop_cape_' + key, palette=pal)
        cape_drop(m)
        m.finish(OUT_PROPS)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    build()
    print('wardrobe built')
