#!/usr/bin/env python3
"""
Journey - animals: chickens, cows and calves (Blender / bpy)

Each animal is a handful of small rigid parts in "pivot space" (the same idea as the character): the game turns the head,
swings the legs and wings, tilts the body and so on to animate them (walk, peck, graze, butt, hurt, fall over...).

  assets/models/animals/<species>_<colorway>_<part>.glb
  assets/models/animals/catalog.txt        joint positions (the pivot of every part) + the colourways

  chicken (2 colourways: white hen, brown hen)   parts: body head wing_l wing_r leg_l leg_r
  cow     (2 colourways: black-and-white, brown-and-white)   parts: body head tail leg_fl leg_fr leg_bl leg_br
  calf    (same colourways, a smaller, rounder, hornless cow)
  goblin  (2 looks: a plain goblin with a club, and a goblin warrior)   parts: body head arm_l arm_r leg_l leg_r

Animal space: y up, +z = the way it faces, +x = its left, origin on the ground in the middle. 1 unit = 1 metre.
Run:  blender -b --python tools/build_animals.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'assets', 'models', 'animals')

# ---------------------------------------------------------------------------------------------- colours
CHICKEN = [
    dict(feather='#f6f2e9', dark='#d8d2c2', comb='#d3342b', beak='#eaa83c', leg='#e6a63a', eye='#1c1c1c'),
    dict(feather='#ad7340', dark='#7a4b27', comb='#d3342b', beak='#dc9c36', leg='#d9a03a', eye='#1c1c1c'),
]
COW = [   # 'patch' is the colour of the spots: black for the first, brown for the second
    dict(base='#f3efe6', patch='#2a2623', muzzle='#eaa9a1', horn='#e9dfc6', hoof='#3a332d', eye='#141414', udder='#eca7a0'),
    dict(base='#f3efe6', patch='#9b5a2f', muzzle='#eaa9a1', horn='#e9dfc6', hoof='#3a332d', eye='#141414', udder='#eca7a0'),
]
CALF = [
    dict(base='#efe6d3', patch='#3a3430', muzzle='#eaa9a1', horn='#e9dfc6', hoof='#3a332d', eye='#141414', udder='#eca7a0'),
    dict(base='#e9d9bd', patch='#a4643a', muzzle='#eaa9a1', horn='#e9dfc6', hoof='#3a332d', eye='#141414', udder='#eca7a0'),
]

# ---------------------------------------------------------------------------------------------- chicken
CH_JOINTS = {'body': (0.0, 0.22, 0.0), 'head': (0.0, 0.33, 0.12), 'wing_l': (0.105, 0.27, 0.03), 'wing_r': (-0.105, 0.27, 0.03),
             'leg_l': (0.05, 0.15, 0.0), 'leg_r': (-0.05, 0.15, 0.0)}


def chicken(cw, c):
    name = lambda part: 'chicken_%d_%s' % (cw, part)
    # body: plump egg with a lifted tail
    m = Model(name('body'), CH_JOINTS['body'], c)
    m.ellipsoid((0, 0.22, 0.0), (0.115, 0.125, 0.165), 'feather', rows=5, cols=9)
    m.ellipsoid((0, 0.22, 0.075), (0.11, 0.12, 0.12), 'feather', rows=5, cols=9)
    m.ellipsoid((0, 0.235, -0.105), (0.10, 0.11, 0.11), 'feather', rows=4, cols=8)
    m.ellipsoid((0, 0.345, -0.215), (0.028, 0.105, 0.05), 'dark', rot=(-0.55, 0, 0), rows=4, cols=6)
    m.ellipsoid((0.045, 0.325, -0.205), (0.022, 0.09, 0.045), 'feather', rot=(-0.5, 0, -0.35), rows=4, cols=6)
    m.ellipsoid((-0.045, 0.325, -0.205), (0.022, 0.09, 0.045), 'feather', rot=(-0.5, 0, 0.35), rows=4, cols=6)
    yield m
    # head and neck (pivot = base of the neck)
    m = Model(name('head'), CH_JOINTS['head'], c)
    m.ellipsoid((0, 0.375, 0.14), (0.04, 0.07, 0.045), 'feather', rows=4, cols=7)
    m.ellipsoid((0, 0.44, 0.165), (0.05, 0.055, 0.06), 'feather', rows=5, cols=8)
    m.rod((0, 0.435, 0.195), (0, 0.428, 0.262), 0.022, 0.003, 'beak', n=6)
    for z, h in ((0.135, 0.02), (0.158, 0.03), (0.182, 0.022)):                    # comb
        m.ellipsoid((0, 0.495 + h * 0.3, z), (0.012, h, 0.02), 'comb', rows=4, cols=6)
    m.ellipsoid((0, 0.395, 0.205), (0.011, 0.026, 0.013), 'comb', rows=4, cols=6)   # wattle
    for sx in (-1, 1):
        m.ellipsoid((sx * 0.046, 0.448, 0.185), (0.011, 0.013, 0.011), 'eye', rows=3, cols=5)
    yield m
    # wings
    for side, sx in (('l', 1), ('r', -1)):
        m = Model(name('wing_' + side), (sx * 0.105, 0.27, 0.03), c)
        m.ellipsoid((sx * 0.125, 0.235, -0.01), (0.02, 0.075, 0.115), 'feather', rot=(0.12, 0, sx * 0.1), rows=4, cols=7)
        m.ellipsoid((sx * 0.128, 0.205, -0.065), (0.018, 0.045, 0.085), 'dark', rot=(0.12, 0, sx * 0.1), rows=4, cols=6)
        yield m
    # legs
    for side, sx in (('l', 1), ('r', -1)):
        m = Model(name('leg_' + side), (sx * 0.05, 0.15, 0.0), c)
        m.ellipsoid((sx * 0.05, 0.15, 0.0), (0.04, 0.045, 0.05), 'feather', rows=4, cols=7)
        m.rod((sx * 0.05, 0.14, 0.0), (sx * 0.05, 0.02, 0.0), 0.013, 0.011, 'leg', n=5)
        for dx, dz in ((0, 0.065), (0.025, 0.045), (-0.025, 0.045)):               # three toes
            m.rod((sx * 0.05, 0.016, 0.0), (sx * 0.05 + dx, 0.012, dz), 0.009, 0.005, 'leg', n=4)
        yield m



# ---------------------------------------------------------------------------------------------- goblin
GOBLIN = [   # 0 = a plain goblin (club, rags); 1 = a goblin warrior (iron cap with horns, chest plate, shield, sword)
    dict(skin='#6e9a3a', skin_dark='#527a2a', cloth='#7a5230', cloth_dark='#4e3320', eye='#ffd23a', pupil='#1a1a14', tooth='#efe6c8',
         wood='#8a5a30', wood_dark='#5a3a1c', metal='#8a8d94', metal_dark='#5d6068', hair='#2b2a1c', bone='#e9dfc6', mouth='#4a1f1f'),
    dict(skin='#7f9332', skin_dark='#626f26', cloth='#a1302b', cloth_dark='#6b1f1c', eye='#ff6a2a', pupil='#1a1a14', tooth='#efe6c8',
         wood='#7a4a28', wood_dark='#4e3018', metal='#7d818a', metal_dark='#53565e', hair='#1c1a12', bone='#e9dfc6', mouth='#4a1f1f'),
    dict(skin='#b8864a', skin_dark='#8f6633', cloth='#4c4036', cloth_dark='#2e261f', eye='#ffe04a', pupil='#1a1a14', tooth='#efe6c8',
         wood='#6a4426', wood_dark='#46301a', metal='#8a8d94', metal_dark='#5d6068', hair='#2a1c10', bone='#e9dfc6', mouth='#4a1f1f'),
    dict(skin='#c2362a', skin_dark='#8f2219', cloth='#2c1a30', cloth_dark='#1a0f1d', eye='#ffe040', pupil='#1a1a14', tooth='#f4ecd0',
         wood='#4a2a1a', wood_dark='#2a180e', metal='#6a5a62', metal_dark='#3e343a', hair='#1c0a0a', bone='#e9dfc6', mouth='#3a0f0f'),
]
GOB_JOINTS = {'body': (0.0, 0.52, 0.0), 'head': (0.0, 0.84, 0.05), 'arm_l': (0.17, 0.82, 0.03), 'arm_r': (-0.17, 0.82, 0.03),
              'leg_l': (0.07, 0.50, 0.0), 'leg_r': (-0.07, 0.50, 0.0)}


def goblin(cw, c):
    name = lambda part: 'goblin_%d_%s' % (cw, part)
    war = cw == 1
    # ---- body: a hunched torso with a pot belly and a ragged loincloth
    m = Model(name('body'), GOB_JOINTS['body'], c)
    m.ellipsoid((0, 0.70, 0.04), (0.15, 0.19, 0.13), 'skin', rot=(0.35, 0, 0), rows=5, cols=9)
    m.ellipsoid((0, 0.60, 0.075), (0.13, 0.12, 0.12), 'skin', rows=4, cols=8)
    m.hull([(sx * 0.125, 0.56, 0.095) for sx in (-1, 1)] + [(sx * 0.10, 0.31, 0.115) for sx in (-1, 1)] +
           [(sx * 0.12, 0.56, 0.06) for sx in (-1, 1)] + [(sx * 0.09, 0.32, 0.075) for sx in (-1, 1)], 'cloth')          # front flap
    m.hull([(sx * 0.125, 0.56, -0.02) for sx in (-1, 1)] + [(sx * 0.10, 0.34, -0.06) for sx in (-1, 1)] +
           [(sx * 0.12, 0.56, 0.0) for sx in (-1, 1)] + [(sx * 0.09, 0.35, -0.035) for sx in (-1, 1)], 'cloth')
    m.box((0, 0.565, 0.04), (0.14, 0.018, 0.115), 'cloth_dark')                                                        # belt
    for sx in (-1, 1):                                                                                                  # ragged hem
        m.hull([(sx * 0.09, 0.32, 0.10), (sx * 0.05, 0.32, 0.105), (sx * 0.07, 0.255, 0.115), (sx * 0.09, 0.32, 0.08)], 'cloth_dark')
    if war:
        m.hull([(sx * 0.15, 0.86, 0.13) for sx in (-1, 1)] + [(sx * 0.13, 0.62, 0.19) for sx in (-1, 1)] +
               [(sx * 0.14, 0.86, 0.05) for sx in (-1, 1)] + [(sx * 0.12, 0.62, 0.08) for sx in (-1, 1)], 'metal')         # chest plate
        m.box((0, 0.74, 0.19), (0.012, 0.11, 0.012), 'metal_dark')
        m.hull([(sx * 0.13, 0.64, 0.17) for sx in (-1, 1)] + [(sx * 0.14, 0.60, 0.17) for sx in (-1, 1)] +
               [(sx * 0.13, 0.64, 0.08) for sx in (-1, 1)] + [(sx * 0.14, 0.60, 0.08) for sx in (-1, 1)], 'cloth_dark')    # sash
    else:
        m.hull([(0.10, 0.86, 0.02), (0.19, 0.80, 0.03), (-0.10, 0.62, 0.17), (-0.04, 0.60, 0.15)], 'cloth')                   # one rag strap across the chest
    yield m
    # ---- head: big, with a hooked nose, long pointed ears, yellow eyes and a mouthful of fangs
    m = Model(name('head'), GOB_JOINTS['head'], c)
    m.ellipsoid((0, 0.90, 0.07), (0.045, 0.06, 0.045), 'skin', rows=3, cols=7)                                           # neck
    m.ellipsoid((0, 0.99, 0.10), (0.118, 0.108, 0.118), 'skin', rot=(0.15, 0, 0), rows=5, cols=9)                      # skull
    m.ellipsoid((0, 0.945, 0.20), (0.075, 0.075, 0.07), 'skin', rows=4, cols=8)                                          # cheeks and jaw
    m.ellipsoid((0, 0.955, 0.255), (0.028, 0.035, 0.055), 'skin_dark', rot=(0.4, 0, 0), rows=4, cols=7)                  # the nose
    m.ellipsoid((0, 0.93, 0.305), (0.02, 0.022, 0.025), 'skin_dark', rows=3, cols=6)
    for sx in (-1, 1):
        m.hull([(sx * 0.10, 1.00, 0.10), (sx * 0.105, 0.96, 0.05), (sx * 0.10, 1.02, 0.02), (sx * 0.34, 1.10, 0.0), (sx * 0.10, 0.99, 0.0)], 'skin')   # ear
        m.hull([(sx * 0.12, 0.995, 0.07), (sx * 0.20, 1.03, 0.03), (sx * 0.13, 0.985, 0.03)], 'skin_dark') if False else None
        m.ellipsoid((sx * 0.052, 1.015, 0.19), (0.026, 0.024, 0.02), 'eye', rows=3, cols=6)                              # eyes
        m.ellipsoid((sx * 0.052, 1.015, 0.207), (0.011, 0.016, 0.008), 'pupil', rows=3, cols=5)
        m.box((sx * 0.052, 1.052, 0.195), (0.034, 0.008, 0.016), 'skin_dark')                                           # angry brow
        m.hull([(sx * 0.028, 0.915, 0.215), (sx * 0.038, 0.915, 0.215), (sx * 0.033, 0.862, 0.222)], 'tooth') if False else None
        m.rod((sx * 0.032, 0.916, 0.222), (sx * 0.034, 0.868, 0.228), 0.011, 0.002, 'tooth', n=4)                      # fangs
    m.box((0, 0.917, 0.222), (0.045, 0.01, 0.014), 'mouth')
    if war:
        m.ellipsoid((0, 1.045, 0.095), (0.128, 0.085, 0.13), 'metal', rows=4, cols=9)                                    # iron cap
        m.box((0, 1.0, 0.205), (0.12, 0.012, 0.008), 'metal_dark') if False else None
        m.box((0, 0.99, 0.215), (0.012, 0.045, 0.01), 'metal_dark')                                                     # nose guard
        for sx in (-1, 1):
            m.rod((sx * 0.10, 1.075, 0.08), (sx * 0.18, 1.115, 0.07), 0.026, 0.012, 'bone', n=5)                      # horns
            m.rod((sx * 0.18, 1.115, 0.07), (sx * 0.21, 1.185, 0.06), 0.012, 0.003, 'bone', n=5)
    else:
        for i, (hx, hz) in enumerate(((-0.04, 0.02), (0.0, 0.0), (0.04, 0.03), (0.02, -0.04))):                         # a few wild hairs
            m.rod((hx, 1.08, 0.10 + hz), (hx * 1.8, 1.16, 0.10 + hz * 1.5), 0.012, 0.002, 'hair', n=4)
    yield m
    # ---- arms
    for side, sx in (('l', 1), ('r', -1)):
        m = Model(name('arm_' + side), (sx * 0.17, 0.82, 0.03), c)
        m.rod((sx * 0.17, 0.82, 0.03), (sx * 0.205, 0.56, 0.075), 0.040, 0.030, 'skin', n=7)
        m.ellipsoid((sx * 0.17, 0.835, 0.03), (0.055, 0.05, 0.055), 'skin', rows=3, cols=7)                              # shoulder
        m.ellipsoid((sx * 0.21, 0.525, 0.085), (0.042, 0.05, 0.042), 'skin_dark', rows=3, cols=7)                       # hand
        for k in (-1, 0, 1):
            m.rod((sx * 0.21 + k * 0.016, 0.505, 0.095), (sx * 0.21 + k * 0.02, 0.475, 0.115), 0.009, 0.003, 'tooth', n=4)   # claws
        m.box((sx * 0.205, 0.70, 0.052), (0.044, 0.012, 0.044), 'cloth_dark')                                           # wrist wrap
        if side == 'r':
            if war:   # a short notched sword
                m.rod((-0.21, 0.55, 0.09), (-0.21, 0.51, 0.115), 0.016, 0.016, 'wood_dark', n=6)
                m.box((-0.21, 0.495, 0.125), (0.075, 0.012, 0.014), 'metal_dark')
                m.hull([(-0.21 + dx, 0.49, 0.125 + dz) for dx in (-0.016, 0.016) for dz in (-0.006, 0.006)] +
                       [(-0.21 + dx, 0.17, 0.30 + dz) for dx in (-0.012, 0.012) for dz in (-0.004, 0.004)] + [(-0.21, 0.12, 0.33)], 'metal')
            else:     # a lumpy club studded with nails
                m.rod((-0.21, 0.54, 0.09), (-0.21, 0.30, 0.22), 0.024, 0.032, 'wood', n=6)
                m.ellipsoid((-0.21, 0.24, 0.255), (0.07, 0.10, 0.07), 'wood', rot=(0.5, 0, 0), rows=4, cols=7)
                for dx, dy, dz in ((0.06, 0.27, 0.255), (-0.06, 0.27, 0.255), (0.0, 0.22, 0.32), (0.0, 0.29, 0.19)):
                    m.rod((-0.21 + dx * 0.9, dy, dz), (-0.21 + dx * 1.35, dy + 0.0, dz + (0.015 if dz > 0.25 else -0.01)), 0.012, 0.003, 'metal', n=4)
        elif war:     # a round wooden shield with an iron rim strapped to the left forearm
            m.rod((0.255, 0.60, 0.085), (0.285, 0.60, 0.085), 0.145, 0.145, 'wood', n=10)
            m.rod((0.25, 0.60, 0.085), (0.262, 0.60, 0.085), 0.152, 0.152, 'metal_dark', n=10)
            m.ellipsoid((0.292, 0.60, 0.085), (0.02, 0.05, 0.05), 'metal', rows=3, cols=7)
        yield m
    # ---- legs: skinny, with big flat feet
    for side, sx in (('l', 1), ('r', -1)):
        m = Model(name('leg_' + side), (sx * 0.07, 0.50, 0.0), c)
        m.rod((sx * 0.07, 0.50, 0.0), (sx * 0.075, 0.13, 0.035), 0.052, 0.034, 'skin', n=7)
        m.ellipsoid((sx * 0.075, 0.10, 0.045), (0.036, 0.045, 0.04), 'skin', rows=3, cols=7)                             # ankle
        m.ellipsoid((sx * 0.078, 0.04, 0.10), (0.05, 0.036, 0.105), 'skin_dark', rows=3, cols=7)                       # foot
        for k in (-1, 0, 1):
            m.rod((sx * 0.078 + k * 0.026, 0.035, 0.18), (sx * 0.078 + k * 0.03, 0.022, 0.215), 0.012, 0.004, 'tooth', n=4)   # toe claws
        if war:
            m.rod((sx * 0.075, 0.30, 0.03), (sx * 0.078, 0.11, 0.04), 0.045, 0.042, 'cloth_dark', n=7)                 # leg wraps
        yield m

# ---------------------------------------------------------------------------------------------- cow and calf
def bovine_params(adult):
    k = 1.0 if adult else 0.64          # overall size
    hk = 1.0 if adult else 1.28         # head size relative to the body
    J = {'body': (0.0, 0.78, 0.0), 'head': (0.0, 0.98, 0.50), 'tail': (0.0, 0.98, -0.62),
         'leg_fl': (0.19, 0.60, 0.40), 'leg_fr': (-0.19, 0.60, 0.40), 'leg_bl': (0.19, 0.60, -0.42), 'leg_br': (-0.19, 0.60, -0.42)}
    if not adult:   # calves have longer legs for their body
        J['body'] = (0.0, 0.84, 0.0)
    return k, hk, J


def bovine(species, cw, c, adult):
    """Cow (adult) or calf. A calf is smaller, rounder-headed, hornless and has no udder."""
    k, hk, J = bovine_params(adult)
    name = lambda part: '%s_%d_%s' % (species, cw, part)

    def mk(part):
        m = Model(name(part), J[part], c)
        m.scale = k
        return m

    # ---- body
    m = mk('body')
    by = J['body'][1]
    m.ellipsoid((0, by, 0.0), (0.30, 0.30, 0.58), 'base', rows=5, cols=10)
    m.ellipsoid((0, by + 0.04, 0.30), (0.29, 0.31, 0.30), 'base', rows=5, cols=10)
    m.ellipsoid((0, by + 0.04, -0.34), (0.285, 0.30, 0.30), 'base', rows=5, cols=10)
    m.ellipsoid((0, by - 0.12, 0.0), (0.27, 0.18, 0.50), 'base', rows=4, cols=10)
    if adult:
        m.ellipsoid((0, by + 0.22, 0.28), (0.18, 0.10, 0.20), 'base', rows=4, cols=8)             # shoulder hump
        m.ellipsoid((0, by - 0.28, -0.30), (0.10, 0.09, 0.13), 'udder', rows=4, cols=8)           # udder
        for sx in (-1, 1):
            for z in (-0.26, -0.35):
                m.rod((sx * 0.04, by - 0.32, z), (sx * 0.04, by - 0.38, z), 0.014, 0.011, 'udder', n=5)
    # coat patches (flat bumps sitting on the surface)
    patches = [((0.285, by + 0.04, 0.10), (0.04, 0.17, 0.22)), ((-0.285, by, -0.15), (0.04, 0.20, 0.20)),
               ((0.0, by + 0.295, -0.12), (0.17, 0.04, 0.22)), ((0.20, by + 0.12, -0.52), (0.12, 0.14, 0.07)),
               ((-0.28, by + 0.10, 0.40), (0.04, 0.14, 0.15)), ((0.12, by - 0.21, 0.18), (0.13, 0.05, 0.20))]
    for pc, pr in patches:
        m.ellipsoid(pc, pr, 'patch', rows=4, cols=8, shade=1.0)
    yield m

    # ---- head and neck (pivot = base of the neck)
    m = mk('head')
    m.zoom = hk
    hy = J['head'][1]
    m.ellipsoid((0, hy - 0.04, 0.64), (0.15, 0.17, 0.20), 'base', rot=(0.5, 0, 0), rows=5, cols=8)         # neck
    m.ellipsoid((0, hy - 0.08, 0.83), (0.135, 0.15, 0.16), 'base', rot=(0.25, 0, 0), rows=5, cols=8)       # skull
    m.ellipsoid((0, hy - 0.14, 0.965), (0.105, 0.085, 0.10), 'muzzle', rot=(0.15, 0, 0), rows=4, cols=8)   # muzzle
    m.ellipsoid((0, hy - 0.01, 0.84), (0.10, 0.05, 0.10), 'patch', rows=4, cols=7)                         # face patch
    for sx in (-1, 1):
        m.ellipsoid((sx * 0.04, hy - 0.14, 1.055), (0.017, 0.017, 0.012), 'hoof', rows=3, cols=5)          # nostrils
        m.ellipsoid((sx * 0.108, hy - 0.04, 0.89), (0.022, 0.024, 0.022), 'eye', rows=3, cols=5)           # eyes
        m.ellipsoid((sx * 0.175, hy - 0.02, 0.77), (0.075, 0.026, 0.045), 'base', rot=(0, 0, sx * 0.35), rows=3, cols=6)   # ears
        if adult:
            m.rod((sx * 0.10, hy + 0.04, 0.80), (sx * 0.175, hy + 0.13, 0.78), 0.028, 0.006, 'horn', n=5)   # horns
    yield m

    # ---- tail
    m = mk('tail')
    ty = J['tail'][1]
    m.rod((0, ty, -0.62), (0, ty - 0.42, -0.68), 0.022, 0.013, 'base', n=5)
    m.ellipsoid((0, ty - 0.47, -0.685), (0.04, 0.07, 0.04), 'patch', rows=4, cols=6)
    yield m

    # ---- legs
    for part in ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br'):
        jx, jy, jz = J[part]
        front = part.startswith('leg_f')
        m = mk(part)
        th = (0.085, 0.15, 0.095) if front else (0.10, 0.17, 0.13)
        m.ellipsoid((jx, jy - 0.10, jz), th, 'base', rows=4, cols=8)                                   # upper leg
        m.rod((jx, jy - 0.18, jz), (jx, 0.07, jz), 0.052, 0.04, 'base', n=6)                           # cannon bone
        m.rod((jx, 0.075, jz), (jx, 0.0, jz + 0.01), 0.052, 0.056, 'hoof', n=6)                        # hoof
        yield m


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):   # (only our own files: tools/build_monsters.py keeps its models in the same folder)
        if f.endswith('.glb') and f.split('_')[0] in ('chicken', 'cow', 'calf', 'goblin'):
            os.remove(os.path.join(OUT, f))
    total = 0
    for cw, c in enumerate(CHICKEN):
        for m in chicken(cw, c):
            total += m.finish(OUT)
    for species, table, adult in (('cow', COW, True), ('calf', CALF, False)):
        for cw, c in enumerate(table):
            for m in bovine(species, cw, c, adult):
                total += m.finish(OUT)
    for cw, c in enumerate(GOBLIN):
        for m in goblin(cw, c):
            total += m.finish(OUT)
    lines = ['colorways chicken %d' % len(CHICKEN), 'colorways cow %d' % len(COW), 'colorways calf %d' % len(CALF),
             'colorways goblin %d' % len(GOBLIN)]
    for part, p in GOB_JOINTS.items():
        lines.append('joint goblin %s %.4f %.4f %.4f' % ((part,) + p))
    for part, p in CH_JOINTS.items():
        lines.append('joint chicken %s %.4f %.4f %.4f' % ((part,) + p))
    for species, adult in (('cow', True), ('calf', False)):
        k, hk, J = bovine_params(adult)
        for part, p in J.items():
            lines.append('joint %s %s %.4f %.4f %.4f' % (species, part, p[0] * k, p[1] * k, p[2] * k))
    with open(os.path.join(OUT, 'catalog.txt'), 'w') as f:
        f.write('// Journey animal catalog. Generated by tools/build_animals.py\n')
        f.write('\n'.join(lines) + '\n')
    print('animal faces exported:', total)


if __name__ == '__main__':
    main()
