#!/usr/bin/env python3
"""
Journey - the wider world (Blender / bpy). Called by build_town.py after the hand-built core has been made.

  expand(T, bt)   moves the core (the long strip: castle town, Millbrook, the lake, the mountains, the caves, the desert, the Abyss) into the
                  middle of the bigger world (worldmap.py has the layout), then builds everything else around it:
                    ground in every biome (blended edges), the river, the sea and the swamp pools, roads and three bridges,
                    mountains and black cliffs, trees (choppable, drawn by the game), rocks, flowers, cacti, graves...,
                    the settlements (Elmswick, Saltmere, ...) and the monsters, resources and people of each wild place.

`bt` is the build_town module (the Town class, the palette, the chunk size). The core's own code is untouched: it is built in its own
coordinates, and here every coordinate is shifted by (OX, OZ) - a whole number of chunks, so the model chunks line up.
"""
import math, random
import bmesh
from mathutils import Vector

import worldmap as WM


def bt_pal():
    import sys
    return sys.modules['__main__'].PAL


def expand(T, bt):
    CH = bt.CH
    OX, OZ, WW, HH = WM.OX, WM.OZ, WM.WW, WM.HH
    P = bt.P
    B = bt.B
    ctx = Wild(T, bt)
    ctx.move_core()
    ctx.build_settlements()
    ctx.ground()
    ctx.mountains()
    ctx.scatter()
    ctx.populate_regions()
    ctx.skirt()
    ctx.report()
    T.extra_areas = ctx.extra_areas        # (the map file lists them after the wild regions: villages, camps and ruins inside them)
    return T


class Wild:
    def __init__(self, T, bt):
        self.T, self.bt = T, bt
        self.B = bt.B
        self.pr = random.Random(5150)          # placement
        self.override = {}                     # (x, z) -> ground material, for settlements and other special floors
        self.reserved = set()                  # tiles nothing random may be put on (settlements, roads, bridges)
        self.biome = {}                        # (x, z) -> biome, for every tile outside the core
        self.bio_cache = {}
        self.extra_areas = []
        self.village_rects = []                # settlements: roads end at their edge, baked shadows are on inside them

    # ------------------------------------------------------------------------------------------ helpers
    def in_core(self, x, z):
        return WM.in_core(x, z)

    def bio(self, x, z):
        k = (x, z)
        if k not in self.bio_cache:
            self.bio_cache[k] = WM.biome_at(x, z)
        return self.bio_cache[k]

    def ok(self, x, z):
        return 0 <= x < WM.WW and 0 <= z < WM.HH

    def free(self, x, z):
        return self.ok(x, z) and self.T.grid[z][x] == '.' and (x, z) not in self.reserved

    def tree_at(self, x, z, kind, scale=None):
        """A choppable tree: a line in the map file (the game draws it), and a blocked tile."""
        T = self.T
        if not self.ok(x, z):
            return
        T.grid[z][x] = 'T'
        T.occupied.add((x, z))
        T.trees.append((x + self.pr.uniform(0.2, 0.8), z + self.pr.uniform(0.2, 0.8), kind, scale or self.pr.uniform(0.85, 1.25), self.pr.uniform(0, 6.28)))

    def rock_at(self, x, z, sc=1.0, mat='rock'):
        """A boulder (baked into the ground chunks, so a cheap one)."""
        T = self.T
        T.blob(x + 0.5, z + 0.5, 0.12 * sc, 0.62 * sc, 0.5 * sc, 0.46 * sc, mat, jitter=0.14, sub=1)
        T.grid[z][x] = 'o'
        T.occupied.add((x, z))

    def far_rock(self, kind, x, z):
        T = self.T
        T.rocks.append((x + 0.5, z + 0.5, kind, self.pr.uniform(0, 360)))
        T.grid[z][x] = 'o'
        T.occupied.add((x, z))

    def person(self, key, x, z, yaw=0.0):
        T = self.T
        T.persons.append((key, x, z, yaw))
        T.grid[int(z)][int(x)] = 'o'

    reach = None   # set of tiles the hero can walk to from the spawn (filled in by flood())

    def flood(self):
        T = self.T
        WW, HH = WM.WW, WM.HH
        sx, sz = 32 + WM.OX, 30 + WM.OZ
        seen = {(sx, sz)}
        stack = [(sx, sz)]
        while stack:
            x, z = stack.pop()
            for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, nz = x + dx, z + dz
                if 0 <= nx < WW and 0 <= nz < HH and (nx, nz) not in seen and T.grid[nz][nx] in '.D':
                    seen.add((nx, nz))
                    stack.append((nx, nz))
        return seen

    def tiles(self, rect, pred=None, in_village=False):
        x0, z0, x1, z1 = rect
        out = []
        for z in range(max(0, z0), min(WM.HH, z1)):
            for x in range(max(0, x0), min(WM.WW, x1)):
                if self.T.grid[z][x] == '.' and (in_village or (x, z) not in self.reserved) and not self.in_core_tile_blocked(x, z) and (self.reach is None or (x, z) in self.reach):
                    if pred is None or pred(x, z):
                        out.append((x, z))
        return out

    def in_core_tile_blocked(self, x, z):
        return False

    def pen(self, name, rect, species_counts, in_village=False):
        """A wild pen: the creatures roam the open tiles inside `rect` (x0, z0, x1, z1)."""
        T = self.T
        name = 'w_' + name
        tiles = self.tiles(rect, in_village=in_village)
        if not tiles:
            print('pen', name, 'has no free tiles')
            return
        T.pens.append((name, rect[0], rect[1], rect[2], rect[3]))
        taken = []
        for species, count in species_counts:
            for _ in range(count):
                for _try in range(300):
                    x, z = self.pr.choice(tiles)
                    if all(abs(x - tx) + abs(z - tz) >= 3 for tx, tz in taken):
                        taken.append((x, z))
                        T.npcs.append((species, x + self.pr.uniform(0.3, 0.7), z + self.pr.uniform(0.3, 0.7), name))
                        break

    # ------------------------------------------------------------------------------------------ 1. the core moves
    def move_core(self):
        T, bt = self.T, self.bt
        CH = bt.CH
        OX, OZ, WW, HH = WM.OX, WM.OZ, WM.WW, WM.HH
        shift = Vector((OX, -OZ, 0.0))
        moved = {}
        for key, bm in list(T.bms.items()):
            if isinstance(key, tuple):
                bmesh.ops.translate(bm, vec=shift, verts=bm.verts[:])
                moved[(key[0] + OX // CH, key[1] + OZ // CH)] = bm
            elif isinstance(key, str) and key.startswith('roof_'):
                bmesh.ops.translate(bm, vec=shift, verts=bm.verts[:])
                moved[key] = bm
            elif isinstance(key, str) and key.startswith('skirt'):
                bm.free()
            else:
                moved[key] = bm
        T.bms = moved
        bt.W, bt.H = WW, HH
        bt.NCX, bt.NCZ = (WW + CH - 1) // CH, (HH + CH - 1) // CH
        old = T.grid
        T.grid = [['.' for _ in range(WW)] for _ in range(HH)]
        for z, row in enumerate(old):
            T.grid[z + OZ][OX:OX + len(row)] = row
        T.trees = [(x + OX, z + OZ, k, s, r) for (x, z, k, s, r) in T.trees]
        T.spots = [(x + OX, z + OZ, k) for (x, z, k) in T.spots]
        T.rocks = [(x + OX, z + OZ, k, y) for (x, z, k, y) in T.rocks]
        T.stations = [(k, x + OX, z + OZ, y) for (k, x, z, y) in T.stations]
        T.persons = [(k, x + OX, z + OZ, y) for (k, x, z, y) in T.persons]
        T.pens = [(n, x0 + OX, z0 + OZ, x1 + OX, z1 + OZ) for (n, x0, z0, x1, z1) in T.pens]
        T.npcs = [(s, x + OX, z + OZ, p) for (s, x, z, p) in T.npcs]
        T.roof_rects = {k: (r[0] + OX, r[1] + OZ, r[2] + OX, r[3] + OZ) for k, r in T.roof_rects.items()}
        T.occupied = {(x + OX, z + OZ) for (x, z) in T.occupied}
        T.floors = []
        T.shadow_rects = [(OX, OZ, OX + 64, OZ + 216)]   # baked sun shadows only where there are buildings (more are added by the settlements)
        # water outside the core is part of the grid too
        for z in range(HH):
            for x in range(WW):
                if not WM.in_core(x, z) and WM.is_water(x, z):
                    T.grid[z][x] = '~'

    # ------------------------------------------------------------------------------------------ 2. settlements
    def village(self, rect):
        self.village_rects.append(rect)
        self.T.shadow_rects.append(rect)
        for z in range(rect[1], rect[3]):
            for x in range(rect[0], rect[2]):
                self.reserved.add((x, z))

    def pave(self, rect, mat='cobble_a'):
        for z in range(rect[1], rect[3]):
            for x in range(rect[0], rect[2]):
                if self.ok(x, z) and self.T.grid[z][x] != '~':
                    self.override[(x, z)] = mat

    def fence(self, x0, z0, x1, z1, gap=None):
        """A post-and-rail fence round a rectangle (tiles x0..x1-1, z0..z1-1); `gap` = (side, a0, a1) leaves a gate. Arrows fly over ('f')."""
        T = self.T
        for z in range(z0, z1):
            for x in range(x0, x1):
                if not (x == x0 or x == x1 - 1 or z == z0 or z == z1 - 1):
                    continue
                if gap:
                    side, a0, a1 = gap
                    if (side == 'S' and z == z1 - 1 and a0 <= x < a1) or (side == 'N' and z == z0 and a0 <= x < a1) or \
                       (side == 'W' and x == x0 and a0 <= z < a1) or (side == 'E' and x == x1 - 1 and a0 <= z < a1):
                        continue
                T.box(x + 0.44, z + 0.44, x + 0.56, z + 0.56, 0, 1.0, 'wood_dark')
                T.box(x, z + 0.46, x + 1, z + 0.54, 0.34, 0.42, 'wood')
                T.box(x, z + 0.46, x + 1, z + 0.54, 0.68, 0.76, 'wood')
                T.grid[z][x] = 'f'

    def tent(self, cx, cz, r, h, mat):
        T = self.T
        T.cyl(cx, cz, r, 0.0, 0.0, h, 7, mat, rot=0.4)
        T.mark(cx - r * 0.7, cz - r * 0.7, cx + r * 0.7, cz + r * 0.7, 'o')

    def campfire(self, x, z):
        T = self.T
        for k in range(7):
            a = k * 0.9
            T.blob(x + 0.5 + math.cos(a) * 0.55, z + 0.5 + math.sin(a) * 0.55, 0.0, 0.16, 0.14, 0.12, 'rock', jitter=0.02, sub=1)
        for k in range(4):
            a = k * 1.6
            T.obox((x + 0.5 + math.cos(a) * 0.4, z + 0.5 + math.sin(a) * 0.4, 0.06), (x + 0.5, z + 0.5, 0.35), 0.1, 0.1, 'wood_dark')
        T.cyl(x + 0.5, z + 0.5, 0.18, 0.0, 0.1, 0.55, 5, 'glow')
        T.grid[z][x] = 'o'

    def stones_circle(self, cx, cz, r, n, h=(1.6, 2.6)):
        T = self.T
        for k in range(n):
            a = 2 * math.pi * k / n
            x, z = cx + math.cos(a) * r, cz + math.sin(a) * r
            hh = self.pr.uniform(*h)
            T.box(x - 0.35, z - 0.25, x + 0.35, z + 0.25, 0, hh, 'stone_dark')
            T.box(x - 0.3, z - 0.22, x + 0.3, z + 0.22, hh, hh + 0.12, 'stone')
            if self.ok(int(x), int(z)):
                T.grid[int(z)][int(x)] = 'o'

    def mound(self, cx, cz, r, h):
        """A barrow: a grassy dome with a dark doorway."""
        T = self.T
        T.blob(cx, cz, 0.0, r, r, h, 'moss', jitter=0.1, sub=2)
        T.box(cx - 0.55, cz + r * 0.75, cx + 0.55, cz + r * 0.75 + 0.6, 0, 1.3, 'obsidian')
        T.box(cx - 0.7, cz + r * 0.75 - 0.05, cx + 0.7, cz + r * 0.75 + 0.1, 1.2, 1.5, 'stone_dark')
        for z in range(int(cz - r), int(cz + r) + 1):
            for x in range(int(cx - r), int(cx + r) + 1):
                if (x + 0.5 - cx) ** 2 + (z + 0.5 - cz) ** 2 < (r * 0.8) ** 2 and self.ok(x, z) and T.grid[z][x] == '.':
                    T.grid[z][x] = 'o'

    def palm(self, x, z):
        T = self.T
        cx, cz = x + 0.5, z + 0.5
        T.cyl(cx, cz, 0.16, 0.10, 0, 3.2, 6, 'trunk')
        for k in range(6):
            a = k * 1.05
            T.obox((cx, cz, 3.1), (cx + math.cos(a) * 1.3, cz + math.sin(a) * 1.3, 2.7), 0.35, 0.05, 'leaf_b')
        T.grid[z][x] = 'T'

    def build_settlements(self):
        T = self.T
        pr = self.pr
        # ---- Elmswick: a woodcutters' village across the river from the castle
        self.village((16, 84, 46, 112))
        self.pave((16, 97, 46, 100))
        self.pave((29, 84, 32, 112))
        self.pave((26, 94, 35, 103), 'plaza_a')
        for (rect, door, roof) in (((17, 89, 24, 95), 'S', 'roof_red'), ((36, 89, 44, 95), 'S', 'roof_brown'),
                                   ((17, 102, 24, 108), 'N', 'roof_blue'), ((36, 102, 44, 108), 'N', 'roof_thatch')):
            T.house(rect[0], rect[1], rect[2], rect[3], door, roof=roof)
        T.well(31, 98)
        T.stall(33.0, 94.2, 36.4, 96.2, 'cloth_green', 'cloth_white', ('leaf_b', 'wood', 'flower_y'))
        self.person('perrin', 34.7, 94.8, 0.0)
        T.stall(25.0, 100.4, 28.4, 102.4, 'banner_red', 'cloth_white', ('wood', 'iron', 'leaf_a'))
        self.person('hobb', 26.7, 101.0, 0.0)
        for (lx, lz) in ((28.4, 96.5), (33.2, 99.6), (28.5, 104.0)):
            T.lamp(lx, lz)
        T.barrel(37.0, 97.2)
        T.crate(24.6, 99.0, 0.7)
        T.sign(47.5, 95.0)
        T.sign(15.0, 94.0)
        for k in range(5):   # a woodpile and stumps
            T.cyl(41.0 + k * 0.3, 111.0, 0.15, 0.15, 0, 0.5, 6, 'wood')
        self.extra_areas.append((14, 82, 48, 112, 'Elmswick'))

        # ---- Saltmere: a harbour on the east coast (a pier into the sea, fishing, a market)
        self.village((142, 80, 182, 104))
        self.pave((142, 90, 178, 93))
        self.pave((164, 82, 168, 104))
        self.pave((160, 88, 172, 95), 'plaza_a')
        for (rect, door, roof) in (((146, 83, 153, 88), 'S', 'roof_red'), ((156, 83, 163, 88), 'S', 'roof_slate'),
                                   ((170, 82, 178, 88), 'S', 'roof_blue'), ((146, 96, 153, 101), 'N', 'roof_brown'),
                                   ((170, 96, 178, 101), 'N', 'roof_thatch')):
            T.house(rect[0], rect[1], rect[2], rect[3], door, roof=roof)
        T.stall(158.0, 96.2, 161.6, 98.2, 'cloth_blue' if 'cloth_blue' in bt_pal() else 'banner_blue', 'cloth_white', ('leaf_b', 'flower_w', 'iron'))
        self.person('brine', 159.8, 96.8, 0.0)
        self.person('captain', 176.5, 94.5, 270.0)
        for (lx, lz) in ((163.2, 92.0), (169.6, 92.0), (150.0, 92.0)):
            T.lamp(lx, lz)
        for (bx, bz) in ((174.5, 96.5), (175.2, 97.3), (150.5, 89.5)):
            T.barrel(bx, bz)
        T.sign(141.0, 94.0)
        # nets drying on frames
        for (nx, nz) in ((154, 93), (156, 93)):
            T.box(nx, nz + 0.4, nx + 0.1, nz + 0.5, 0, 1.6, 'wood_dark')
        T.box(154, 93.38, 156.1, 93.52, 1.4, 1.5, 'wood')
        T.box(154.15, 93.4, 156.0, 93.46, 0.6, 1.4, 'rope' if 'rope' in bt_pal() else 'straw')
        T.grid[93][154] = 'o'
        T.grid[93][156] = 'o'
        # the pier: planks over the sea
        for z in (91, 92):
            xs = [x for x in range(178, WM.WW) if T.grid[z][x] == '~' or T.grid[z][x] == '.']
            for x in xs:
                if T.grid[z][x] == '~':
                    T.grid[z][x] = '.'
            T.box(178.0, z, WM.WW - 1.0, z + 1, 0.0, 0.12, 'wood', c=(185, z))
        for xx in range(179, WM.WW - 1, 3):
            for zz in (90.95, 93.05):
                T.box(xx - 0.1, zz - 0.1, xx + 0.1, zz + 0.1, -0.35, 0.5, 'wood_dark', c=(xx, zz))
        for (sx, sz, kind) in ((184, 90, 'net'), (186, 90, 'harpoon'), (189, 90, 'cage'), (184, 93, 'rod'), (186, 93, 'net'), (189, 93, 'harpoon'),
                               (181, 93, 'net'), (181, 90, 'rod')):
            if self.ok(sx, sz) and T.grid[sz][sx] == '~':
                T.spots.append((sx + 0.5, sz + 0.5, kind))
        self.extra_areas.append((140, 78, 184, 106, 'Saltmere'))

        # ---- Sunfield Farm: a farmhouse, a barn, a fenced paddock with cows and chickens
        self.village((144, 150, 170, 172))
        T.house(147, 154, 155, 160, 'S', roof='roof_thatch')
        T.house(158, 154, 167, 162, 'S', roof='roof_brown')
        self.pave((144, 162, 170, 165), 'dirt')
        self.fence(148, 166, 168, 172, gap=('N', 156, 160))
        for (hx, hz) in ((146, 163), (169, 163)):
            self.hay_bale(hx, hz)
        T.well(151, 164)
        self.person('ada', 152.5, 162.8, 180.0)
        T.sign(143.0, 163.0)
        for z in range(167, 171):   # the paddock floor
            for x in range(149, 167):
                self.override[(x, z)] = 'dirt'
        self.extra_areas.append((142, 148, 172, 174, "Sunfield Farm"))

        # ---- Northwatch: a ruined watchtower on the moor north of the castle
        self.village((104, 28, 118, 40))
        cx, cz = 110.5, 33.5
        T.cyl(cx, cz, 2.3, 1.9, 0, 5.5, 10, 'stone_dark')
        T.cyl(cx, cz, 2.0, 1.9, 5.4, 5.7, 10, 'stone')
        for k in range(10):
            a = 2 * math.pi * k / 10
            if k % 3 != 0:
                T.box(cx + math.cos(a) * 2.0 - 0.3, cz + math.sin(a) * 2.0 - 0.3, cx + math.cos(a) * 2.0 + 0.3, cz + math.sin(a) * 2.0 + 0.3, 5.6, 6.5, 'stone')
        for z in range(31, 37):
            for x in range(108, 114):
                T.grid[z][x] = 'o'
        for k in range(8):   # rubble
            a = k * 0.8
            T.blob(cx + math.cos(a) * 3.2, cz + math.sin(a) * 3.2, 0.0, 0.5, 0.4, 0.35, 'stone_dark', jitter=0.1, sub=1)
        T.sign(113.5, 40.5)
        self.person('watchman', 115.5, 36.5, 270.0)
        self.extra_areas.append((104, 26, 118, 42, 'Northwatch Tower'))

        # ---- a bandit camp deep in Darkwood
        self.village((150, 238, 168, 254))
        for (tx, tz, m) in ((153, 241, 'cloth_red'), (158, 240, 'cloth_green'), (163, 242, 'cloth_red'), (155, 249, 'cloth_green'), (162, 249, 'cloth_red')):
            self.tent(tx + 0.5, tz + 0.5, 2.0, 2.2, m)
        self.campfire(158, 245)
        for (bx, bz) in ((160, 246), (152, 246), (165, 246)):
            T.barrel(bx + 0.5, bz + 0.5)
        self.extra_areas.append((148, 236, 170, 256, "Bandits' Hollow"))

        # ---- Giant's Rise: a circle of standing stones
        self.village((150, 268, 168, 284))
        self.stones_circle(159.5, 276.5, 6.0, 9)
        self.stones_circle(159.5, 276.5, 3.0, 5, h=(0.9, 1.5))
        T.box(158.6, 275.6, 160.4, 277.4, 0, 0.7, 'stone')
        self.person('druid', 163.5, 276.5, 90.0)
        self.extra_areas.append((148, 266, 170, 286, 'The Standing Stones'))

        # ---- the Barrow Downs: grassy mounds with dark doors, a ring of stones
        for (bx, bz, r, h) in ((20, 320, 3.2, 2.4), (36, 314, 2.6, 2.0), (14, 342, 3.0, 2.2), (40, 336, 3.4, 2.6), (26, 352, 2.8, 2.1)):
            self.mound(bx, bz, r, h)
            for dz in range(-4, 5):
                for dx in range(-4, 5):
                    self.reserved.add((int(bx) + dx, int(bz) + dz))
        self.stones_circle(28.5, 330.5, 5.0, 7)
        # ---- the witch's hut in the swamp
        T.house(30, 262, 36, 267, 'S', roof='roof_thatch')
        T.cyl(33.0, 270.2, 0.5, 0.4, 0, 0.8, 8, 'iron')
        T.cyl(33.0, 270.2, 0.38, 0.38, 0.7, 0.78, 8, 'glow')
        T.grid[270][33] = 'o'
        self.person('maggie', 31.5, 269.5, 0.0)
        for dz in range(-2, 3):
            for dx in range(-2, 3):
                self.reserved.add((33 + dx, 266 + dz))
        self.extra_areas.append((26, 258, 42, 274, "The Witch's Hut"))
        # ---- the oasis in the Dune Sea: palms round the pool, a broken arch
        for k in range(9):
            a = k * 0.7 + 0.2
            x, z = int(158 + math.cos(a) * 9.0), int(340 + math.sin(a) * 6.5)
            if self.ok(x, z) and T.grid[z][x] == '.' and not WM.is_water(x, z):
                self.palm(x, z)
        # ---- the Iron Range mine: a timber frame over the path and the rocks themselves
        mx = 156
        for xx in (mx - 1.6, mx + 1.6):
            T.box(xx - 0.25, 300.0, xx + 0.25, 300.5, 0, 3.4, 'timber')
        T.box(mx - 2.0, 299.95, mx + 2.0, 300.55, 3.2, 3.6, 'wood_dark')
        T.grid[300][mx - 2] = 'o'
        T.grid[300][mx + 1] = 'o'
        self.extra_areas.append((146, 296, 168, 318, 'The Deep Mine'))
        # ---- the woodcutters' camp in Whispering Pines
        T.house(10, 56, 17, 62, 'E', roof='roof_brown')
        self.campfire(19, 62)
        for k in range(6):
            T.cyl(8.0 + k * 0.35, 64.0, 0.16, 0.16, 0, 0.55, 6, 'wood')
        self.village((6, 52, 24, 68))
        self.extra_areas.append((6, 52, 24, 68, "Woodcutters' Camp"))

    # ------------------------------------------------------------------------------------------ 3. ground, water, roads, bridges
    def ground_material(self, b, x, z):
        a, c = WM.GROUND[b]
        # slow noise decides which of the two the ground leans to, and a little per-tile hash frays the edges of the patches
        n = WM.vnoise(x, z, 5.0, 31) + ((x * 73856093 ^ z * 19349663) & 255) / 255.0 * 0.30 - 0.15
        m = c if n > 0.52 else a
        if m in ('grass_a', 'grass_b') and not self.B:
            n = (x * 73856093 ^ z * 19349663) & 7
            m = ('grass_a', 'grass_b', 'grass_c', 'grass_a', 'grass_b', 'grass_a', 'grass_c', 'grass_dark')[n]
        if self.B and m.startswith('grass'):
            m = 'grass_a'
        return m

    def ground(self):
        T, bt = self.T, self.bt
        P = bt.P
        OX, OZ, WW, HH = WM.OX, WM.OZ, WM.WW, WM.HH
        roads = {k: v for k, v in WM.road_tiles().items() if not any(WM.in_rect(k[0], k[1], r) for r in self.village_rects)}
        # road tiles never go over water (the bridges do that) and keep a clear margin free of scatter
        bridge = {}
        for (zc, w) in WM.BRIDGES:
            west, east = WM.river_edges(zc)
            for dz in range(-(w // 2), w // 2 + 1):
                for x in range(west - 1, east + 1):
                    bridge[(x, zc + dz)] = zc
        for (x, z) in roads:
            for dz in range(-1, 2):
                for dx in range(-1, 2):
                    self.reserved.add((x + dx, z + dz))
        for (x, z) in bridge:
            for dz in range(-2, 3):
                for dx in range(-2, 3):
                    self.reserved.add((x + dx, z + dz))
        self.roads, self.bridge = roads, bridge

        def wet(x, z):
            return self.ok(x, z) and T.grid[z][x] == '~'

        for z in range(HH):
            for x in range(WW):
                if WM.in_core(x, z):
                    continue
                if wet(x, z):
                    self.water_tile(x, z, wet)
                    continue
                b = self.bio(x, z)
                self.biome[(x, z)] = b
                mat = self.override.get((x, z)) or ('dirt' if (x, z) in roads else self.ground_material(b, x, z))
                T.quad(P(x, z + 1, 0), P(x + 1, z + 1, 0), P(x + 1, z, 0), P(x, z, 0), mat, (x + .5, z + .5))
        # shorelines: a sandy strip along the river's far bank
        # (the core's own banks already have theirs)
        self.link_roads_in_core()
        self.bridges()

    def water_tile(self, x, z, wet):
        T, bt = self.T, self.bt
        P = bt.P
        T.quad(P(x, z + 1, -0.35), P(x + 1, z + 1, -0.35), P(x + 1, z, -0.35), P(x, z, -0.35),
               'water' if self.B else ('water', 'water_b', 'water_c', 'water')[(x * 31 + z * 17) & 3], (x + .5, z + .5))
        # the edge down to the water: a low bank (mud round the swamp, sand round the sea and the oasis, earth elsewhere)
        for dx, dz, p0, p1 in ((1, 0, (x + 1, z + 1), (x + 1, z)), (0, 1, (x, z + 1), (x + 1, z + 1)),
                               (0, -1, (x + 1, z), (x, z)), (-1, 0, (x, z), (x, z + 1))):
            nx, nz = x + dx, z + dz
            if not self.ok(nx, nz) or wet(nx, nz):
                continue
            if self.B:   # a paler strip of shallow water / foam where it meets the land
                fw = 0.32
                if dx == 1:    q = (x + 1 - fw, z, x + 1, z + 1)
                elif dx == -1: q = (x, z, x + fw, z + 1)
                elif dz == 1:  q = (x, z + 1 - fw, x + 1, z + 1)
                else:          q = (x, z, x + 1, z + fw)
                T.quad(P(q[0], q[3], -0.338), P(q[2], q[3], -0.338), P(q[2], q[1], -0.338), P(q[0], q[1], -0.338), 'foam', (x + .5, z + .5))
            b = self.bio(nx, nz) if not WM.in_core(nx, nz) else 'meadow'
            if x >= WM.coast_x(z) - 8 or b in ('desert', 'beach'):
                face = 'sand'
            elif b in ('swamp', 'barrow'):
                face = 'mud'
            else:
                face = 'cliff' if (x + z) % 3 else 'cliff_dark'
            T.quad(P(p0[0], p0[1], -0.35), P(p1[0], p1[1], -0.35), P(p1[0], p1[1], 0), P(p0[0], p0[1], 0), face, (x + .5, z + .5))

    def link_roads_in_core(self):
        """Roads that cross the core's edge: lay a strip of earth over the core's own ground, clearing trees out of the way."""
        T, bt = self.T, self.bt
        P = bt.P
        cleared = 0
        for (x0, z0, x1, z1) in WM.CORE_LINKS:
            for z in range(z0, z1):
                for x in range(x0, x1):
                    ch = T.grid[z][x]
                    if ch == 'T':
                        T.grid[z][x] = '.'
                        T.trees = [t for t in T.trees if not (int(t[0]) == x and int(t[1]) == z)]
                        cleared += 1
                        ch = '.'
                    if ch in '.':
                        T.quad(P(x, z + 1, 0.012), P(x + 1, z + 1, 0.012), P(x + 1, z, 0.012), P(x, z, 0.012), 'dirt', (x + .5, z + .5))
                    elif ch != '~':
                        print('road link blocked at', x, z, repr(ch))
                    self.reserved.add((x, z))
        print('core links: %d trees cleared' % cleared)

    def bridges(self):
        T, bt = self.T, self.bt
        for (zc, w) in WM.BRIDGES:
            west, east = WM.river_edges(zc)
            z0, z1 = zc - w // 2, zc + w // 2 + 1
            for z in range(z0, z1):
                for x in range(west - 1, east + 1):
                    if T.grid[z][x] == '~':
                        T.grid[z][x] = '.'
            T.box(west - 1, z0, east + 1, z1, 0.0, 0.12, 'wood', c=((west + east) / 2, zc))
            T.box(west - 1, z0 - 0.02, east + 1, z0 + 0.12, 0.12, 0.30, 'wood_dark', c=((west + east) / 2, z0))        # the rails
            T.box(west - 1, z1 - 0.12, east + 1, z1 + 0.02, 0.12, 0.30, 'wood_dark', c=((west + east) / 2, z1))
            for x in range(west, east + 1, 2):                                                                          # piers and posts
                for zz in (z0 - 0.05, z1 + 0.05):
                    T.box(x - 0.1, zz - 0.1, x + 0.1, zz + 0.1, -0.35, 0.95, 'wood_dark', c=(x, zz))
                T.box(x - 0.16, z0, x + 0.16, z1, -0.2, 0.0, 'wood_dark', c=(x, zc))
            T.sign(west - 1.8, z1 + 0.9)
            T.sign(east + 2.2, z0 - 0.9)
            T.spots = [s for s in T.spots if not (west - 1 <= s[0] < east + 1 and z0 <= s[1] < z1)]

    # ------------------------------------------------------------------------------------------ 4. mountains and cliffs
    def solid_mountain(self, x, z):
        """Is this tile of the Frostpeak Range / the Iron Range / the black cliffs a wall nobody can climb?"""
        b = self.biome.get((x, z))
        if b is None:
            return False
        for r in WM.CLIFFS:
            if WM.in_rect(x, z, r):
                return True
        if b != 'mountain':
            return False
        if z < 5:
            return True
        n = WM.vnoise(x, z, 9.0, 5)
        if z < 17:
            return n > 0.26
        if any(WM.in_rect(x, z, r) for r in WM.MOUNTAIN_PASSES):
            return False
        return n > 0.52 and (x, z) not in self.reserved

    def mountains(self):
        T, bt = self.T, self.bt
        OX, OZ, WW, HH = WM.OX, WM.OZ, WM.WW, WM.HH
        pr = self.pr
        solid = set()
        for (x, z), b in self.biome.items():
            if self.solid_mountain(x, z) and T.grid[z][x] == '.':
                solid.add((x, z))
        # keep the roads open
        solid = {t for t in solid if t not in self.reserved}
        for (x, z) in solid:
            T.grid[z][x] = '#'
        self.solid = solid
        n = 0
        for (x, z) in sorted(solid):
            if x % 2 or z % 2:
                continue
            ash = self.biome.get((x, z)) == 'ash'
            snow = z < 30 and not ash
            ridge = WM.vnoise(x, z, 14.0, 6)
            h = (2.4 + 5.2 * ridge + pr.uniform(0, 1.0)) * (1.25 if z < 14 and not ash else 1.0)
            r = pr.uniform(2.8, 3.8)
            base = 'obsidian' if ash else ('rock_dark' if (x + z) % 3 else 'rock')
            bt_ = T
            cx, cz = x + 1.0 + pr.uniform(-0.3, 0.3), z + 1.0 + pr.uniform(-0.3, 0.3)
            bt_.cyl(cx, cz, r, 0.0, 0.0, h, 7, base, rot=pr.uniform(0, 1))     # broad, wide-based peaks (the first try, 4.7 tall for every 1 wide, were needles)
            if snow and h > 4.4:
                bt_.cyl(cx, cz, r * 0.55, 0.0, h * 0.6, h * 1.004, 7, 'snow')
            n += 1
        print('mountains: %d solid tiles, %d peaks' % (len(solid), n))

    # ------------------------------------------------------------------------------------------ 5. scatter
    SCATTER = {
        # biome: (tree chance a tile, [(kind, weight)], boulder chance, flower patch chance, bush chance)
        'meadow': (0.035, [('round', 5), ('tall', 2), ('pine', 1)], 0.003, 0.012, 0.004),
        'dry_meadow': (0.022, [('round', 3), ('tall', 1), ('oak', 1)], 0.010, 0.004, 0.003),
        'pine': (0.150, [('pine', 8), ('tall', 1), ('yew', 1)], 0.005, 0.0, 0.0),
        'snow': (0.070, [('pine', 9), ('yew', 1)], 0.014, 0.0, 0.0),
        'forest': (0.200, [('round', 5), ('oak', 4), ('tall', 2), ('willow', 1), ('maple', 1)], 0.003, 0.008, 0.008),
        'darkforest': (0.250, [('yew', 5), ('pine', 4), ('oak', 2), ('mahogany', 1), ('teak', 1)], 0.005, 0.0, 0.004),
        'farm': (0.008, [('oak', 3), ('round', 2)], 0.002, 0.020, 0.003),
        'swamp': (0.050, [('willow', 6), ('yew', 1)], 0.004, 0.0, 0.0),
        'barrow': (0.014, [('pine', 1)], 0.030, 0.0, 0.0),
        'moor': (0.012, [('pine', 2), ('yew', 1)], 0.040, 0.0, 0.0),
        'desert': (0.0, [], 0.014, 0.0, 0.0),
        'beach': (0.0, [], 0.004, 0.0, 0.0),
        'mountain': (0.01, [('pine', 1)], 0.04, 0.0, 0.0),
    }

    def scatter(self):
        T, bt = self.T, self.bt
        pr = self.pr
        vr = T.vrng
        placed_t = 0
        counts = {}
        for z in range(WM.HH):
            for x in range(WM.WW):
                if WM.in_core(x, z) or not self.free(x, z):
                    continue
                b = self.biome.get((x, z))
                rule = self.SCATTER.get(b)
                if not rule:
                    continue
                tree_p, kinds, rock_p, flower_p, bush_p = rule
                r = pr.random()
                if r < tree_p and kinds and not self.tree_near(x, z):
                    kind = self.pick(kinds)
                    if b == 'forest' and kind == 'willow' and not self.near_water(x, z, 6):
                        kind = 'round'
                    self.tree_at(x, z, kind)
                    placed_t += 1
                    counts[kind] = counts.get(kind, 0) + 1
                    continue
                if r < tree_p + rock_p:
                    self.rock_at(x, z, pr.uniform(0.7, 1.4), 'rock_dark' if b in ('barrow', 'moor', 'mountain', 'snow') else 'rock')
                elif r < tree_p + rock_p + flower_p:
                    T.flower_patch(x + .5, z + .5, pr.randint(4, 8), 0.7)
                elif r < tree_p + rock_p + flower_p + bush_p:
                    T.blob(x + 0.5, z + 0.5, 0.3, 0.5, 0.5, 0.4, 'hedge', jitter=0.07, sub=1)
                    T.grid[z][x] = 'o'
                else:
                    self.extras(b, x, z, r)
                # grass tufts on grass-like ground
                if self.B and T.grid[z][x] == '.' and b in ('meadow', 'dry_meadow', 'pine', 'forest', 'darkforest', 'farm', 'swamp', 'moor') and \
                        (x, z) not in self.roads and vr.random() < (0.45 if b in ('meadow', 'forest', 'farm') else 0.28):
                    T.grass_tuft(x + vr.uniform(0.1, 0.9), z + vr.uniform(0.1, 0.9), 0.34, 3)
        print('trees placed:', placed_t, counts)

    def pick(self, kinds):
        total = sum(w for _, w in kinds)
        r = self.pr.uniform(0, total)
        for k, w in kinds:
            if r < w:
                return k
            r -= w
        return kinds[0][0]

    def tree_near(self, x, z):
        g = self.T.grid
        for dz in (-1, 0, 1):
            for dx in (-1, 0, 1):
                if self.ok(x + dx, z + dz) and g[z + dz][x + dx] == 'T':
                    return True
        return False

    def near_water(self, x, z, r):
        for dz in range(-r, r + 1, 2):
            for dx in range(-r, r + 1, 2):
                if self.ok(x + dx, z + dz) and self.T.grid[z + dz][x + dx] == '~':
                    return True
        return False

    def extras(self, b, x, z, r):
        """Odds and ends that make a biome look like itself."""
        T = self.T
        pr = self.pr
        if b == 'desert' and r > 0.986:
            self.cactus(x, z)
        elif b == 'swamp' and r > 0.992:
            self.dead_tree(x, z)
        elif b in ('barrow', 'moor') and r > 0.996:
            self.dead_tree(x, z)
        elif b == 'barrow' and r > 0.985:
            self.gravestone(x, z)
        elif b == 'farm' and r > 0.993:
            self.hay_bale(x, z)

    def cactus(self, x, z):
        T = self.T
        cx, cz = x + 0.5, z + 0.5
        h = self.pr.uniform(1.0, 1.9)
        T.cyl(cx, cz, 0.17, 0.15, 0, h, 6, 'cactus')
        if self.pr.random() < 0.7:
            T.cyl(cx + 0.28, cz, 0.08, 0.07, h * 0.45, h * 0.8, 5, 'cactus')
            T.box(cx + 0.02, cz - 0.07, cx + 0.3, cz + 0.07, h * 0.4, h * 0.48, 'cactus')
        T.grid[z][x] = 'o'

    def dead_tree(self, x, z):
        T = self.T
        cx, cz = x + 0.5, z + 0.5
        T.cyl(cx, cz, 0.16, 0.07, 0, 2.2, 5, 'trunk')
        T.obox((cx, cz, 1.4), (cx + 0.8, cz + 0.2, 2.2), 0.08, 0.08, 'trunk')
        T.obox((cx, cz, 1.0), (cx - 0.7, cz - 0.3, 1.8), 0.07, 0.07, 'trunk')
        T.grid[z][x] = 'o'

    def gravestone(self, x, z):
        T = self.T
        cx, cz = x + 0.5, z + 0.5
        T.box(cx - 0.28, cz - 0.07, cx + 0.28, cz + 0.07, 0, 0.85, 'stone')
        T.box(cx - 0.24, cz - 0.07, cx + 0.24, cz + 0.07, 0.85, 0.98, 'stone_light')
        T.grid[z][x] = 'o'

    def hay_bale(self, x, z):
        T = self.T
        T.box(x + 0.15, z + 0.2, x + 0.85, z + 0.8, 0, 0.6, 'straw')
        T.box(x + 0.13, z + 0.45, x + 0.87, z + 0.55, 0.1, 0.5, 'wood')
        T.grid[z][x] = 'o'

    # ------------------------------------------------------------------------------------------ 6. monsters and resources
    def populate_regions(self):
        T = self.T
        pr = self.pr
        self.reach = self.flood()
        # (pen name, rect, [(species, count)]): the creatures roam the open ground inside the rectangle
        PENS = [
            ('eastgoblins', (140, 58, 176, 78), [('goblin', 5), ('goblin_warrior', 2), ('hobgoblin', 2)]),
            ('eastdowns', (130, 100, 176, 114), [('giant_rat', 3), ('goblin', 2)]),
            ('northreach', (60, 18, 128, 46), [('wolf', 6), ('bandit', 4), ('dark_wolf', 2)]),
            ('northpeaks', (128, 18, 192, 46), [('dark_wolf', 4), ('ogre', 2), ('hill_giant', 2), ('barbarian', 2)]),
            ('northwest', (2, 18, 60, 50), [('dark_wolf', 3), ('hill_giant', 2), ('bandit', 3)]),
            ('pines', (4, 70, 54, 110), [('wolf', 6), ('bandit', 3), ('giant_spider', 3)]),
            ('elmwood', (4, 114, 54, 170), [('hobgoblin', 4), ('bandit', 3), ('goblin', 4), ('giant_rat', 3)]),
            ('greenmarch', (4, 174, 54, 224), [('giant_rat', 4), ('giant_mole', 3), ('goblin_warrior', 3)]),
            ('sunfields', (130, 116, 178, 148), [('giant_rat', 3), ('giant_mole', 2), ('goblin', 3)]),
            ('sunfields2', (130, 176, 178, 224), [('giant_rat', 3), ('giant_mole', 3), ('hobgoblin', 2)]),
            ('mirewood', (4, 226, 54, 300), [('giant_rat', 5), ('zombie', 5), ('imp', 3), ('giant_spider', 3), ('hobgoblin', 2)]),
            ('barrows', (4, 304, 54, 366), [('skeleton', 5), ('zombie', 4), ('skeleton_warrior', 3), ('dark_wizard', 2), ('imp', 2), ('black_knight', 1)]),
            ('darkwood', (130, 228, 190, 264), [('wolf', 5), ('dark_wolf', 3), ('giant_spider', 3), ('hobgoblin', 2)]),
            ('giants', (130, 268, 190, 290), [('hill_giant', 3), ('ogre', 3), ('barbarian', 3), ('mountain_troll', 2)]),
            ('ironrange', (140, 296, 176, 326), [('cave_spider', 4), ('giant_mole', 3), ('mountain_troll', 2)]),
            ('dunes', (130, 326, 190, 370), [('jackal', 6), ('scorpion', 5), ('desert_mummy', 3)]),
        ]
        # mineable rocks where the country has them
        def rocks(rect, kinds):
            tiles = self.tiles(rect)
            pr.shuffle(tiles)
            used = []
            for kind in kinds:
                for (x, z) in tiles:
                    if all(abs(x - ux) + abs(z - uz) >= 2 for ux, uz in used):
                        used.append((x, z))
                        self.far_rock(kind, x, z)
                        break
        rocks((148, 302, 166, 318), ['iron', 'iron', 'iron', 'coal', 'coal', 'coal', 'coal', 'mithril', 'mithril', 'gems', 'clay'])
        rocks((146, 268, 172, 286), ['silver', 'silver', 'gold', 'gold', 'gems', 'clay', 'coal'])
        rocks((10, 304, 50, 360), ['coal', 'coal', 'tin', 'copper', 'clay', 'iron'])
        rocks((136, 326, 180, 366), ['gold', 'gold', 'gems', 'silver', 'clay'])
        rocks((6, 236, 50, 296), ['clay', 'clay', 'copper', 'tin'])
        self.reach = self.flood()   # the rocks may have closed off a corner
        for name, rect, counts in PENS:
            self.pen(name, rect, counts)
        self.pen('hollow', (150, 238, 168, 254), [('bandit', 5)], in_village=True)
        # the paddock at Sunfield Farm
        T.pens.append(('w_sunfarm', 149, 167, 167, 171))
        taken = []
        tiles = [(x, z) for z in range(167, 171) for x in range(149, 167) if T.grid[z][x] == '.']
        for species, count in (('cow', 3), ('calf', 2), ('chicken', 5)):
            for _ in range(count):
                for _t in range(100):
                    x, z = pr.choice(tiles)
                    if all(abs(x - tx) + abs(z - tz) >= 2 for tx, tz in taken):
                        taken.append((x, z))
                        T.npcs.append((species, x + pr.uniform(0.3, 0.7), z + pr.uniform(0.3, 0.7), 'w_sunfarm'))
                        break
        # river fishing on the west bank (the wings have a river too)
        for zz in (62, 74, 112, 126, 150, 166, 206, 222):
            west, east = WM.river_edges(zz)
            sx, sz = west, zz
            if self.ok(sx, sz) and T.grid[sz][sx] == '~' and (sx - 1, sz) in self.reach:
                T.spots.append((sx + 0.5, sz + 0.5, 'net'))
        # the pools of the swamp and the oasis have fish too
        for (cx, cz, rx, rz) in WM.PONDS:
            sx, sz = int(cx - rx), int(cz)
            if self.ok(sx, sz) and T.grid[sz][sx] == '~' and (sx - 1, sz) in self.reach:
                T.spots.append((sx + 0.5, sz + 0.5, 'rod' if cx < 100 else 'net'))

    # ------------------------------------------------------------------------------------------ 7. the skirt: far ground, the sea and river beyond the edge, a ring of forest
    def skirt(self):
        T, bt = self.T, self.bt
        P = bt.P
        WW, HH = WM.WW, WM.HH
        S = 48
        sea0, sea1 = WM.SEA_Z[0] + 8, WM.SEA_Z[1] - 8
        w_n, e_n = WM.river_edges(0)
        w_s, e_s = WM.river_edges(HH - 1)

        def q(a, b, c, d, mat):   # x from a to c, z from b to d
            T.quad(P(a, d, 0), P(c, d, 0), P(c, b, 0), P(a, b, 0), mat, ((a + c) / 2, (b + d) / 2))

        def qw(a, b, c, d):
            T.quad(P(a, d, -0.35), P(c, d, -0.35), P(c, b, -0.35), P(a, b, -0.35), 'water', ((a + c) / 2, (b + d) / 2))

        # north: mountains, south: black rock, west: forest, east: forest and the sea
        q(-S, -S, WW + S, 0, 'highland')
        q(-S, HH, WW + S, HH + S, 'abyss')
        q(-S, 0, 0, HH, 'grass_dark')
        q(WW, 0, WW + S, sea0, 'grass_dark')
        q(WW, sea1, WW + S, HH, 'grass_dark')
        qw(WW, sea0, WW + S, sea1)
        # the river runs out of the map to the north and the south
        qw(w_n, -S, e_n, 0)
        qw(w_s, HH, e_s, HH + S)
        # a far ring of trees: only on the land
        n = 0
        for _ in range(300):
            x = self.pr.uniform(-S + 4, WW + S - 4)
            z = self.pr.uniform(-S + 4, HH + S - 4)
            if -2 <= x <= WW + 2 and -2 <= z <= HH + 2:
                continue
            if z < 0 or z > HH:
                continue
            if x > WW and sea0 - 2 <= z <= sea1 + 2:
                continue
            T.tree(x, z, lod=1)
            n += 1
        print('skirt trees:', n)

    # ------------------------------------------------------------------------------------------ report
    def report(self):
        T = self.T
        WW, HH = WM.WW, WM.HH
        # flood fill from the spawn: what can the hero reach?
        sx, sz = 32 + WM.OX, 30 + WM.OZ
        seen = {(sx, sz)}
        stack = [(sx, sz)]
        while stack:
            x, z = stack.pop()
            for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, nz = x + dx, z + dz
                if 0 <= nx < WW and 0 <= nz < HH and (nx, nz) not in seen and T.grid[nz][nx] in '.D':
                    seen.add((nx, nz))
                    stack.append((nx, nz))
        sealed = 0
        for z in range(HH):
            for x in range(WW):
                if T.grid[z][x] in '.D' and (x, z) not in seen:
                    T.grid[z][x] = 'o'
                    sealed += 1
        print('sealed pockets nobody can reach: %d tiles' % sealed)
        walk = sum(1 for r in T.grid for c in r if c in '.D')
        print('walkable tiles: %d, reachable from the spawn: %d (%.0f%%)' % (walk, len(seen), 100.0 * len(seen) / max(1, walk)))
        for (x, z, kind) in T.spots:
            pass
        lost = [(k, x, z) for (k, x, z, _) in T.persons if (int(x), int(z)) not in seen and not any(((int(x) + dx), (int(z) + dz)) in seen for dx in (-2, -1, 0, 1, 2) for dz in (-2, -1, 0, 1, 2))]   # shopkeepers talk across a counter (reach 2)

        if lost:
            print('PEOPLE THE HERO CANNOT REACH:', lost)
        self.seen = seen
        print('trees %d, rocks %d, pens %d, creatures %d, people %d, spots %d' % (len(T.trees), len(T.rocks), len(T.pens), len(T.npcs), len(T.persons), len(T.spots)))
