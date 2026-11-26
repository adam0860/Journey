"""The Abyss is where the best resources in the game are: adamant and gem rocks, mithril, magic and yew trees.

apply(text) adds them to the text of town.map (idempotent: a marker line says it has been done); build_town.py calls it after writing the map, and
running this file on its own does the same to assets/maps/town.map (so the map can be updated without an 8-minute Blender rebuild).
Positions are chosen on free ground inside the Abyss area, away from its gate, with space around every one (nothing is walled in).
"""
import math
import os
import random
import sys

MARKER = '// abyss riches (tools/abyss_riches.py)'

# (kind, how many, only south of this many tiles below the gate)
ROCKS = [('adamant', 16, 14), ('gems', 8, 14), ('mithril', 8, 6)]
TREES = [('magic', 10, 6), ('yew', 10, 6)]


def apply(text):
    if MARKER in text:
        return text
    lines = text.split('\n')
    ri = lines.index('@rows')
    head, rows = lines[:ri], [list(r) for r in lines[ri + 1:] if r != '']
    area = None
    for l in head:
        p = l.split(' ', 5)
        if l.startswith('@area ') and len(p) >= 6 and p[5] == 'The Abyss':
            area = tuple(int(v) for v in p[1:5])
    if not area:
        return text
    x0, z0, x1, z1 = area
    taken = []   # (x, z) of everything already there
    for l in head:
        p = l.split()
        if p and p[0] in ('@rock', '@tree'):
            taken.append((float(p[1]), float(p[2])))
        elif p and p[0] in ('@npc', '@person'):   # (the kind comes first)
            taken.append((float(p[2]), float(p[3])))
    rng = random.Random(2026)

    def free(x, z):
        if not (x0 + 3 <= x < x1 - 3 and z0 + 5 <= z < z1 - 2):
            return False
        for dz in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if rows[z + dz][x + dx] != '.':
                    return False
        return all((tx - x - 0.5) ** 2 + (tz - z - 0.5) ** 2 > 2.6 ** 2 for tx, tz in taken)

    out = []
    for kind, count, south in ROCKS + TREES:
        is_rock = (kind, count, south) in ROCKS
        placed = 0
        for _ in range(4000):
            if placed >= count:
                break
            x, z = rng.randint(x0 + 3, x1 - 4), rng.randint(z0 + south, z1 - 3)
            if not free(x, z):
                continue
            taken.append((x + 0.5, z + 0.5))
            rows[z][x] = 'o' if is_rock else 'T'
            if is_rock:
                out.append('@rock %.2f %.2f %s %.1f' % (x + 0.5, z + 0.5, kind, rng.uniform(0, 360)))
            else:
                out.append('@tree %.2f %.2f %s %.2f %.1f' % (x + 0.5, z + 0.5, kind, rng.uniform(0.95, 1.2), rng.uniform(0, 360)))
            placed += 1
    return '\n'.join(head + [MARKER] + out + ['@rows'] + [''.join(r) for r in rows]) + '\n'


def apply_file(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    new = apply(text)
    if new != text:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(new)
        print('abyss riches added to', path)
    else:
        print('abyss riches already there (or no Abyss) in', path)


if __name__ == '__main__':
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    apply_file(sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, 'assets', 'maps', 'town.map'))
