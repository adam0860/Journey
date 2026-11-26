#!/usr/bin/env python3
"""
Journey - worn amulets (Blender / bpy)

  assets/models/character/gear_amulet_<kind>.glb    a chain with a pendant, worn on the chest (pivot = the torso joint, like the armour)

kinds: gold, sapphire, emerald, ruby, diamond, dragonstone, mummy (sandy gem), troll (a big tooth), opal, jade, topaz

Run:  blender -b --python tools/build_amulets.py
"""
import bpy, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_gear import Model, J, OUT_CHAR

GOLD = '#d6ac45'
GEMS = {'gold': None, 'sapphire': '#3b66e8', 'emerald': '#27b862', 'ruby': '#e02a42', 'diamond': '#e8f6ff', 'dragonstone': '#9a40e0',
        'mummy': '#d6b25a', 'troll': '#efe8d2', 'opal': '#d8e4ee', 'jade': '#46aa78', 'topaz': '#e8702c'}


def amulet(kind, gem):
    m = Model('gear_amulet_' + kind, 'bronze', J['torso'])
    for sx in (-1, 1):                                                       # the chain: a V from the shoulders to the chest
        m.rod((sx * 0.085, 1.475, 0.045), (sx * 0.012, 1.325, 0.163), 0.0065, 0.0065, GOLD, n=5)
    if kind == 'troll':                                                      # a big tooth on a thong
        m.ellipsoid((0, 1.285, 0.168), (0.020, 0.052, 0.016), gem, rows=4, cols=6)
        m.ellipsoid((0, 1.335, 0.166), (0.014, 0.014, 0.012), GOLD, rows=3, cols=6)
    else:
        m.ellipsoid((0, 1.295, 0.166), (0.036, 0.040, 0.012), GOLD, rows=3, cols=8)
        if gem:
            m.ellipsoid((0, 1.295, 0.176), (0.024, 0.028, 0.012), gem, rows=3, cols=6)
    m.finish(OUT_CHAR)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for kind, gem in GEMS.items():
        amulet(kind, gem)
    print('amulets built')
