#!/usr/bin/env python3
"""
Journey - monsters: wolves, giant rats, giant spiders and the dragon (Blender / bpy)

The same idea as the farm animals (tools/build_animals.py): a handful of rigid parts in "pivot space" that the game swings and
tilts. Everything that looks like a person (zombies, skeletons, trolls, mummies, the Abyss fighters) is drawn with the hero's own
body in the game, so it needs no models.

  assets/models/animals/<species>_<colorway>_<part>.glb
  assets/models/animals/catalog_monsters.txt       joints + colourways (read by AnimalRenderer after catalog.txt)

  wolf    (2 colourways: grey wolf, black dark wolf)          parts: body head tail leg_fl leg_fr leg_bl leg_br
  rat     (2 colourways: brown, grey)                         parts: body head tail leg_fl leg_fr leg_bl leg_br
  spider  (2 colourways: brown with red marks, green-black)   parts: body head leg_fl leg_fr leg_bl leg_br  (each leg part is TWO legs)
  dragon  (1 colourway: green)                                parts: body head tail leg_fl leg_fr leg_bl leg_br wing_l wing_r

Animal space: y up, +z = the way it faces, +x = its left, origin on the ground in the middle. 1 unit = 1 metre.
Run:  blender -b --python tools/build_monsters.py
"""
import bpy, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lowpoly import Model

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'assets', 'models', 'animals')

WOLF = [
    dict(fur='#8d9299', fur_dark='#5d6269', belly='#d2d4d8', eye='#e8c040', nose='#1c1c1e', tooth='#f4f0e0', inner='#caa0a0'),
    dict(fur='#2e3036', fur_dark='#16171b', belly='#4a4d55', eye='#ff3a2a', nose='#0c0c0e', tooth='#ece6d2', inner='#8a4048'),
    dict(fur='#c9a96a', fur_dark='#8c7440', belly='#efe2bd', eye='#3a2a10', nose='#1c1c1e', tooth='#f4f0e0', inner='#caa0a0'),
    dict(fur='#5a1a14', fur_dark='#2a0c09', belly='#8a2a1c', eye='#ffb020', nose='#0c0c0e', tooth='#ece6d2', inner='#c0602a'),
]
RAT = [
    dict(fur='#6f5a44', fur_dark='#4b3c2c', belly='#a88f74', eye='#101010', nose='#e8a0a0', tooth='#f1e6c0', tail='#d9a0a0'),
    dict(fur='#7d7b7e', fur_dark='#575558', belly='#b9b6b8', eye='#101010', nose='#e8a0a0', tooth='#f1e6c0', tail='#d9a8a8'),
    dict(fur='#43404c', fur_dark='#2a2830', belly='#6a6672', eye='#101010', nose='#e8a0a0', tooth='#f1e6c0', tail='#c89898'),
]
SPIDER = [
    dict(body='#3b2d26', dark='#1f1814', mark='#a02828', eye='#ff3030', fang='#efe6c8', leg='#2d221c'),
    dict(body='#243020', dark='#131a11', mark='#d8c030', eye='#ffe040', fang='#efe6c8', leg='#1c261a'),
    dict(body='#b9b2a2', dark='#8a8374', mark='#5a5448', eye='#202020', fang='#efe6c8', leg='#a49c8a'),
    dict(body='#c8a050', dark='#8e6a2a', mark='#3a2410', eye='#2a1a0a', fang='#2a1a0a', leg='#ae8438'),
    dict(body='#2a1838', dark='#150b1f', mark='#c040ff', eye='#ff40ff', fang='#efe6c8', leg='#22132e'),
]
DRAGON = [
    dict(scale='#3e7c3a', scale_dark='#285126', belly='#d8c27c', horn='#e9dfc2', claw='#2b2620', eye='#ffb020', membrane='#8a3a2e',
         membrane_dark='#5f2a22', tooth='#f4f0e0', mouth='#7a2a2a'),
    dict(scale='#2a2a36', scale_dark='#17171f', belly='#6a5a78', horn='#b8b0c8', claw='#111114', eye='#ff3030', membrane='#5a1a3a',
         membrane_dark='#380f24', tooth='#f0eadc', mouth='#6a1a1a'),
]

JOINTS = {}


# ------------------------------------------------------------------------------------------------ wolf and rat (quadrupeds)
def quad_joints(k, body_y, front_z, back_z, hip_x, leg_y, head_y, head_z, tail_y, tail_z):
    return {'body': (0.0, body_y, 0.0), 'head': (0.0, head_y, head_z), 'tail': (0.0, tail_y, tail_z),
            'leg_fl': (hip_x, leg_y, front_z), 'leg_fr': (-hip_x, leg_y, front_z),
            'leg_bl': (hip_x, leg_y, back_z), 'leg_br': (-hip_x, leg_y, back_z)}


WOLF_J = quad_joints(1.0, 0.60, 0.30, -0.30, 0.13, 0.50, 0.80, 0.42, 0.72, -0.50)
RAT_J = quad_joints(1.0, 0.17, 0.10, -0.10, 0.06, 0.12, 0.20, 0.17, 0.17, -0.20)


def wolf(cw, c):
    name = lambda p: 'wolf_%d_%s' % (cw, p)
    J = WOLF_J
    m = Model(name('body'), J['body'], c)
    m.ellipsoid((0, 0.60, 0.0), (0.17, 0.20, 0.42), 'fur', rows=5, cols=9)
    m.ellipsoid((0, 0.62, 0.22), (0.17, 0.22, 0.20), 'fur', rows=5, cols=9)             # deep chest
    m.ellipsoid((0, 0.60, -0.26), (0.16, 0.19, 0.20), 'fur', rows=5, cols=9)            # haunches
    m.ellipsoid((0, 0.50, 0.0), (0.12, 0.11, 0.34), 'belly', rows=4, cols=8)
    m.ellipsoid((0, 0.76, 0.14), (0.12, 0.10, 0.24), 'fur_dark', rows=4, cols=8)         # a darker saddle and a raised mane
    m.ellipsoid((0, 0.80, 0.30), (0.10, 0.09, 0.14), 'fur_dark', rows=4, cols=7)
    yield m
    m = Model(name('head'), J['head'], c)
    m.ellipsoid((0, 0.78, 0.40), (0.10, 0.12, 0.15), 'fur', rot=(0.4, 0, 0), rows=4, cols=8)         # neck
    m.ellipsoid((0, 0.82, 0.54), (0.105, 0.10, 0.11), 'fur', rows=4, cols=8)                          # skull
    m.ellipsoid((0, 0.77, 0.68), (0.055, 0.052, 0.115), 'fur', rot=(0.18, 0, 0), rows=4, cols=8)      # snout
    m.ellipsoid((0, 0.795, 0.79), (0.03, 0.026, 0.03), 'nose', rows=3, cols=6)
    m.ellipsoid((0, 0.71, 0.66), (0.045, 0.02, 0.09), 'belly', rows=3, cols=7)                        # jaw
    for sx in (-1, 1):
        m.hull([(sx * 0.07, 0.88, 0.50), (sx * 0.115, 0.89, 0.52), (sx * 0.095, 1.00, 0.49), (sx * 0.06, 0.88, 0.56)], 'fur')         # ears
        m.hull([(sx * 0.075, 0.885, 0.52), (sx * 0.1, 0.89, 0.53), (sx * 0.09, 0.97, 0.505)], 'inner')
        m.ellipsoid((sx * 0.075, 0.845, 0.61), (0.017, 0.015, 0.014), 'eye', rows=3, cols=5)
        m.rod((sx * 0.026, 0.745, 0.74), (sx * 0.028, 0.70, 0.745), 0.009, 0.002, 'tooth', n=4)                                      # fangs
    yield m
    m = Model(name('tail'), J['tail'], c)
    m.rod((0, 0.72, -0.48), (0, 0.58, -0.76), 0.055, 0.075, 'fur', n=6)
    m.ellipsoid((0, 0.52, -0.80), (0.07, 0.12, 0.08), 'fur_dark', rows=4, cols=7)
    yield m
    for part in ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br'):
        jx, jy, jz = J[part]
        front = part.startswith('leg_f')
        m = Model(name(part), J[part], c)
        m.ellipsoid((jx, jy - 0.02, jz), (0.07, 0.15, 0.10 if front else 0.12), 'fur', rows=4, cols=7)
        m.rod((jx, jy - 0.12, jz), (jx, 0.06, jz + (0.0 if front else -0.02)), 0.045, 0.032, 'fur', n=6)
        m.ellipsoid((jx, 0.03, jz + 0.03), (0.04, 0.03, 0.065), 'fur_dark', rows=3, cols=6)
        yield m


def rat(cw, c):
    name = lambda p: 'rat_%d_%s' % (cw, p)
    J = RAT_J
    m = Model(name('body'), J['body'], c)
    m.ellipsoid((0, 0.17, 0.0), (0.085, 0.085, 0.17), 'fur', rows=4, cols=8)
    m.ellipsoid((0, 0.17, -0.10), (0.09, 0.09, 0.11), 'fur', rows=4, cols=8)
    m.ellipsoid((0, 0.13, 0.0), (0.06, 0.05, 0.14), 'belly', rows=3, cols=7)
    yield m
    m = Model(name('head'), J['head'], c)
    m.ellipsoid((0, 0.20, 0.19), (0.055, 0.055, 0.07), 'fur', rows=4, cols=7)
    m.ellipsoid((0, 0.185, 0.27), (0.028, 0.03, 0.06), 'fur', rot=(0.2, 0, 0), rows=3, cols=6)
    m.ellipsoid((0, 0.19, 0.325), (0.012, 0.012, 0.012), 'nose', rows=3, cols=5)
    for sx in (-1, 1):
        m.ellipsoid((sx * 0.05, 0.255, 0.16), (0.03, 0.035, 0.012), 'nose', rows=3, cols=6)       # round pink ears
        m.ellipsoid((sx * 0.032, 0.215, 0.235), (0.009, 0.009, 0.009), 'eye', rows=3, cols=5)
        m.rod((sx * 0.01, 0.17, 0.30), (sx * 0.011, 0.145, 0.30), 0.005, 0.001, 'tooth', n=4)    # buck teeth
    yield m
    m = Model(name('tail'), J['tail'], c)
    m.rod((0, 0.17, -0.19), (0, 0.10, -0.38), 0.014, 0.008, 'tail', n=5)
    m.rod((0, 0.10, -0.38), (0, 0.04, -0.50), 0.008, 0.004, 'tail', n=5)
    yield m
    for part in ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br'):
        jx, jy, jz = J[part]
        m = Model(name(part), J[part], c)
        m.ellipsoid((jx, jy, jz), (0.035, 0.06, 0.05), 'fur', rows=3, cols=6)
        m.rod((jx, jy - 0.04, jz), (jx, 0.012, jz + 0.02), 0.02, 0.012, 'fur', n=5)
        m.ellipsoid((jx, 0.01, jz + 0.035), (0.022, 0.012, 0.04), 'nose', rows=3, cols=5)
        yield m


# ------------------------------------------------------------------------------------------------ spider
SPIDER_J = {'body': (0.0, 0.42, 0.0), 'head': (0.0, 0.42, 0.14),
            'leg_fl': (0.10, 0.44, 0.10), 'leg_fr': (-0.10, 0.44, 0.10), 'leg_bl': (0.10, 0.44, -0.06), 'leg_br': (-0.10, 0.44, -0.06)}


def spider(cw, c):
    name = lambda p: 'spider_%d_%s' % (cw, p)
    J = SPIDER_J
    m = Model(name('body'), J['body'], c)
    m.ellipsoid((0, 0.46, -0.24), (0.21, 0.20, 0.27), 'body', rows=5, cols=9)          # the fat abdomen
    m.ellipsoid((0, 0.62, -0.20), (0.07, 0.04, 0.16), 'mark', rows=3, cols=7)           # marks on its back
    m.ellipsoid((0, 0.585, -0.34), (0.11, 0.04, 0.07), 'mark', rows=3, cols=7)
    m.ellipsoid((0, 0.43, 0.04), (0.14, 0.12, 0.16), 'body', rows=4, cols=8)            # cephalothorax
    m.ellipsoid((0, 0.50, 0.02), (0.07, 0.05, 0.08), 'dark', rows=3, cols=6)
    yield m
    m = Model(name('head'), J['head'], c)
    m.ellipsoid((0, 0.41, 0.19), (0.095, 0.085, 0.085), 'dark', rows=4, cols=8)
    for sx in (-1, 1):
        m.ellipsoid((sx * 0.035, 0.455, 0.255), (0.018, 0.018, 0.012), 'eye', rows=3, cols=5)
        m.ellipsoid((sx * 0.065, 0.435, 0.235), (0.012, 0.012, 0.01), 'eye', rows=3, cols=5)
        m.rod((sx * 0.04, 0.37, 0.25), (sx * 0.05, 0.27, 0.285), 0.022, 0.006, 'fang', n=5)            # fangs
        m.rod((sx * 0.07, 0.40, 0.24), (sx * 0.10, 0.33, 0.30), 0.016, 0.006, 'dark', n=5)             # palps
    yield m
    # each leg part holds two legs of one side: a thigh up and out, a shin down to the ground
    for part in ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br'):
        jx, jy, jz = J[part]
        sx = 1 if jx > 0 else -1
        front = part.startswith('leg_f')
        m = Model(name(part), J[part], c)
        for k, dz in enumerate((0.20, 0.02) if front else (-0.16, -0.34)):
            zz = jz + dz * 0.6
            kx = jx + sx * 0.05
            knee = (jx + sx * 0.36, jy + 0.20, jz + dz * 1.5)
            foot = (jx + sx * 0.66, 0.0, jz + dz * 2.1)
            m.rod((kx, jy, zz), knee, 0.030, 0.022, 'leg', n=5)
            m.rod(knee, foot, 0.022, 0.008, 'leg', n=5)
            m.ellipsoid(knee, (0.032, 0.032, 0.032), 'dark', rows=3, cols=5)
        yield m


# ------------------------------------------------------------------------------------------------ dragon
DRAGON_J = {'body': (0.0, 1.15, 0.0), 'head': (0.0, 1.55, 0.80), 'tail': (0.0, 1.0, -0.95),
            'leg_fl': (0.36, 1.0, 0.60), 'leg_fr': (-0.36, 1.0, 0.60), 'leg_bl': (0.38, 1.0, -0.62), 'leg_br': (-0.38, 1.0, -0.62),
            'wing_l': (0.30, 1.62, 0.22), 'wing_r': (-0.30, 1.62, 0.22)}


def dragon(cw, c):
    name = lambda p: 'dragon_%d_%s' % (cw, p)
    J = DRAGON_J
    m = Model(name('body'), J['body'], c)
    m.ellipsoid((0, 1.15, 0.0), (0.50, 0.50, 0.95), 'scale', rows=5, cols=10)
    m.ellipsoid((0, 1.20, 0.55), (0.46, 0.50, 0.42), 'scale', rows=5, cols=10)
    m.ellipsoid((0, 1.10, -0.55), (0.44, 0.46, 0.42), 'scale', rows=5, cols=10)
    m.ellipsoid((0, 0.98, 0.05), (0.38, 0.30, 0.85), 'belly', rows=4, cols=9)                   # pale belly plates
    for i in range(7):                                                                           # a row of back spikes
        z = 0.7 - i * 0.28
        m.hull([(sx * 0.05, 1.58 - 0.02 * i, z) for sx in (-1, 1)] + [(0, 1.88 - 0.03 * i, z - 0.06)], 'scale_dark')
    for sx in (-1, 1):
        m.ellipsoid((sx * 0.46, 1.38, 0.40), (0.10, 0.12, 0.2), 'scale_dark', rows=3, cols=7)
    yield m
    m = Model(name('head'), J['head'], c)
    m.rod((0, 1.50, 0.70), (0, 1.88, 1.00), 0.24, 0.17, 'scale', n=8)                          # a long neck rising
    m.ellipsoid((0, 1.98, 1.14), (0.20, 0.17, 0.26), 'scale', rows=4, cols=8)                  # skull
    m.ellipsoid((0, 1.92, 1.42), (0.13, 0.10, 0.22), 'scale', rows=4, cols=8)                  # snout
    m.ellipsoid((0, 1.82, 1.36), (0.12, 0.05, 0.2), 'mouth', rows=3, cols=7)                    # lower jaw
    for sx in (-1, 1):
        m.ellipsoid((sx * 0.13, 2.04, 1.28), (0.035, 0.035, 0.03), 'eye', rows=3, cols=6)
        m.rod((sx * 0.12, 2.10, 1.02), (sx * 0.22, 2.34, 0.78), 0.05, 0.012, 'horn', n=6)      # swept-back horns
        m.rod((sx * 0.05, 1.86, 1.50), (sx * 0.05, 1.77, 1.52), 0.018, 0.004, 'tooth', n=4)
        m.ellipsoid((sx * 0.05, 1.94, 1.55), (0.02, 0.02, 0.02), 'scale_dark', rows=3, cols=5)  # nostrils
    yield m
    m = Model(name('tail'), J['tail'], c)
    pts = [(0, 1.05, -0.90), (0, 0.95, -1.40), (0, 0.80, -1.90), (0, 0.65, -2.35), (0, 0.55, -2.75)]
    rs = [0.30, 0.22, 0.15, 0.09, 0.03]
    for a, b, ra, rb in zip(pts, pts[1:], rs, rs[1:]):
        m.rod(a, b, ra, rb, 'scale', n=7)
    for i in range(1, 5):
        m.hull([(sx * 0.03, pts[i][1] + rs[i] - 0.01, pts[i][2]) for sx in (-1, 1)] + [(0, pts[i][1] + rs[i] + 0.2, pts[i][2] - 0.08)], 'scale_dark')
    yield m
    for part in ('leg_fl', 'leg_fr', 'leg_bl', 'leg_br'):
        jx, jy, jz = J[part]
        sx = 1 if jx > 0 else -1
        front = part.startswith('leg_f')
        m = Model(name(part), J[part], c)
        m.ellipsoid((jx, jy - 0.05, jz), (0.20, 0.34, 0.28), 'scale', rows=4, cols=8)            # thigh
        m.rod((jx, jy - 0.3, jz), (jx + sx * 0.03, 0.10, jz + 0.04), 0.15, 0.11, 'scale', n=7)
        m.ellipsoid((jx + sx * 0.03, 0.06, jz + 0.14), (0.14, 0.06, 0.22), 'scale_dark', rows=3, cols=7)   # foot
        for k in (-1, 0, 1):
            m.rod((jx + sx * 0.03 + k * 0.07, 0.04, jz + 0.30), (jx + sx * 0.03 + k * 0.08, 0.02, jz + 0.42), 0.035, 0.006, 'claw', n=4)
        yield m
    for side, sx in (('l', 1), ('r', -1)):
        m = Model(name('wing_' + side), J['wing_' + side], c)
        x0, y0, z0 = sx * 0.30, 1.62, 0.22
        elbow = (sx * 1.30, 2.35, 0.05)
        tip = (sx * 2.50, 2.10, -0.50)
        m.rod((x0, y0, z0), elbow, 0.07, 0.05, 'scale_dark', n=6)                                # the arm bone
        m.rod(elbow, tip, 0.05, 0.015, 'scale_dark', n=6)
        for fx, fz in ((sx * 1.75, -0.35), (sx * 1.95, -0.80), (sx * 1.2, -0.55)):               # fingers across the membrane
            m.rod(elbow, (fx, 1.35 if abs(fx) > 1.5 else 1.0, fz - 0.3), 0.025, 0.008, 'scale_dark', n=5)
        m.hull([(x0, y0 - 0.02, z0 - 0.05), elbow, (sx * 1.75, 1.40, -0.65), (sx * 0.5, 1.05, -0.70), (x0, y0 - 0.25, z0 - 0.4)], 'membrane')
        m.hull([elbow, tip, (sx * 1.95, 1.50, -1.0), (sx * 1.75, 1.40, -0.65)], 'membrane_dark')
        yield m


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    os.makedirs(OUT, exist_ok=True)
    total = 0
    lines = []
    for sp, table, fn, joints in (('wolf', WOLF, wolf, WOLF_J), ('rat', RAT, rat, RAT_J), ('spider', SPIDER, spider, SPIDER_J),
                                  ('dragon', DRAGON, dragon, DRAGON_J)):
        for cw, c in enumerate(table):
            for m in fn(cw, c):
                total += m.finish(OUT)
        lines.append('colorways %s %d' % (sp, len(table)))
        for part, p in joints.items():
            lines.append('joint %s %s %.4f %.4f %.4f' % ((sp, part) + p))
    with open(os.path.join(OUT, 'catalog_monsters.txt'), 'w') as f:
        f.write('// Journey monster catalog. Generated by tools/build_monsters.py\n')
        f.write('\n'.join(lines) + '\n')
    print('monster faces exported:', total)


if __name__ == '__main__':
    main()
