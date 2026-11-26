#!/usr/bin/env python3
"""
Journey - Castle Town builder (Blender / bpy)

One layout drives BOTH the art and the collision:
  * assets/models/town/chunk_X_Z.glb   16x16-tile model chunks (culled by the game)
  * assets/models/town/roof_<name>.glb  roofs of enterable buildings (hidden when the player is inside)
  * assets/models/town/skirt.glb        far-away ground + forest ring
  * assets/maps/town.map                tile collision grid + spawn + roof rectangles

Run:   python3 tools/build_town.py            (needs `pip install bpy`)
Coordinates: tile units. x grows east, z grows south. 1 tile = 1 metre-ish. Height is "h".
Exported glTF is Y-up, so game world == (tile x, height, tile z).
"""
import bpy, bmesh, math, random, os, sys
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import worldmap as WM

WORLD = True     # the hand-built strip (the "core") sits in the middle of a bigger world: see worldmap.py and world_wilds.py

W = 64
HT = 64    # the castle town is the top 64 rows of the map (its random layout is unchanged)
H = 336   # the whole map: the town, the road south, Millbrook, the lake, the mountains, the caves, the desert and the Abyss
CH = 16  # chunk size in tiles
NCX = (W + CH - 1) // CH
NCZ = (H + CH - 1) // CH
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
STYLE = 'a'
if '--style' in sys.argv:
    STYLE = sys.argv[sys.argv.index('--style') + 1].lower()
B = (STYLE == 'b')                      # style B = style A + textures, fuller foliage, bevels, contact AO
OUT_MODELS = os.path.join(ROOT, 'assets', 'models', 'town_b' if B else 'town')
OUT_MAP = os.path.join(ROOT, 'assets', 'maps', 'town.map')

# ----------------------------------------------------------------------------------------------
# Palette: modernised Classic. Warmer, richer, slightly desaturated greens, same chunky shapes.
# Written as sRGB hex (what you see); converted to linear for glTF. The game shader does the
# gamma-correct lighting, so what you pick here is what you get.
# ----------------------------------------------------------------------------------------------
PAL = {
    'grass_a': '#5da13a', 'grass_b': '#54973a', 'grass_c': '#67ab40', 'grass_dark': '#3f7a31',
    'dirt': '#8a6a45', 'sand': '#d6c58f',
    'cobble_a': '#9b9a95', 'cobble_b': '#8b8a86', 'plaza_a': '#cbc2ae', 'plaza_b': '#b8af9b',
    'water': '#2f86c9', 'water_b': '#2c80c3', 'water_c': '#3490d0', 'water_deep': '#1d5f9e', 'cliff': '#8c5b33', 'cliff_dark': '#6e4527',
    'rock': '#8e9095', 'rock_dark': '#6e7076',
    'stone': '#a5a49f', 'stone_dark': '#7c7b78', 'stone_light': '#c1bfb7',
    'floor_a': '#b9b3a5', 'floor_b': '#a8a294', 'wood_floor': '#b8885c',
    'plaster': '#f0e7d1', 'timber': '#4a2e1c', 'wood': '#9c6c3e', 'wood_dark': '#66401f',
    'roof_red': '#a53b2c', 'roof_brown': '#704b2e', 'roof_slate': '#4d586b', 'roof_thatch': '#c9a64f',
    'roof_blue': '#375f9e', 'glass': '#6aa4cf', 'carpet': '#a31e2a', 'gold': '#e6b73a',
    'banner_blue': '#2a4aa8', 'banner_red': '#b8262f', 'iron': '#3a3c43',
    'leaf_a': '#3f9030', 'leaf_b': '#52a63a', 'leaf_c': '#2e7b2c', 'leaf_pine': '#2a6a35', 'leaf_maple': '#c8602a', 'leaf_magic': '#7a52c8',
    'trunk': '#6b4527', 'flower_y': '#f5d33f', 'flower_r': '#e2473d', 'flower_w': '#f5f2ea',
    'flower_p': '#a65ec4', 'cloth_white': '#f1ece0', 'cloth_red': '#c4372f', 'cloth_green': '#3f8050',
    'glow': '#ffd37a', 'straw': '#d9bb62', 'hedge': '#2d6b2a', 'skin': '#e0b48a',
    'tuft': '#78b648', 'foam': '#d3e8ee',
    'highland': '#7d7565', 'highland_b': '#716a5c', 'cave': '#4b453f', 'cave_b': '#423c37', 'abyss': '#3a2f48', 'abyss_b': '#31283d',
    'obsidian': '#2a2133', 'lava': '#ff5a22', 'bone': '#e9dfc6',
    # round 12: the biomes of the wider world
    'snow': '#d2dce2', 'moss': '#587a45', 'mud': '#5b4a36', 'wheat': '#c9ad55', 'dry_grass': '#9a9b4c', 'red_sand': '#c98f5a',
    'reed': '#7e9a4a', 'cactus': '#4f8f48',
}

# Style B: material -> (procedural texture, tile-units per texture repeat). Colours come from PAL (texture is tinted to match).
TEXMAP = {
    'grass_a': ('grass', 3), 'grass_b': ('grass', 3), 'grass_c': ('grass', 3), 'grass_dark': ('grass', 3),
    'dirt': ('dirt', 3), 'sand': ('sand', 3),
    'cobble_a': ('cobble', 4), 'cobble_b': ('cobble', 4), 'plaza_a': ('flagstone', 4), 'plaza_b': ('flagstone', 4),
    'floor_a': ('flagstone', 4), 'floor_b': ('flagstone', 4), 'wood_floor': ('planks', 2.2),
    'water': ('water', 6), 'water_b': ('water', 6), 'water_c': ('water', 6), 'water_deep': ('water', 6),
    'cliff': ('rock', 3), 'cliff_dark': ('rock', 3), 'rock': ('rock', 2.5), 'rock_dark': ('rock', 2.5),
    'stone': ('stone_bricks', 4), 'stone_dark': ('stone_bricks', 4), 'stone_light': ('stone_bricks', 4),
    'plaster': ('plaster', 3), 'wood': ('planks', 1.6), 'wood_dark': ('planks', 1.6), 'timber': ('planks', 1.4),
    'roof_red': ('roof_tiles', 2.4), 'roof_brown': ('roof_tiles', 2.4), 'roof_blue': ('slate_tiles', 2.4),
    'roof_slate': ('slate_tiles', 2.4), 'roof_thatch': ('thatch', 2.0),
    'leaf_a': ('leaves', 2.6), 'leaf_b': ('leaves', 2.6), 'leaf_c': ('leaves', 2.6), 'leaf_pine': ('leaves', 2.6),
    'leaf_maple': ('leaves', 2.6), 'leaf_magic': ('leaves', 2.6),
    'hedge': ('leaves', 2.6), 'trunk': ('bark', 1.4), 'carpet': ('carpet', 1.0),
    'highland': ('dirt', 3), 'highland_b': ('dirt', 3), 'cave': ('rock', 3), 'cave_b': ('rock', 3), 'abyss': ('dirt', 3), 'abyss_b': ('dirt', 3),
    'obsidian': ('stone_bricks', 4),
    'snow': ('sand', 3), 'moss': ('grass', 3), 'mud': ('dirt', 3), 'wheat': ('grass', 3), 'dry_grass': ('grass', 3), 'red_sand': ('sand', 3),
}
GROUND_MATS = {'tuft', 'foam', 'grass_a', 'grass_b', 'grass_c', 'grass_dark', 'dirt', 'sand', 'cobble_a', 'cobble_b',
               'plaza_a', 'plaza_b', 'water', 'water_b', 'water_c', 'water_deep', 'floor_a', 'floor_b', 'wood_floor',
               'highland', 'highland_b', 'cave', 'cave_b', 'abyss', 'abyss_b', 'snow', 'moss', 'mud', 'wheat', 'dry_grass', 'red_sand'}


def s2l(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hexlin(h):
    h = h.lstrip('#')
    return tuple(s2l(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (1.0,)


def P(x, z, h):
    """tile coords -> Blender coords (Blender -Y == game +Z)."""
    return Vector((x, -z, h))


def lt(axis, u, v, h):
    return (u, v, h) if axis == 'x' else (v, u, h)


def lp(axis, u, v, h):
    return P(*lt(axis, u, v, h))


class Town:
    def __init__(s):
        s.grid = [['.' for _ in range(W)] for _ in range(H)]
        s.bms = {}
        s.mats = {}
        s.matlist = []
        s.rng = random.Random(1337)   # layout placement only
        s.vrng = random.Random(4242)  # visual-only randomness (tree shapes, flowers) - same in A and B
        s.floors = []         # (rect, matA, matB)
        s.roof_rects = {}     # name -> rect
        s.target = None       # override bmesh key (e.g. 'roof_bank')
        s.occupied = set()    # tiles reserved for trees/props placement
        s.bvh = None
        s.trees = []          # choppable trees: (x, z, kind, scale, rot) - drawn by the game at runtime, not baked
        s.spots = []          # fishing spots: (x, z, kind) on water tiles next to the bank - the game draws the ripples
        s.pens = []           # fenced animal areas: (name, x0, z0, x1, z1) interior tiles, x1/z1 exclusive
        s.npcs = []           # animals: (species, x, z, pen)
        s.rocks = []          # mineable rocks: (x, z, kind, yaw) - the game draws them (full or mined out)
        s.stations = []       # smithing workstations: (kind, x, z, yaw) - 'furnace' / 'anvil', drawn by the game
        s.persons = []        # people: (key, x, z, yaw) - shopkeepers, smiths, villagers (looks and lines are in game/People.cpp)

    # ---------------------------------------------------------------- bookkeeping
    def mi(s, name):
        if name not in s.mats:
            s.mats[name] = len(s.matlist)
            s.matlist.append(name)
        return s.mats[name]

    def bm_for(s, c):
        if s.target:
            key = s.target
        elif c[0] < -0.5 or c[0] > W + 0.5 or c[1] < -0.5 or c[1] > H + 0.5:
            key = ('skirt_%d' % ((c[0] > W / 2) + 2 * (c[1] > H / 2))) if B else 'skirt'
        else:
            key = (min(NCX - 1, max(0, int(c[0] // CH))), min(NCZ - 1, max(0, int(c[1] // CH))))
        if key not in s.bms:
            bm = bmesh.new()
            bm.loops.layers.color.new('Col')
            s.bms[key] = bm
        return s.bms[key]

    def mark(s, x0, z0, x1, z1, ch):
        for z in range(int(math.floor(z0)), int(math.ceil(z1))):
            for x in range(int(math.floor(x0)), int(math.ceil(x1))):
                if 0 <= x < W and 0 <= z < H:
                    s.grid[z][x] = ch
                    s.occupied.add((x, z))

    def floor(s, rect, a, b=None):
        s.floors.append((rect, a, b or a))

    # ---------------------------------------------------------------- primitives
    def solid(s, pts, faces, mat, c, bevel=0.0):
        bm = s.bm_for(c)
        vs = [bm.verts.new(p) for p in pts]
        fs = []
        for f in faces:
            try:
                fc = bm.faces.new([vs[i] for i in f])
            except ValueError:
                continue
            fc.material_index = s.mi(mat)
            fc.smooth = False
            fs.append(fc)
        bmesh.ops.recalc_face_normals(bm, faces=fs)
        col = bm.loops.layers.color.get('Col')
        for fc in fs:
            for l in fc.loops:
                l[col] = (1.0, 1.0, 1.0, 1.0)
        if B and bevel > 0.0 and fs:
            edges = list({e for fc in fs for e in fc.edges if len(e.link_faces) == 2})
            if edges:
                res = bmesh.ops.bevel(bm, geom=edges, offset=bevel, offset_type='OFFSET', segments=1, affect='EDGES')
                col = bm.loops.layers.color.get('Col')
                mi = s.mi(mat)
                for fc in res['faces']:      # chamfer faces would otherwise default to material 0
                    fc.material_index = mi
                    fc.smooth = False
                    for l in fc.loops:
                        l[col] = (1.0, 1.0, 1.0, 1.0)
        return fs

    def quad(s, p0, p1, p2, p3, mat, c):
        bm = s.bm_for(c)
        vs = [bm.verts.new(p) for p in (p0, p1, p2, p3)]
        fc = bm.faces.new(vs)
        fc.material_index = s.mi(mat)
        fc.smooth = False
        return fc

    def box(s, x0, z0, x1, z1, h0, h1, mat, c=None):
        xa, xb = sorted((x0, x1)); za, zb = sorted((z0, z1))
        pts = [P(xa, za, h0), P(xb, za, h0), P(xb, zb, h0), P(xa, zb, h0),
               P(xa, za, h1), P(xb, za, h1), P(xb, zb, h1), P(xa, zb, h1)]
        faces = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        mn = min(xb - xa, zb - za, h1 - h0)
        bv = min(0.035, 0.22 * mn) if mn > 0.12 else 0.0
        s.solid(pts, faces, mat, c or ((xa + xb) / 2, (za + zb) / 2), bevel=bv)

    def obox(s, a, b, w, t, mat, upref=(0, 0, 1)):
        A, B = P(*a), P(*b)
        d = B - A
        if d.length < 1e-6:
            return
        dn = d.normalized()
        side = dn.cross(Vector(upref))
        if side.length < 1e-4:
            side = Vector((1, 0, 0))
        side.normalize()
        up2 = side.cross(dn).normalized()
        pts = []
        for E in (A, B):
            for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                pts.append(E + side * (w / 2 * sx) + up2 * (t / 2 * sz))
        faces = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        mn = min(w, t)
        s.solid(pts, faces, mat, ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2), bevel=(min(0.025, 0.22 * mn) if mn > 0.12 else 0.0))

    def cyl(s, cx, cz, r0, r1, h0, h1, n, mat, rot=0.0, sx=1.0, sz=1.0):
        pts, faces = [], []
        for i in range(n):
            a = rot + 2 * math.pi * i / n
            pts.append(P(cx + math.cos(a) * r0 * sx, cz + math.sin(a) * r0 * sz, h0))
        if r1 > 0:
            for i in range(n):
                a = rot + 2 * math.pi * i / n
                pts.append(P(cx + math.cos(a) * r1 * sx, cz + math.sin(a) * r1 * sz, h1))
            for i in range(n):
                faces.append((i, (i + 1) % n, n + (i + 1) % n, n + i))
            faces.append(tuple(range(n)))
            faces.append(tuple(range(n, 2 * n)))
        else:
            pts.append(P(cx, cz, h1))
            for i in range(n):
                faces.append((i, (i + 1) % n, n))
            faces.append(tuple(range(n)))
        s.solid(pts, faces, mat, (cx, cz))

    def blob(s, cx, cz, h, rx, rz, rh, mat, jitter=0.12, sub=1, shade=None):
        """Lumpy ellipsoid. shade=(h0,h1): vertex colour ramps dark->light between those absolute heights."""
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=sub, radius=1.0)
        vr = s.vrng
        pts = [Vector(((v.co.x * rx) + vr.uniform(-jitter, jitter),
                       (v.co.y * rz) + vr.uniform(-jitter, jitter),
                       (v.co.z * rh) + vr.uniform(-jitter, jitter) * 0.6)) for v in tmp.verts]
        idx = {v.index: i for i, v in enumerate(tmp.verts)}
        faces = [tuple(idx[v.index] for v in f.verts) for f in tmp.faces]
        tmp.free()
        pts = [P(cx + p.x, cz + p.y, h + p.z) for p in pts]
        fs = s.solid(pts, faces, mat, (cx, cz))
        if shade and fs:
            col = s.bm_for((cx, cz)).loops.layers.color.get('Col')
            h0, h1 = shade
            for fc in fs:
                for l in fc.loops:
                    t = max(0.0, min(1.0, (l.vert.co.z - h0) / max(1e-3, h1 - h0)))
                    k = 0.52 + 0.48 * t * t * (3 - 2 * t)
                    l[col] = (k, k, k, 1.0)
        return fs

    def slab(s, axis, u0, u1, va, ha, vb, hb, t, mat):
        dv, dh = vb - va, hb - ha
        L = math.hypot(dv, dh)
        n = (-dh / L, dv / L) if dv > 0 else (dh / L, -dv / L)
        if n[1] < 0:
            n = (-n[0], -n[1])
        pts = []
        for u in (u0, u1):
            pts += [lp(axis, u, va, ha), lp(axis, u, vb, hb),
                    lp(axis, u, vb - n[0] * t, hb - n[1] * t), lp(axis, u, va - n[0] * t, ha - n[1] * t)]
        faces = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        c = lt(axis, (u0 + u1) / 2, (va + vb) / 2, 0)
        s.solid(pts, faces, mat, (c[0], c[1]))

    def tri_prism(s, axis, u0, u1, v0, v1, h0, h1, mat):
        vm = (v0 + v1) / 2
        pts = []
        for u in (u0, u1):
            pts += [lp(axis, u, v0, h0), lp(axis, u, v1, h0), lp(axis, u, vm, h1)]
        faces = [(0, 1, 2), (3, 4, 5), (0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5)]
        c = lt(axis, (u0 + u1) / 2, vm, 0)
        s.solid(pts, faces, mat, (c[0], c[1]))

    # ---------------------------------------------------------------- roofs
    def gable_roof(s, x0, z0, x1, z1, he, rise, ov, mat, axis=None, gable_mat='plaster', fill=True, trim='timber'):
        if axis is None:
            axis = 'x' if (x1 - x0) >= (z1 - z0) else 'z'
        u0, u1, v0, v1 = (x0, x1, z0, z1) if axis == 'x' else (z0, z1, x0, x1)
        vm = (v0 + v1) / 2
        drop = 0.28
        s.slab(axis, u0 - ov, u1 + ov, v0 - ov, he - drop, vm, he + rise, 0.17, mat)
        s.slab(axis, u0 - ov, u1 + ov, v1 + ov, he - drop, vm, he + rise, 0.17, mat)
        s.obox(lt(axis, u0 - ov, vm, he + rise + 0.03), lt(axis, u1 + ov, vm, he + rise + 0.03), 0.34, 0.16, trim)
        for v in (v0 - ov, v1 + ov):
            s.obox(lt(axis, u0 - ov, v, he - drop + 0.02), lt(axis, u1 + ov, v, he - drop + 0.02), 0.12, 0.2, trim)
        if fill:
            for (ua, ub) in ((u0 + 0.02, u0 + 0.4), (u1 - 0.4, u1 - 0.02)):
                s.tri_prism(axis, ua, ub, v0 + 0.02, v1 - 0.02, he - 0.02, he + rise - 0.08, gable_mat)
            # gable timber
            for u in (u0 - 0.03, u1 + 0.03):
                s.obox(lt(axis, u, vm, he), lt(axis, u, vm, he + rise - 0.2), 0.12, 0.12, trim)
                s.obox(lt(axis, u, v0 + 0.2, he + 0.05), lt(axis, u, vm, he + rise - 0.25), 0.1, 0.12, trim)
                s.obox(lt(axis, u, v1 - 0.2, he + 0.05), lt(axis, u, vm, he + rise - 0.25), 0.1, 0.12, trim)
        return axis

    # ---------------------------------------------------------------- wall decor
    def side_box(s, side, rect, a0, a1, h0, h1, prot, mat):
        x0, z0, x1, z1 = rect
        e = 0.03
        if side == 'N':   s.box(a0, z0 - prot, a1, z0 + e, h0, h1, mat)
        elif side == 'S': s.box(a0, z1 - e, a1, z1 + prot, h0, h1, mat)
        elif side == 'W': s.box(x0 - prot, a0, x0 + e, a1, h0, h1, mat)
        else:             s.box(x1 - e, a0, x1 + prot, a1, h0, h1, mat)

    def wp(s, side, rect, a, h, prot=0.0):
        x0, z0, x1, z1 = rect
        if side == 'N': return (a, z0 - prot, h)
        if side == 'S': return (a, z1 + prot, h)
        if side == 'W': return (x0 - prot, a, h)
        return (x1 + prot, a, h)

    def wall_span(s, side, rect):
        x0, z0, x1, z1 = rect
        return (x0, x1) if side in 'NS' else (z0, z1)

    def timber(s, side, rect, h0, h1, step=1.15, braces=True, mid=None):
        a0, a1 = s.wall_span(side, rect)
        n = max(1, int(round((a1 - a0) / step)))
        xs = [a0 + (a1 - a0) * i / n for i in range(n + 1)]
        for i, a in enumerate(xs):
            hh = 0.2 if i in (0, n) else 0.14
            s.side_box(side, rect, a - hh / 2, a + hh / 2, h0, h1, 0.07, 'timber')
        s.side_box(side, rect, a0, a1, h0, h0 + 0.16, 0.08, 'timber')
        s.side_box(side, rect, a0, a1, h1 - 0.2, h1, 0.08, 'timber')
        if mid:
            s.side_box(side, rect, a0, a1, mid, mid + 0.14, 0.08, 'timber')
        if braces:
            for i in range(n):
                if (i + side.__hash__()) % 2 == 0:
                    pa, pb = xs[i] + 0.07, xs[i + 1] - 0.07
                    lo, hi = h0 + 0.16, (mid or h1 - 0.2)
                    if (i % 4) < 2:
                        s.obox(s.wp(side, rect, pa, lo, 0.06), s.wp(side, rect, pb, hi, 0.06), 0.11, 0.1, 'timber')
                    else:
                        s.obox(s.wp(side, rect, pa, hi, 0.06), s.wp(side, rect, pb, lo, 0.06), 0.11, 0.1, 'timber')

    def window(s, side, rect, a, h0=1.1, h1=1.95, w=0.7, shutters=True, flowers=False, shutter_mat='wood_dark'):
        s.side_box(side, rect, a - w / 2 - 0.07, a + w / 2 + 0.07, h0 - 0.07, h1 + 0.07, 0.09, 'timber')
        s.side_box(side, rect, a - w / 2, a + w / 2, h0, h1, 0.12, 'glass')
        if B:
            s.side_box(side, rect, a - w / 2 - 0.13, a + w / 2 + 0.13, h0 - 0.13, h0 - 0.05, 0.21, 'stone_light')
            s.side_box(side, rect, a - w / 2 - 0.1, a + w / 2 + 0.1, h1 + 0.07, h1 + 0.15, 0.15, 'stone_light')
        s.side_box(side, rect, a - 0.025, a + 0.025, h0, h1, 0.14, 'timber')
        s.side_box(side, rect, a - w / 2, a + w / 2, (h0 + h1) / 2 - 0.025, (h0 + h1) / 2 + 0.025, 0.14, 'timber')
        if shutters:
            for sg in (-1, 1):
                aa = a + sg * (w / 2 + 0.07 + 0.17)
                s.side_box(side, rect, aa - 0.17, aa + 0.17, h0 - 0.05, h1 + 0.05, 0.1, shutter_mat)
        if flowers:
            s.side_box(side, rect, a - w / 2 - 0.1, a + w / 2 + 0.1, h0 - 0.35, h0 - 0.05, 0.3, 'wood')
            for k in range(4):
                aa = a - w / 2 + 0.1 + k * (w - 0.2) / 3
                s.side_box(side, rect, aa - 0.07, aa + 0.07, h0 - 0.05, h0 + 0.17, 0.26,
                           ['flower_r', 'flower_y', 'flower_w', 'flower_p'][(k + int(a * 3)) % 4])

    def door(s, side, rect, a, w=1.0, h=2.1, mat='wood_dark', lantern=True, step=True):
        s.side_box(side, rect, a - w / 2 - 0.14, a + w / 2 + 0.14, 0, h + 0.16, 0.1, 'timber')
        s.side_box(side, rect, a - w / 2, a + w / 2, 0, h, 0.14, mat)
        for k in (-1, 1):
            s.side_box(side, rect, a + k * w / 4 - 0.02, a + k * w / 4 + 0.02, 0.1, h - 0.1, 0.17, 'timber')
        s.side_box(side, rect, a + w / 2 - 0.22, a + w / 2 - 0.12, 0.95, 1.05, 0.2, 'gold')
        if step:
            s.side_box(side, rect, a - w / 2 - 0.2, a + w / 2 + 0.2, 0, 0.12, 0.45, 'stone_light')
        if lantern:
            ap = a + w / 2 + 0.55
            s.side_box(side, rect, ap - 0.07, ap + 0.07, h - 0.2, h + 0.15, 0.22, 'glow')
            s.side_box(side, rect, ap - 0.11, ap + 0.11, h + 0.15, h + 0.2, 0.26, 'iron')

    def plinth_ring(s, rect, h, mat='stone_dark', ext=0.1, thick=0.9):
        """Hollow base band around an enterable building (a solid plinth would bury the floor)."""
        x0, z0, x1, z1 = rect
        s.box(x0 - ext, z0 - ext, x1 + ext, z0 + thick, 0, h, mat)
        s.box(x0 - ext, z1 - thick, x1 + ext, z1 + ext, 0, h, mat)
        s.box(x0 - ext, z0 + thick, x0 + thick, z1 - thick, 0, h, mat)
        s.box(x1 - thick, z0 + thick, x1 + ext, z1 - thick, 0, h, mat)

    def chimney(s, x, z, h0, h1):
        s.box(x - 0.35, z - 0.35, x + 0.35, z + 0.35, h0, h1, 'stone_dark')
        s.box(x - 0.45, z - 0.45, x + 0.45, z + 0.45, h1, h1 + 0.14, 'stone')

    # ---------------------------------------------------------------- generic buildings
    def house(s, x0, z0, x1, z1, door_side='S', roof='roof_red', wall_h=2.6, rise=1.7, plaster='plaster',
              door_a=None, chim=True, axis=None, flowers=True):
        rect = (x0, z0, x1, z1)
        s.mark(x0, z0, x1, z1, '#')
        s.box(x0 - 0.04, z0 - 0.04, x1 + 0.04, z1 + 0.04, 0, 0.45, 'stone_dark')
        s.box(x0, z0, x1, z1, 0.4, wall_h, plaster)
        for side in 'NSEW':
            s.timber(side, rect, 0.4, wall_h, mid=1.55, braces=True)
        axis = s.gable_roof(x0, z0, x1, z1, wall_h, rise, 0.55, roof, axis=axis, gable_mat=plaster)
        if door_a is None:
            a0, a1 = s.wall_span(door_side, rect)
            door_a = (a0 + a1) / 2
        s.door(door_side, rect, door_a)
        for side in 'NSEW':
            a0, a1 = s.wall_span(side, rect)
            n = max(1, int((a1 - a0) / 2.6))
            for k in range(n):
                a = a0 + (a1 - a0) * (k + 0.5) / n
                if side == door_side and abs(a - door_a) < 1.5:
                    continue
                s.window(side, rect, a, flowers=(flowers and side == door_side))
        if chim:
            if axis == 'x':
                s.chimney(x0 + 1.0, (z0 + z1) / 2 + 0.4, wall_h + 0.6, wall_h + rise + 0.5)
            else:
                s.chimney((x0 + x1) / 2 + 0.4, z0 + 1.0, wall_h + 0.6, wall_h + rise + 0.5)

    def walls(s, rect, h, mat, thick, doors=(), door_h=2.9, trim=None):
        """Ring walls on the outer edge of rect. doors = [(side, a0, a1)]."""
        x0, z0, x1, z1 = rect
        for side in 'NSEW':
            a0, a1 = s.wall_span(side, rect)
            if side in 'EW':
                a0, a1 = a0 + thick, a1 - thick
            gaps = sorted((d[1], d[2]) for d in doors if d[0] == side)
            cur = a0
            segs = []
            for g0, g1 in gaps:
                segs.append((cur, g0)); cur = g1
            segs.append((cur, a1))
            for (p, q) in segs:
                if q - p < 0.01:
                    continue
                if side == 'N':   s.box(p, z0, q, z0 + thick, 0, h, mat)
                elif side == 'S': s.box(p, z1 - thick, q, z1, 0, h, mat)
                elif side == 'W': s.box(x0, p, x0 + thick, q, 0, h, mat)
                else:             s.box(x1 - thick, p, x1, q, 0, h, mat)
            for g0, g1 in gaps:
                if side == 'N':   s.box(g0, z0, g1, z0 + thick, door_h, h, mat)
                elif side == 'S': s.box(g0, z1 - thick, g1, z1, door_h, h, mat)
                elif side == 'W': s.box(x0, g0, x0 + thick, g1, door_h, h, mat)
                else:             s.box(x1 - thick, g0, x1, g1, door_h, h, mat)
        # collision ring
        for z in range(int(z0), int(z1)):
            for x in range(int(x0), int(x1)):
                if x == int(x0) or x == int(x1) - 1 or z == int(z0) or z == int(z1) - 1:
                    s.grid[z][x] = '#'
        for side, g0, g1 in doors:
            for a in range(int(g0), int(g1)):
                if side == 'N': s.grid[int(z0)][a] = 'D'
                elif side == 'S': s.grid[int(z1) - 1][a] = 'D'
                elif side == 'W': s.grid[a][int(x0)] = 'D'
                else: s.grid[a][int(x1) - 1] = 'D'

    # ---------------------------------------------------------------- props
    def barrel(s, x, z, block=True):
        s.cyl(x, z, 0.32, 0.27, 0, 0.8, 8, 'wood', sx=1, sz=1)
        s.cyl(x, z, 0.35, 0.35, 0.18, 0.24, 8, 'iron')
        s.cyl(x, z, 0.35, 0.35, 0.56, 0.62, 8, 'iron')
        if block: s.mark(x - 0.4, z - 0.4, x + 0.4, z + 0.4, 'o')

    def crate(s, x, z, sz=0.7, block=True, mat='wood'):
        s.box(x - sz / 2, z - sz / 2, x + sz / 2, z + sz / 2, 0, sz, mat)
        s.box(x - sz / 2 - 0.02, z - sz / 2 - 0.02, x + sz / 2 + 0.02, z + sz / 2 + 0.02, sz - 0.1, sz, 'wood_dark')
        s.box(x - sz / 2 - 0.02, z - sz / 2 - 0.02, x + sz / 2 + 0.02, z + sz / 2 + 0.02, 0, 0.1, 'wood_dark')
        if block: s.mark(x - sz / 2, z - sz / 2, x + sz / 2, z + sz / 2, 'o')

    def lamp(s, x, z):
        s.box(x - 0.07, z - 0.07, x + 0.07, z + 0.07, 0, 2.5, 'iron')
        s.box(x - 0.22, z - 0.22, x + 0.22, z + 0.22, 2.5, 2.95, 'glow')
        s.cyl(x, z, 0.34, 0.0, 2.95, 3.3, 4, 'iron', rot=math.pi / 4)
        s.box(x - 0.26, z - 0.26, x + 0.26, z + 0.26, 2.45, 2.52, 'iron')
        s.mark(x - 0.2, z - 0.2, x + 0.2, z + 0.2, 'o')

    def tree(s, x, z, kind=None, lod=0):
        r = s.vrng
        kind = kind or r.choice(['round', 'round', 'round', 'pine', 'tall'])
        sc = r.uniform(0.85, 1.25)
        rot = r.uniform(0, 6.28)
        if 0 <= int(x) < W and 0 <= int(z) < H:
            s.grid[int(z)][int(x)] = 'T'
        if lod == 0:
            # In-town trees are choppable: the game draws them (and their stumps) at runtime from tree_<kind>.glb.
            # The geometry is still built into a throw-away mesh so the random stream (and so every flower,
            # bush and tuft placed afterwards) stays exactly as it was when trees were baked in.
            prev, s.target = s.target, '_discard'
            s.tree_geom(x, z, kind, sc, rot, lod)
            s.target = prev
            s.trees.append((x, z, kind, sc, rot))
            return
        s.tree_geom(x, z, kind, sc, rot, lod)    # far forest ring: baked into the skirt

    def tree_geom(s, x, z, kind, sc, rot, lod):
        r = s.vrng
        if kind == 'teak':       # a tall, straight tree
            return s.tree_geom(x, z, 'tall', sc * 1.25, rot, lod)
        if kind == 'mahogany':   # a vast, broad tree
            return s.tree_geom(x, z, 'round', sc * 1.5, rot, lod)
        if B:
            return s.tree_b(x, z, kind, sc, rot, lod)
        if kind == 'oak':      # a broad, old oak: a bigger round tree
            return s.tree_geom(x, z, 'round', sc * 1.3, rot, lod)
        if kind == 'maple':    # a round tree in flame colours
            s.cyl(x, z, 0.26 * sc, 0.17 * sc, 0, 1.5 * sc, 6, 'trunk')
            s.blob(x, z, 2.25 * sc, 1.35 * sc, 1.35 * sc, 1.05 * sc, 'leaf_maple')
            s.blob(x + r.uniform(-.5, .5) * sc, z + r.uniform(-.5, .5) * sc, 2.9 * sc, 0.85 * sc, 0.85 * sc, 0.7 * sc, 'leaf_maple')
            return
        if kind in ('yew', 'magic'):      # a dark, dense conifer (the magic tree's leaves are violet)
            s.cyl(x, z, 0.30 * sc, 0.2 * sc, 0, 1.6 * sc, 6, 'trunk')
            for i, (rr, hh) in enumerate(((1.3, 1.1), (1.0, 2.0), (0.7, 2.8))):
                s.cyl(x, z, rr * sc, 0.0, hh * sc, (hh + 1.3) * sc, 7, 'leaf_magic' if kind == 'magic' else 'hedge', rot=r.uniform(0, 1))
            return
        if kind == 'willow':   # a drooping tree: a round crown with hanging strands
            s.cyl(x, z, 0.26 * sc, 0.17 * sc, 0, 1.5 * sc, 6, 'trunk')
            s.blob(x, z, 2.1 * sc, 1.25 * sc, 1.25 * sc, 0.95 * sc, 'leaf_b')
            for k in range(8):
                a = k * 0.785
                s.cyl(x + math.cos(a) * 1.05 * sc, z + math.sin(a) * 1.05 * sc, 0.08 * sc, 0.03 * sc, 0.5 * sc, 2.0 * sc, 4, 'leaf_c')
            return
        s.cyl(x, z, 0.26 * sc, 0.17 * sc, 0, 1.5 * sc, 6, 'trunk')
        if kind == 'pine':
            for i, (rr, hh) in enumerate(((1.35, 1.2), (1.05, 2.1), (0.72, 2.95))):
                s.cyl(x, z, rr * sc, 0.0, hh * sc, (hh + 1.35) * sc, 7, 'leaf_pine', rot=r.uniform(0, 1))
        elif kind == 'tall':
            s.blob(x, z, 2.7 * sc, 0.95 * sc, 0.95 * sc, 1.35 * sc, r.choice(['leaf_a', 'leaf_c']))
        else:
            m = r.choice(['leaf_a', 'leaf_b', 'leaf_c'])
            s.blob(x, z, 2.25 * sc, 1.35 * sc, 1.35 * sc, 1.05 * sc, m)
            s.blob(x + r.uniform(-.5, .5) * sc, z + r.uniform(-.5, .5) * sc, 2.9 * sc, 0.85 * sc, 0.85 * sc, 0.7 * sc, r.choice(['leaf_b', 'leaf_a']))

    def stump(s):
        """Cut-down tree: flared stump with a pale cut face. Built at the origin; the game places it."""
        r = 0.27 * 1.15
        if B:
            s.cyl(0, 0, r * 1.45, r * 1.05, 0, 0.26, 8, 'trunk')
            s.cyl(0, 0, r * 1.05, r * 0.92, 0.26, 0.40, 8, 'trunk')
            s.cyl(0, 0, r * 0.90, r * 0.90, 0.40, 0.43, 8, 'wood')
        else:
            s.cyl(0, 0, 0.30, 0.22, 0, 0.40, 6, 'trunk')
            s.cyl(0, 0, 0.21, 0.21, 0.40, 0.43, 6, 'wood')

    # ---- style B: fuller trees ----------------------------------------------------------------
    def trunk_b(s, x, z, sc, top, lean=(0.0, 0.0), n=8):
        """Flared, slightly leaning, three-section trunk."""
        r = 0.27 * sc
        s.cyl(x, z, r * 1.45, r * 1.05, 0, 0.28 * sc, n, 'trunk')
        mid = top * 0.55
        s.cyl(x, z, r * 1.05, r * 0.78, 0.28 * sc, mid, n, 'trunk')
        s.cyl(x + lean[0] * 0.4, z + lean[1] * 0.4, r * 0.78, r * 0.55, mid, top, n, 'trunk')

    def branch_b(s, x, z, h0, tx, tz, h1, w, mat='trunk'):
        s.obox((x, z, h0), (tx, tz, h1), w, w, mat)

    def tree_b(s, x, z, kind, sc, rot, lod):
        r = s.vrng
        sub = 1 if lod else 2
        if kind == 'oak':      # a broad, old oak: a bigger round tree
            return s.tree_b(x, z, 'round', sc * 1.3, rot, lod)
        if kind in ('yew', 'magic'):      # a dark, dense conifer (the magic tree's leaves are violet)
            top = 2.4 * sc
            s.trunk_b(x, z, sc * 1.15, top, n=7)
            tiers = 4 if lod else 6
            tmat = 'leaf_magic' if kind == 'magic' else 'hedge'
            for i in range(tiers):
                t = i / (tiers - 1)
                rr = (1.45 - 0.95 * t) * sc
                h0 = (0.9 + 2.7 * t * 0.85) * sc
                hh = (1.0 - 0.25 * t) * sc
                sides = 7 if lod else 10
                s.cyl(x, z, rr, rr * 0.25, h0, h0 + hh * 0.55, sides, tmat, rot=rot + i * 0.7)
                s.cyl(x, z, rr * 0.25, 0.0, h0 + hh * 0.55, h0 + hh * 1.45, sides, tmat, rot=rot + i * 0.7)
            return
        if kind == 'willow':   # a drooping tree: a round crown with long hanging strands
            top = 1.7 * sc
            s.trunk_b(x, z, sc, top)
            base = 2.2 * sc
            s.blob(x, z, base, 1.3 * sc, 1.3 * sc, 0.95 * sc, 'leaf_b', jitter=0.12, sub=sub, shade=(base - 1.0 * sc, base + 1.8 * sc))
            for k in range(5 if lod else 8):
                a = rot + k * (2 * math.pi / (5 if lod else 8))
                s.blob(x + math.cos(a) * 0.9 * sc, z + math.sin(a) * 0.9 * sc, base - 0.1 * sc, 0.55 * sc, 0.55 * sc, 0.5 * sc, 'leaf_c', jitter=0.1, sub=1)
                for m in range(2 if lod else 3):
                    b = a + (m - 1) * 0.28
                    s.cyl(x + math.cos(b) * 1.3 * sc, z + math.sin(b) * 1.3 * sc, 0.07 * sc, 0.025 * sc, 0.35 * sc, base - 0.1 * sc, 4, 'leaf_c')
            return
        if kind == 'pine':
            top = 2.2 * sc
            s.trunk_b(x, z, sc, top, n=7)
            tiers = 4 if lod else 6
            for i in range(tiers):
                t = i / (tiers - 1)
                rr = (1.55 - 1.05 * t) * sc
                h0 = (1.0 + 2.9 * t * 0.85) * sc
                hh = (1.0 - 0.25 * t) * sc
                sides = 7 if lod else 10
                m = 'leaf_pine'
                s.cyl(x, z, rr, rr * 0.25, h0, h0 + hh * 0.55, sides, m, rot=rot + i * 0.7)   # droopy skirt
                s.cyl(x, z, rr * 0.25, 0.0, h0 + hh * 0.55, h0 + hh * 1.45, sides, m, rot=rot + i * 0.7)
            # shade ramp for pine geometry is applied in apply_ao_b by height above tree base (see tag)
            return
        if kind == 'tall':
            top = 2.3 * sc
            s.trunk_b(x, z, sc, top)
            base = 2.0 * sc
            rows = [(0.0, 0.95), (0.9, 1.0), (1.8, 0.85), (2.6, 0.55)]
            for (dh, rad) in rows[:3 if lod else 4]:
                for k in range(2 if lod else 3):
                    a = rot + k * 2.1 + dh
                    off = 0.28 * sc * (1 if dh < 2 else 0.4)
                    s.blob(x + math.cos(a) * off, z + math.sin(a) * off, base + dh * sc, rad * 0.78 * sc, rad * 0.78 * sc,
                           rad * 0.95 * sc, r.choice(['leaf_a', 'leaf_c']), jitter=0.1, sub=sub,
                           shade=(base - 0.6 * sc, base + 3.5 * sc))
            return
        # ---- round broadleaf: trunk, forked branches, a cluster of lumpy canopies
        top = 1.7 * sc
        lean = (r.uniform(-.3, .3), r.uniform(-.3, .3))
        s.trunk_b(x, z, sc, top, lean)
        cx, cz = x + lean[0] * 0.4, z + lean[1] * 0.4
        base = 2.35 * sc
        for k in range(3):
            a = rot + k * 2.09
            s.branch_b(cx, cz, top - 0.1 * sc, cx + math.cos(a) * 0.75 * sc, cz + math.sin(a) * 0.75 * sc, base - 0.2 * sc, 0.13 * sc)
        leaf = ('leaf_maple', 'leaf_maple', 'leaf_maple') if kind == 'maple' else ('leaf_a', 'leaf_b', 'leaf_c')   # the maple burns orange
        mats = list(leaf)
        shade = (base - 1.1 * sc, base + 1.9 * sc)
        s.blob(cx, cz, base + 0.15 * sc, 1.25 * sc, 1.25 * sc, 0.95 * sc, leaf[0], jitter=0.14, sub=sub, shade=shade)
        n_ring = 4 if lod else 6
        for k in range(n_ring):
            a = rot + k * (2 * math.pi / n_ring) + r.uniform(-.25, .25)
            d = r.uniform(0.85, 1.15) * sc
            hz = base + r.uniform(-0.15, 0.55) * sc
            rad = r.uniform(0.7, 0.95) * sc
            s.blob(cx + math.cos(a) * d, cz + math.sin(a) * d, hz, rad, rad, rad * 0.82, r.choice(mats), jitter=0.12, sub=sub, shade=shade)
        for k in range(2 if lod else 3):
            a = rot + k * 2.4 + 1.0
            d = r.uniform(0.1, 0.55) * sc
            rad = r.uniform(0.55, 0.78) * sc
            s.blob(cx + math.cos(a) * d, cz + math.sin(a) * d, base + r.uniform(0.75, 1.2) * sc, rad, rad, rad * 0.8,
                   r.choice([leaf[1], leaf[0]]), jitter=0.1, sub=sub, shade=shade)

    def bush(s, x, z, flowers=False):
        r = s.vrng
        if B:
            for k in range(4):
                a = k * 1.7 + r.uniform(0, 1)
                d = 0.0 if k == 0 else r.uniform(0.25, 0.4)
                rad = r.uniform(0.34, 0.5)
                s.blob(x + math.cos(a) * d, z + math.sin(a) * d, 0.28 + (0.12 if k == 0 else 0.0), rad, rad, rad * 0.8,
                       'hedge' if not flowers else 'leaf_b', jitter=0.06, sub=2, shade=(0.0, 0.9))
            if flowers:
                for _ in range(7):
                    s.flower(x + r.uniform(-.45, .45), z + r.uniform(-.45, .45), h0=0.5 + r.uniform(0, .15), stem=0.0)
            return
        s.blob(x, z, 0.35, 0.5, 0.5, 0.42, 'hedge' if not flowers else 'leaf_b', jitter=0.07, sub=1)
        if flowers:
            for _ in range(4):
                s.box(x + r.uniform(-.35, .35), z + r.uniform(-.35, .35), x + r.uniform(-.35, .35) + 0.12,
                      z + r.uniform(-.35, .35) + 0.12, 0.55, 0.7, r.choice(['flower_r', 'flower_y', 'flower_w', 'flower_p']))

    def rock(s, x, z, sc=1.0, mat='rock', block=True):
        if B:
            r = s.vrng
            s.blob(x, z, 0.12 * sc, 0.7 * sc, 0.55 * sc, 0.5 * sc, mat, jitter=0.16, sub=2, shade=(-0.2 * sc, 0.8 * sc))
            if r.random() < 0.7:
                a = r.uniform(0, 6.28)
                s.blob(x + math.cos(a) * 0.6 * sc, z + math.sin(a) * 0.5 * sc, 0.06 * sc, 0.32 * sc, 0.28 * sc, 0.26 * sc, mat,
                       jitter=0.08, sub=1, shade=(-0.1, 0.4))
        else:
            s.blob(x, z, 0.15 * sc, 0.7 * sc, 0.55 * sc, 0.55 * sc, mat, jitter=0.18, sub=1)
        if block and 0 <= int(x) < W and 0 <= int(z) < H:
            s.grid[int(z)][int(x)] = 'o'

    def flower(s, fx, fz, h0=0.0, stem=0.28):
        """Style B flower: thin stem + small round head (no collision)."""
        r = s.vrng
        m = r.choice(['flower_r', 'flower_y', 'flower_w', 'flower_p'])
        if stem > 0:
            s.cyl(fx, fz, 0.016, 0.012, 0, stem, 3, 'leaf_b')
        s.blob(fx, fz, h0 + stem, 0.07, 0.07, 0.05, m, jitter=0.0, sub=0)

    def flower_patch(s, x, z, n=7, spread=0.8):
        r = s.vrng
        for _ in range(n):
            fx, fz = x + r.uniform(-spread, spread), z + r.uniform(-spread, spread)
            if B:
                s.flower(fx, fz, stem=r.uniform(0.2, 0.34))
                if r.random() < 0.6:
                    s.grass_tuft(fx + r.uniform(-.1, .1), fz + r.uniform(-.1, .1), 0.3, 3)
                continue
            m = r.choice(['flower_r', 'flower_y', 'flower_w', 'flower_p'])
            s.box(fx - .03, fz - .03, fx + .03, fz + .03, 0, 0.22, 'leaf_b')
            s.box(fx - .1, fz - .1, fx + .1, fz + .1, 0.22, 0.34, m)

    def grass_tuft(s, x, z, hgt=0.38, blades=4):
        """A little fan of bent two-sided blades. Dark at the root, light at the tip."""
        r = s.vrng
        bm = s.bm_for((x, z))
        col = bm.loops.layers.color.get('Col')
        mi = s.mi('tuft')
        for _ in range(blades):
            a = r.uniform(0, 6.28)
            lean = r.uniform(0.10, 0.22)
            hh = hgt * r.uniform(0.7, 1.2)
            w = 0.035
            ca, sa = math.cos(a), math.sin(a)
            px, pz = -sa * w, ca * w
            bx, bz = x + r.uniform(-0.12, 0.12), z + r.uniform(-0.12, 0.12)
            pts = [P(bx - px, bz - pz, 0.0), P(bx + px, bz + pz, 0.0), P(bx + ca * lean, bz + sa * lean, hh)]
            for order, k_root in (((0, 1, 2), 0.5), ((0, 2, 1), 0.5)):
                vs = [bm.verts.new(p) for p in pts]
                try:
                    fc = bm.faces.new([vs[i] for i in order])
                except ValueError:
                    continue
                fc.material_index = mi
                fc.smooth = False
                for l in fc.loops:
                    k = k_root if l.vert.co.z < 0.01 else 1.0
                    l[col] = (k, k, k, 1.0)

    def banner(s, side, rect, a, h_top, w, length, mat, inside=False, off=0.0):
        e = 0.03 + off
        x0, z0, x1, z1 = rect
        if side == 'N':   s.box(a - w / 2, z0 - 0.08 + off, a + w / 2, z0 + 0.0 + off, h_top - length, h_top, mat)
        elif side == 'S': s.box(a - w / 2, z1 - off, a + w / 2, z1 + 0.08 - off, h_top - length, h_top, mat)
        elif side == 'W': s.box(x0 - 0.08 + off, a - w / 2, x0 + off, a + w / 2, h_top - length, h_top, mat)
        else:             s.box(x1 - off, a - w / 2, x1 + 0.08 - off, a + w / 2, h_top - length, h_top, mat)

    def well(s, x, z):
        s.mark(x - 1, z - 1, x + 1, z + 1, 'o')
        s.cyl(x, z, 0.95, 0.95, 0, 0.75, 10, 'stone', rot=0.15)
        s.cyl(x, z, 0.98, 0.98, 0.7, 0.82, 10, 'stone_light', rot=0.15)
        s.cyl(x, z, 0.72, 0.72, 0.5, 0.78, 10, 'water_deep', rot=0.15)
        for sx in (-0.8, 0.8):
            s.box(x + sx - 0.08, z - 0.08, x + sx + 0.08, z + 0.08, 0.75, 2.5, 'wood_dark')
        s.obox((x - 0.8, z, 2.0), (x + 0.8, z, 2.0), 0.1, 0.1, 'wood')
        s.cyl(x, z, 0.06, 0.06, 2.0, 2.0 + 0.001, 6, 'wood')
        s.gable_roof(x - 1.0, z - 0.95, x + 1.0, z + 0.95, 2.5, 0.8, 0.3, 'roof_brown', axis='x', fill=False)
        s.box(x - 0.12, z - 0.12, x + 0.12, z + 0.12, 1.1, 1.5, 'wood')

    def fountain(s, x, z):
        s.mark(x - 1.5, z - 1.5, x + 1.5, z + 1.5, 'o')
        s.cyl(x, z, 1.62, 1.55, 0, 0.62, 12, 'stone_light', rot=0.13)
        s.cyl(x, z, 1.32, 1.32, 0.45, 0.58, 12, 'water', rot=0.13)
        s.cyl(x, z, 0.28, 0.22, 0.5, 1.5, 8, 'stone')
        s.cyl(x, z, 0.85, 0.2, 1.4, 1.75, 12, 'stone_light', rot=0.13)
        s.cyl(x, z, 0.7, 0.7, 1.67, 1.74, 12, 'water', rot=0.13)
        s.cyl(x, z, 0.1, 0.06, 1.7, 2.35, 6, 'stone')
        s.blob(x, z, 2.38, 0.17, 0.17, 0.17, 'gold', jitter=0.0, sub=1)

    def stall(s, x0, z0, x1, z1, awning_a='cloth_red', awning_b='cloth_white', goods=('flower_r', 'flower_y', 'leaf_b')):
        s.mark(x0, z0, x1, z1, 'o')
        s.box(x0 + 0.1, z1 - 0.9, x1 - 0.1, z1 - 0.1, 0, 0.95, 'wood')
        s.box(x0 + 0.05, z1 - 0.95, x1 - 0.05, z1 - 0.05, 0.95, 1.05, 'wood_dark')
        for i, gx in enumerate([x0 + 0.35 + k * 0.5 for k in range(int((x1 - x0 - 0.5) / 0.5) + 1)]):
            s.box(gx - 0.17, z1 - 0.75, gx + 0.17, z1 - 0.4, 1.05, 1.28, goods[i % len(goods)])
        for px in (x0 + 0.1, x1 - 0.1):
            for pz in (z0 + 0.1, z1 - 0.1):
                s.box(px - 0.06, pz - 0.06, px + 0.06, pz + 0.06, 0, 2.3, 'wood_dark')
        n = max(2, int((x1 - x0) / 0.5))
        for i in range(n):
            a0 = x0 + (x1 - x0) * i / n
            a1 = x0 + (x1 - x0) * (i + 1) / n
            s.box(a0, z0 - 0.2, a1, z1 + 0.15, 2.3, 2.42, awning_a if i % 2 == 0 else awning_b)
            s.box(a0, z1 + 0.15, a1, z1 + 0.22, 2.0, 2.42, awning_a if i % 2 == 0 else awning_b)

    def bench(s, x, z, axis='x'):
        if axis == 'x':
            s.box(x - 0.7, z - 0.22, x + 0.7, z + 0.22, 0.4, 0.5, 'wood')
            s.box(x - 0.7, z + 0.16, x + 0.7, z + 0.22, 0.5, 0.95, 'wood')
            for sx in (-0.6, 0.6): s.box(x + sx - 0.06, z - 0.2, x + sx + 0.06, z + 0.2, 0, 0.4, 'wood_dark')
        s.mark(x - 0.7, z - 0.25, x + 0.7, z + 0.25, 'o')

    def sign(s, x, z, mat='wood'):
        s.box(x - 0.06, z - 0.06, x + 0.06, z + 0.06, 0, 1.9, 'wood_dark')
        s.box(x - 0.55, z - 0.04, x + 0.55, z + 0.06, 1.25, 1.85, mat)
        s.box(x - 0.45, z - 0.06, x + 0.45, z - 0.0, 1.35, 1.75, 'gold')
        s.mark(x - 0.1, z - 0.1, x + 0.1, z + 0.1, 'o')

    # ---------------------------------------------------------------- finish
    def blocked_corner(s, x, z):
        """How many of the 4 tiles around lattice corner (x,z) are solid objects (building/prop/tree)."""
        n = 0
        for dx in (-1, 0):
            for dz in (-1, 0):
                xx, zz = x + dx, z + dz
                if 0 <= xx < W and 0 <= zz < H and s.grid[zz][xx] in '#oT':
                    n += 1
        return n

    def corner_ao(s, gx, gz):
        """Contact darkening near solid objects: bilinear blend of the corner counts around (gx,gz)."""
        x0, z0 = int(math.floor(gx)), int(math.floor(gz))
        fx, fz = gx - x0, gz - z0
        v = [[max(0.55, 1.0 - 0.14 * s.blocked_corner(x0 + i, z0 + j)) for i in (0, 1)] for j in (0, 1)]
        return (v[0][0] * (1 - fx) + v[0][1] * fx) * (1 - fz) + (v[1][0] * (1 - fx) + v[1][1] * fx) * fz

    # ---------------------------------------------------------------- style B: baked sun shadows
    SUN = Vector((-0.46, -0.49, 0.74)).normalized()     # sun direction in Blender coords (x, -z, up)
    SHADOW_STRENGTH = 0.52

    shadow_rects = None   # None = everywhere; otherwise only inside these rects (x0, z0, x1, z1): the wilds have no buildings to cast shadows

    def want_shadow(s, gx, gz):
        if s.shadow_rects is None:
            return True
        for r in s.shadow_rects:
            if r[0] <= gx < r[2] and r[1] <= gz < r[3]:
                return True
        return False

    def build_shadow_bvh(s):
        """Occluders = everything except ground, water, tufts, foam and the (hideable) roofs of enterable buildings."""
        skip = GROUND_MATS | {'carpet'} | {m for m in PAL if m.startswith('water')}
        verts, tris = [], []
        for key, bm in s.bms.items():
            if isinstance(key, str) and (key.startswith('roof_') or key.startswith('tree_') or key in ('stump', '_discard')):
                continue
            bm.verts.ensure_lookup_table()
            base = len(verts)
            idx = {}
            for f in bm.faces:
                if s.matlist[f.material_index] in skip or len(f.verts) < 3:
                    continue
                ids = []
                for v in f.verts:
                    if v.index not in idx:
                        idx[v.index] = len(verts)
                        verts.append(v.co.copy())
                    ids.append(idx[v.index])
                for i in range(1, len(ids) - 1):
                    tris.append((ids[0], ids[i], ids[i + 1]))
        s.bvh = BVHTree.FromPolygons(verts, tris)
        s._shadow_cache = {}
        print('shadow BVH: %d triangles' % len(tris))

    def shadow_k(s, bx, by):
        """Brightness multiplier (<=1) at ground point (Blender x, y): soft sun visibility from 5 jittered rays."""
        key = (round(bx * 8), round(by * 8))
        c = s._shadow_cache.get(key)
        if c is not None:
            return c
        o = Vector((bx, by, 0.04))
        side = s.SUN.cross(Vector((0, 0, 1))).normalized()
        up2 = side.cross(s.SUN).normalized()
        hit = 0
        offs = ((0, 0), (0.07, 0), (-0.07, 0), (0, 0.07), (0, -0.07))
        for (a, b) in offs:
            d = (s.SUN + side * a + up2 * b).normalized()
            if s.bvh.ray_cast(o, d, 60.0)[0] is not None:
                hit += 1
        k = 1.0 - s.SHADOW_STRENGTH * (hit / len(offs))
        s._shadow_cache[key] = k
        return k

    def apply_ao(s, bm):
        col = bm.loops.layers.color.get('Col')
        for f in bm.faces:
            mname = s.matlist[f.material_index]
            gnd = mname in GROUND_MATS
            wet = mname.startswith('water')   # river/fountain/well water is lit by the shader only
            for l in f.loops:
                z = l.vert.co.z
                old = l[col]
                if gnd or (B and mname == 'carpet'):
                    k = 1.0
                    if B and mname != 'tuft' and mname != 'carpet' and not wet:
                        gx, gz = l.vert.co.x, -l.vert.co.y
                        if mname.startswith('grass') or mname in ('dirt', 'sand', 'snow', 'moss', 'mud', 'wheat', 'dry_grass', 'red_sand', 'highland', 'highland_b'):
                            v = (math.sin(gx * 0.31) + math.sin(gz * 0.27 + 1.3) + math.sin((gx + gz) * 0.13 + 0.6)) / 3.0
                            k *= 0.90 + 0.10 * v + 0.0
                        if z > -0.2:
                            k *= s.corner_ao(gx, gz)
                    if B and s.bvh is not None and z > -0.2 and not wet and s.want_shadow(l.vert.co.x, -l.vert.co.y):
                        k *= s.shadow_k(l.vert.co.x, l.vert.co.y)
                elif B and mname in ('leaf_pine', 'leaf_magic'):
                    t = max(0.0, min(1.0, z / 4.4))
                    k = 0.58 + 0.42 * t
                else:
                    t = max(0.0, min(1.0, z / 1.6))
                    k = 0.70 + 0.30 * (t * t * (3 - 2 * t))
                l[col] = (old[0] * k, old[1] * k, old[2] * k, 1.0)

    # ---------------------------------------------------------------- style B: world-space UVs
    def apply_uv(s, bm):
        uvl = bm.loops.layers.uv.verify()
        bm.normal_update()
        for f in bm.faces:
            m = s.matlist[f.material_index]
            tm = TEXMAP.get(m)
            if not tm:
                continue
            sc = tm[1]
            n = f.normal
            if abs(n.z) > 0.95:
                for l in f.loops:
                    l[uvl].uv = (l.vert.co.x / sc, -l.vert.co.y / sc)
                continue
            L = math.hypot(n.x, n.y)
            tx, ty = -n.y / L, n.x / L
            vx, vy, vz = -n.z * n.x / L, -n.z * n.y / L, L
            for l in f.loops:
                p = l.vert.co
                l[uvl].uv = ((p.x * tx + p.y * ty) / sc, (p.x * vx + p.y * vy + p.z * vz) / sc)


# ==================================================================================================
# THE LAYOUT
# ==================================================================================================
def build():
    T = Town()
    R = T.rng

    # ---- terrain definition --------------------------------------------------------------------
    def wx(z):  # river edge: water where x < wx
        base = 5 + int(round(1.5 * math.sin(z / 6.0)))
        if 180 <= z <= 214:   # south of Millbrook the river swells into a lake
            base += int(round(15 * math.sin(math.pi * (z - 180) / 34.0)))
        return base

    CASTLE = (18, 3, 47, 25)    # curtain wall ring (x 18..46, z 3..24)
    KEEP = (24, 6, 41, 16)      # throne hall (x 24..40, z 6..15)
    PLAZA = (25, 35, 40, 46)
    ROAD_NS = (31, 25, 34, 64)
    ROAD_EW = (9, 39, 64, 42)
    GATE = (31, 34)             # gate columns 31..33

    T.floor((0, 0, W, H), 'grass_a', 'grass_b')
    T.floor(ROAD_NS, 'cobble_a', 'cobble_b')
    T.floor(ROAD_EW, 'cobble_a', 'cobble_b')
    T.floor(PLAZA, 'plaza_a', 'plaza_b')
    T.floor((19, 4, 46, 24), 'floor_a', 'floor_b')                      # courtyard
    T.floor((19, 4, 46, 24), 'grass_c', 'grass_b')                     # grass base...
    T.floor((31, 16, 34, 24), 'cobble_a', 'cobble_b')                  # ...with a stone path to the keep
    T.floor((24, 17, 41, 19), 'cobble_a', 'cobble_b')
    T.floor((19, 4, 24, 24), 'floor_a', 'floor_b')                     # west training yard
    T.floor((24, 6, 41, 16), 'floor_a', 'floor_b')                     # keep interior
    T.floor((31, 8, 34, 16), 'carpet')                                 # red carpet
    T.floor((42, 44, 52, 52), 'wood_floor', 'wood_floor')              # inn interior
    T.floor((42, 31, 50, 38), 'floor_a', 'floor_b')                    # bank interior (floors must be laid before the ground is built)
    T.floor((48, 132, 56, 139), 'floor_a', 'floor_b')                  # Millbrook bank
    T.floor((9, 53, 23, 63), 'floor_a', 'floor_b')                     # the crypt under the castle's graveyard
    T.floor((47, 99, 62, 110), 'cobble_a', 'cobble_b')                 # the crafters' yard in Millbrook
    def road2(z):                                                      # the road south of Millbrook (straight again from the caves on)
        return 32 if z >= 236 else 32 + int(round(3 * math.sin((z - 176) / 9.0)))
    T.floor((0, 216, 64, 241), 'highland', 'highland_b')                # the troll mountains
    T.floor((0, 241, 64, 272), 'cave', 'cave_b')                       # the mining caves
    T.floor((0, 272, 64, 304), 'sand', 'sand')                         # the desert
    T.floor((44, 290, 62, 304), 'floor_a', 'floor_b')                  # the dragon's temple floor
    T.floor((0, 304, 64, 336), 'abyss', 'abyss_b')                     # the Abyss
    T.floor((4, 219, 20, 234), 'dirt', 'dirt')                         # the highland quarry
    for z in range(176, 336):                                          # the road runs over the top of all of them
        if z < 304:
            T.floor((road2(z) - 1, z, road2(z) + 3, z + 1), 'dirt', 'dirt')
        else:
            T.floor((29, z, 36, z + 1), 'abyss', 'abyss_b')
    COOP = (9, 8, 17, 20)       # chicken coop: fence ring x 9..16, z 8..19 (interior x 10..15, z 9..18), gate on the south side
    PEN = (50, 6, 62, 20)       # cow pen: fence ring x 50..61, z 6..19 (interior x 51..60, z 7..18), gate on the south side
    T.floor((10, 9, 16, 19), 'dirt', 'dirt')                           # the chickens' yard is bare earth
    T.floor((53, 15, 59, 19), 'dirt', 'dirt')                         # trampled ground by the cow pen gate
    T.floor((51, 7, 61, 10), 'dirt', 'dirt')                          # ...and round the troughs
    SMITHY = (54, 51, 63, 56)   # the smithy yard (flagstones): furnace and anvils.  The quarry (bare earth, rocks) is just south of it.
    QUARRY = (53, 57, 63, 63)
    T.floor(SMITHY, 'floor_a', 'floor_b')
    T.floor(QUARRY, 'dirt', 'dirt')
    T.floor((60, 42, 62, 51), 'cobble_a', 'cobble_b')                 # a cobbled path from the main road down to the smithy

    # ---- the road south and Millbrook village: only the ground here (the buildings and props come further down) ----
    def path_x(z):                                                    # the dirt road wanders a little on its way south
        return 32 if z >= 91 else 32 + int(round(3 * math.sin((z - 64) / 7.0)))
    for z in range(64, 96):
        T.floor((path_x(z) - 1, z, path_x(z) + 3, z + 1), 'dirt', 'dirt')
    GOBCAMP = (40, 73, 57, 89)        # the goblins' camp beside the road
    V_STREET = (30, 96, 34, 176)      # Millbrook's main street (cobbles)
    V_SQUARE = (19, 127, 46, 146)     # the village square: shops all round it
    V_SMITHY = (46, 111, 62, 119)     # a second smithy
    V_MINE = (47, 119, 63, 129)       # Millbrook mine (bare earth)
    V_FARM = (45, 158, 62, 174)       # Old Tom's field
    T.floor(GOBCAMP, 'dirt', 'dirt')
    T.floor(V_STREET, 'cobble_a', 'cobble_b')
    T.floor(V_SQUARE, 'plaza_a', 'plaza_b')
    T.floor(V_SMITHY, 'floor_a', 'floor_b')
    T.floor(V_MINE, 'dirt', 'dirt')
    T.floor(V_FARM, 'dirt', 'dirt')
    T.floor((34, 102, 36, 105), 'cobble_a', 'cobble_b')              # the inn's doorstep

    def detail_n(x, z):   # (core coordinates) quads per tile side: the castle town and Millbrook keep 4, the road between and Lakeside 2, the wild south 1
        if z < 70 or 92 <= z < 180:
            return 4
        if z < 216:
            return 2
        if 44 <= x < 62 and 288 <= z < 304:
            return 2
        return 1

    # grass colour noise
    def tile_mat(x, z):
        m = None
        for rect, a, b in T.floors:
            if rect[0] <= x < rect[2] and rect[1] <= z < rect[3]:
                m = a if ((x + z) & 1) == 0 else b
        if m in ('grass_a', 'grass_b'):
            n = (x * 73856093 ^ z * 19349663) & 7
            m = ('grass_a', 'grass_b', 'grass_c', 'grass_a', 'grass_b', 'grass_a', 'grass_c', 'grass_dark')[n]
        if WORLD and m:
            m = WM.core_seam_material(m, x, z)
            m = WM.core_edge_material(m, x, z)
        if B and m:   # textured ground: one material per surface type, variation comes from vertex colour
            if m.startswith('grass'):
                m = 'grass_a'
            m = {'cobble_b': 'cobble_a', 'plaza_b': 'plaza_a', 'floor_b': 'floor_a'}.get(m, m)
        return m

    # ---- ground, river, cliffs ------------------------------------------------------------------
    water = set()
    for z in range(H):
        for x in range(W):
            if x < wx(z):
                water.add((x, z))
    for z in range(H):
        for x in range(W):
            if (x, z) in water:
                T.grid[z][x] = '~'
                T.quad(P(x, z + 1, -0.35), P(x + 1, z + 1, -0.35), P(x + 1, z, -0.35), P(x, z, -0.35),
                       'water' if B else ('water', 'water_b', 'water_c', 'water')[(x * 31 + z * 17) & 3], (x + .5, z + .5))
                if B:   # pale shallow-water / foam strip where the river meets the bank
                    for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        if (x + dx, z + dz) not in water and 0 <= x + dx < W and 0 <= z + dz < H:
                            fw = 0.32
                            if dx == 1:    q = (x + 1 - fw, z, x + 1, z + 1)
                            elif dx == -1: q = (x, z, x + fw, z + 1)
                            elif dz == 1:  q = (x, z + 1 - fw, x + 1, z + 1)
                            else:          q = (x, z, x + 1, z + fw)
                            T.quad(P(q[0], q[3], -0.338), P(q[2], q[3], -0.338), P(q[2], q[1], -0.338), P(q[0], q[1], -0.338), 'foam', (x + .5, z + .5))
                for dx, dz, p0, p1 in ((1, 0, (x + 1, z + 1), (x + 1, z)), (0, 1, (x, z + 1), (x + 1, z + 1)),
                                       (0, -1, (x + 1, z), (x, z)), (-1, 0, (x, z), (x, z + 1))):
                    if (x + dx, z + dz) not in water and 0 <= x + dx < W and 0 <= z + dz < H:
                        T.quad(P(p0[0], p0[1], -0.35), P(p1[0], p1[1], -0.35), P(p1[0], p1[1], 0), P(p0[0], p0[1], 0),
                               'cliff' if (x + z) % 3 else 'cliff_dark', (x + .5, z + .5))
            else:
                gm = tile_mat(x, z)
                if B:   # sub-quads so baked sun shadows have finer resolution where there are buildings; the wilds get one quad a tile
                    n = detail_n(x, z)
                    for i in range(n):
                        for j in range(n):
                            xa, xb, za, zb = x + i / n, x + (i + 1) / n, z + j / n, z + (j + 1) / n
                            T.quad(P(xa, zb, 0), P(xb, zb, 0), P(xb, za, 0), P(xa, za, 0), gm, (x + .5, z + .5))
                else:
                    T.quad(P(x, z + 1, 0), P(x + 1, z + 1, 0), P(x + 1, z, 0), P(x, z, 0), gm, (x + .5, z + .5))
    # sandy shoreline strip
    for z in range(H):
        e = wx(z)
        for x in range(e, e + 1):
            T.quad(P(x, z + 1, 0.012), P(x + 1, z + 1, 0.012), P(x + 1, z, 0.012), P(x, z, 0.012), 'sand', (x + .5, z + .5))
    if B:   # low stone kerbs where paved ground meets grass
        for z in range(H):
            for x in range(W):
                if tile_mat(x, z) not in ('cobble_a', 'plaza_a'):
                    continue
                for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, nz = x + dx, z + dz
                    if not (0 <= nx < W and 0 <= nz < H) or (nx, nz) in water:
                        continue
                    if (tile_mat(nx, nz) or '').startswith('grass'):
                        kw = 0.17
                        if dx == 1:    T.box(x + 1 - kw, z, x + 1, z + 1, 0, 0.07, 'stone_light', c=(x + .5, z + .5))
                        elif dx == -1: T.box(x, z, x + kw, z + 1, 0, 0.07, 'stone_light', c=(x + .5, z + .5))
                        elif dz == 1:  T.box(x, z + 1 - kw, x + 1, z + 1, 0, 0.07, 'stone_light', c=(x + .5, z + .5))
                        else:          T.box(x, z, x + 1, z + kw, 0, 0.07, 'stone_light', c=(x + .5, z + .5))
    # rocks along the bank
    for z in range(2, HT - 2, 3):
        T.rock(wx(z) + R.uniform(0.3, 1.1), z + R.uniform(0.2, 0.8), R.uniform(0.7, 1.3), 'cliff' if R.random() < .5 else 'rock')

    # pier (two planks wide) for fishing later
    pz0 = 33
    for z in (pz0, pz0 + 1):
        for x in range(1, wx(z) + 1):
            T.grid[z][x] = '.'
        T.box(1.0, z, wx(z) + 0.4, z + 1, 0.0, 0.12, 'wood', c=(3, z))
    for x in (1.5, 3.5):
        for z in (pz0 - 0.05, pz0 + 2.05):
            T.box(x - 0.1, z - 0.1, x + 0.1, z + 0.1, -0.35, 0.5, 'wood_dark', c=(x, z))
    T.box(1.0, pz0 + 0.0, 1.12, pz0 + 2, 0.12, 0.2, 'wood_dark', c=(1, pz0))

    # ---- fishing spots ------------------------------------------------------------------------
    # A spot is a water tile right next to a walkable bank/pier tile. Here we only pick the tiles (and write them to
    # town.map); the game draws the swirling ripples, bubbles and the leaping fish on them.
    def shore_ok(z):   # last water tile has an open bank tile (and the tile behind it) next to it
        e = wx(z)
        return 2 <= z < HT - 2 and T.grid[z][e] == '.' and T.grid[z][e + 1] == '.' and (e - 1, z) in water
    for z0 in (11, 20, 47, 57):
        for dz in (0, 1, -1, 2, -2, 3, -3):
            if shore_ok(z0 + dz):
                z = z0 + dz
                T.spots.append((wx(z) - 1 + 0.5, z + 0.5, 'net'))
                break
    for (sx, sz) in ((0, pz0), (2, pz0 - 1), (3, pz0 + 2)):   # off the end and the two sides of the pier
        assert (sx, sz) in water and T.grid[sz][sx] == '~'
        T.spots.append((sx + 0.5, sz + 0.5, 'net'))
    spot_tiles = {(int(sx), int(sz)) for sx, sz, _ in T.spots}

    # reeds along the near bank and a few lily pads (own random stream, so the rest of the layout is unchanged)
    rr = random.Random(555)
    for z in range(1, HT - 1):
        x = wx(z) - 1
        if pz0 - 1 <= z <= pz0 + 2 or (x, z) in spot_tiles or (x, z) not in water or rr.random() > 0.30:
            continue
        cx0, cz0 = x + rr.uniform(0.55, 0.9), z + rr.uniform(0.2, 0.8)
        for _ in range(rr.randint(3, 5)):
            rx, rz, rh = cx0 + rr.uniform(-0.18, 0.18), cz0 + rr.uniform(-0.18, 0.18), rr.uniform(0.55, 1.0)
            T.cyl(rx, rz, 0.02, 0.009, -0.34, rh, 5, 'leaf_c')
            if rr.random() < 0.6:   # cattail head
                T.cyl(rx, rz, 0.028, 0.026, rh - 0.1, rh + 0.1, 5, 'trunk')
    for _ in range(26):
        z = rr.randint(2, HT - 3)
        x = rr.randint(0, max(0, wx(z) - 3))
        if (x, z) not in water or (x, z) in spot_tiles or pz0 - 1 <= z <= pz0 + 2:
            continue
        px, pz = x + rr.uniform(0.2, 0.8), z + rr.uniform(0.2, 0.8)
        T.cyl(px, pz, 0.30, 0.30, -0.335, -0.325, 8, 'leaf_b', rot=rr.uniform(0, 6.28))
        if rr.random() < 0.35:
            T.cyl(px + 0.04, pz, 0.07, 0.01, -0.325, -0.24, 6, 'flower_w')

    # ---- CASTLE ---------------------------------------------------------------------------------
    cx0, cz0, cx1, cz1 = CASTLE
    WALL_H, WALL_T = 3.6, 1.0
    T.walls(CASTLE, WALL_H, 'stone', WALL_T, doors=[('S', GATE[0], GATE[1])], door_h=3.1)
    # wall batter + merlons
    def merlons(side):
        a0, a1 = T.wall_span(side, CASTLE)
        n = int((a1 - a0) / 0.9)
        for i in range(n):
            a = a0 + (i + 0.5) * (a1 - a0) / n
            if side == 'S' and GATE[0] - 0.2 < a < GATE[1] + 0.2:
                continue
            for off in (0.12, WALL_T - 0.12):
                if side == 'N':   T.box(a - 0.25, cz0 + off - 0.12, a + 0.25, cz0 + off + 0.12, WALL_H, WALL_H + 0.5, 'stone_light')
                elif side == 'S': T.box(a - 0.25, cz1 - off - 0.12, a + 0.25, cz1 - off + 0.12, WALL_H, WALL_H + 0.5, 'stone_light')
                elif side == 'W': T.box(cx0 + off - 0.12, a - 0.25, cx0 + off + 0.12, a + 0.25, WALL_H, WALL_H + 0.5, 'stone_light')
                else:             T.box(cx1 - off - 0.12, a - 0.25, cx1 - off + 0.12, a + 0.25, WALL_H, WALL_H + 0.5, 'stone_light')
    for sd in 'NSEW':
        merlons(sd)
    # plinth (darker base band)
    T.box(cx0 - 0.1, cz0 - 0.1, cx1 + 0.1, cz0 + 0.0, 0, 0.7, 'stone_dark')
    T.box(cx0 - 0.1, cz1, cx1 + 0.1, cz1 + 0.1, 0, 0.7, 'stone_dark')
    T.box(cx0 - 0.1, cz0, cx0, cz1, 0, 0.7, 'stone_dark')
    T.box(cx1, cz0, cx1 + 0.1, cz1, 0, 0.7, 'stone_dark')
    # gate arch + portcullis + banners
    T.box(GATE[0] - 0.2, cz1 - 1.0, GATE[0], cz1 + 0.12, 0, 3.1, 'stone_light')
    T.box(GATE[1], cz1 - 1.0, GATE[1] + 0.2, cz1 + 0.12, 0, 3.1, 'stone_light')
    T.box(GATE[0] - 0.2, cz1 - 1.0, GATE[1] + 0.2, cz1 + 0.12, 3.0, 3.3, 'stone_light')
    for gx in [GATE[0] + 0.3 + i * 0.4 for i in range(7)]:
        T.box(gx - 0.03, cz1 - 0.6, gx + 0.03, cz1 - 0.5, 2.35, 3.0, 'iron')
    T.box(GATE[0], cz1 - 0.6, GATE[1], cz1 - 0.5, 2.7, 2.78, 'iron')
    T.box(GATE[0], cz1 - 0.6, GATE[1], cz1 - 0.5, 2.4, 2.48, 'iron')

    # corner towers (round)
    def tower(cx, cz, r, h, roof_mat='roof_blue', flag='banner_blue'):
        T.mark(cx - r, cz - r, cx + r, cz + r, '#')
        T.cyl(cx, cz, r + 0.15, r + 0.1, 0, 0.9, 12, 'stone_dark')
        T.cyl(cx, cz, r, r - 0.08, 0.9, h, 12, 'stone')
        for k in range(3):   # arrow slits
            a = k * 2 * math.pi / 3 + 0.5
            T.box(cx + math.cos(a) * (r - 0.05) - 0.06, cz + math.sin(a) * (r - 0.05) - 0.06,
                  cx + math.cos(a) * (r + 0.1) + 0.06, cz + math.sin(a) * (r + 0.1) + 0.06, h * 0.55, h * 0.55 + 0.9, 'iron')
        T.cyl(cx, cz, r + 0.3, r + 0.3, h, h + 0.3, 12, 'stone_light')
        n = 10
        for i in range(n):
            a = 2 * math.pi * (i + 0.5) / n
            mx, mz = cx + math.cos(a) * (r + 0.22), cz + math.sin(a) * (r + 0.22)
            T.cyl(mx, mz, 0.26, 0.26, h + 0.3, h + 0.8, 4, 'stone_light', rot=a + math.pi / 4)
        T.cyl(cx, cz, r - 0.1, r * 0.2, h + 0.3, h + 0.3 + r * 1.9, 12, roof_mat)
        T.obox((cx, cz, h + 0.3 + r * 1.9), (cx, cz, h + 0.3 + r * 1.9 + 1.3), 0.06, 0.06, 'wood_dark')
        T.obox((cx + 0.03, cz, h + 0.3 + r * 1.9 + 1.25), (cx + 0.75, cz, h + 0.3 + r * 1.9 + 0.95), 0.03, 0.4, flag)

    for (tx, tz) in ((cx0 + 0.5, cz0 + 0.5), (cx1 - 0.5, cz0 + 0.5), (cx0 + 0.5, cz1 - 0.5), (cx1 - 0.5, cz1 - 0.5)):
        tower(tx, tz, 2.05, 6.4)
    # gatehouse towers (square)
    def sq_tower(x0, z0, x1, z1, h):
        T.mark(x0, z0, x1, z1, '#')
        T.box(x0 - 0.1, z0 - 0.1, x1 + 0.1, z1 + 0.1, 0, 0.8, 'stone_dark')
        T.box(x0, z0, x1, z1, 0.8, h, 'stone')
        T.box(x0 - 0.2, z0 - 0.2, x1 + 0.2, z1 + 0.2, h, h + 0.3, 'stone_light')
        for i in range(5):
            for (px, pz) in ((x0 - 0.1 + i * (x1 - x0 + 0.2) / 4.0, z0 - 0.1), (x0 - 0.1 + i * (x1 - x0 + 0.2) / 4.0, z1 + 0.1)):
                T.box(px - 0.2, pz - 0.2, px + 0.2, pz + 0.2, h + 0.3, h + 0.8, 'stone_light')
        for i in range(1, 4):
            for (px, pz) in ((x0 - 0.1, z0 - 0.1 + i * (z1 - z0 + 0.2) / 4.0), (x1 + 0.1, z0 - 0.1 + i * (z1 - z0 + 0.2) / 4.0)):
                T.box(px - 0.2, pz - 0.2, px + 0.2, pz + 0.2, h + 0.3, h + 0.8, 'stone_light')
        for k in range(2):
            ax = x0 + 0.75 + k * 1.0
            T.box(ax - 0.06, z1 - 0.02, ax + 0.06, z1 + 0.1, 3.2, 4.3, 'iron')
    sq_tower(28, 21.2, 31, 24.8, 5.2)
    sq_tower(34, 21.2, 37, 24.8, 5.2)
    # banners on gatehouse
    T.box(28.9, 24.8, 29.5, 24.9, 3.0, 5.0, 'banner_blue')
    T.box(35.5, 24.8, 36.1, 24.9, 3.0, 5.0, 'banner_blue')
    T.box(29.0, 24.9, 29.4, 24.95, 4.5, 4.9, 'gold')
    T.box(35.6, 24.9, 36.0, 24.95, 4.5, 4.9, 'gold')

    # ---- keep / throne hall -------------------------------------------------------------------
    kx0, kz0, kx1, kz1 = KEEP
    KH = 4.8
    T.walls(KEEP, KH, 'stone_light', 0.8, doors=[('S', 31, 34)], door_h=3.3)
    T.plinth_ring(KEEP, 0.6)
    # buttresses and door frame
    for (bx, bz) in ((kx0, kz0), (kx1, kz0), (kx0, kz1), (kx1, kz1)):
        T.box(bx - 0.45, bz - 0.45, bx + 0.45, bz + 0.45, 0, KH + 0.4, 'stone')
        T.box(bx - 0.55, bz - 0.55, bx + 0.55, bz + 0.55, KH + 0.4, KH + 0.55, 'stone_light')
    T.box(30.7, kz1 - 0.8, 31.0, kz1 + 0.2, 0, 3.3, 'stone')
    T.box(34.0, kz1 - 0.8, 34.3, kz1 + 0.2, 0, 3.3, 'stone')
    T.box(30.7, kz1 - 0.8, 34.3, kz1 + 0.2, 3.3, 3.65, 'stone')
    for ax in (31.4, 32.5, 33.6):
        T.box(ax - 0.05, kz1 - 0.55, ax + 0.05, kz1 - 0.45, 0, 3.3, 'wood_dark')
    # tall windows on the keep (south face either side of the door, plus east/west)
    keep_rect = KEEP
    for a in (26.5, 28.5, 36.5, 38.5):
        T.side_box('S', keep_rect, a - 0.3, a + 0.3, 1.8, 3.6, 0.1, 'timber')
        T.side_box('S', keep_rect, a - 0.22, a + 0.22, 1.9, 3.5, 0.14, 'glass')
    for a in (8.5, 12.5):
        for sd in 'EW':
            T.side_box(sd, keep_rect, a - 0.3, a + 0.3, 1.8, 3.6, 0.1, 'timber')
            T.side_box(sd, keep_rect, a - 0.22, a + 0.22, 1.9, 3.5, 0.14, 'glass')
    # banners on keep
    for a in (30.0, 35.0):
        T.banner('S', keep_rect, a, 4.3, 0.7, 2.4, 'banner_red', off=-0.0)
    # keep roof (separate object so it can be hidden when the player walks in)
    T.target = 'roof_keep'
    T.gable_roof(kx0, kz0, kx1, kz1, KH + 0.4, 2.7, 0.9, 'roof_slate', axis='x', gable_mat='stone_light', trim='stone_dark')
    T.chimney(27.5, 8.5, KH + 0.5, KH + 4.2)
    T.target = None
    T.roof_rects['keep'] = KEEP
    # interior: pillars, dais, thrones, candelabra, tables
    T.box(28, 6.8, 37, 8.6, 0, 0.35, 'stone_light')           # dais
    T.floor((28, 7, 37, 9), 'floor_a')
    T.floor((31, 7, 34, 9), 'carpet')
    T.box(31.0, 6.85, 31.9, 7.45, 0.35, 0.95, 'wood_dark')
    for (tx, col) in ((30.8, 'banner_red'), (33.3, 'banner_red')):
        T.box(tx, 6.9, tx + 0.9, 7.6, 0.35, 0.8, 'gold')      # thrones: seat
        T.box(tx, 6.85, tx + 0.9, 7.1, 0.8, 2.1, 'gold')      # back
        T.box(tx + 0.05, 6.9, tx + 0.85, 7.15, 1.0, 1.9, col)
        T.mark(tx, 6.8, tx + 0.9, 7.6, 'o')
    for py in (9.5, 11.5, 13.5):
        for px in (28.3, 36.7):
            T.cyl(px, py, 0.42, 0.42, 0, 0.3, 8, 'stone_dark')
            T.cyl(px, py, 0.3, 0.3, 0.3, 4.3, 8, 'stone_light')
            T.cyl(px, py, 0.44, 0.44, 4.3, 4.6, 8, 'stone_dark')
            T.mark(px - 0.4, py - 0.4, px + 0.4, py + 0.4, 'o')
    for cy in (10.5, 12.5):
        for cx in (30.3, 34.7):
            T.cyl(cx, cy, 0.18, 0.1, 0, 0.12, 6, 'iron')
            T.box(cx - 0.04, cy - 0.04, cx + 0.04, cy + 0.04, 0.12, 1.45, 'gold')
            T.obox((cx - 0.3, cy, 1.5), (cx + 0.3, cy, 1.5), 0.06, 0.06, 'gold')
            for dx in (-0.3, 0, 0.3):
                T.box(cx + dx - 0.04, cy - 0.04, cx + dx + 0.04, cy + 0.04, 1.5, 1.85 if dx == 0 else 1.7, 'glow')
            T.mark(cx - .25, cy - .25, cx + .25, cy + .25, 'o')
    for bx in (26.5, 29.5, 35.5, 38.5):
        T.box(bx - 0.5, 6.8, bx + 0.5, 6.88, 1.8, 4.2, 'banner_blue')
        T.box(bx - 0.5, 6.8, bx - 0.4, 6.9, 1.8, 4.2, 'gold')
        T.box(bx + 0.4, 6.8, bx + 0.5, 6.9, 1.8, 4.2, 'gold')
        T.box(bx - 0.1, 6.88, bx + 0.1, 6.92, 2.8, 3.4, 'gold')
    for (tz0, tz1) in ((9, 14),):
        for (tx0, tx1) in ((25.2, 26.8), (38.2, 39.8)):
            T.box(tx0, tz0, tx1, tz1, 0.75, 0.9, 'cloth_white')
            T.box(tx0 + 0.1, tz0 + 0.1, tx1 - 0.1, tz1 - 0.1, 0, 0.8, 'wood_dark')
            for k in range(3):
                T.box(tx0 + 0.4, tz0 + 0.3 + k * 1.5, tx0 + 0.9, tz0 + 0.55 + k * 1.5, 0.9, 1.15, ['cloth_red', 'flower_y', 'cloth_green'][k])
            T.mark(tx0, tz0, tx1, tz1, 'o')

    # courtyard dressing
    T.flower_patch(21.5, 5.5, 14, 1.2); T.flower_patch(43.5, 5.5, 14, 1.2)
    for (hx, hz0, hz1) in ((20.2, 6.5, 14), (44.3, 6.5, 14)):
        pass
    for z in (7, 9, 11, 13, 15, 17):
        T.bush(20.5, z + 0.5, flowers=(z % 4 == 1)); T.bush(44.5, z + 0.5, flowers=(z % 4 == 3))
    # training yard (west): dummies + weapon rack + archery targets
    for (dx, dz) in ((21.5, 18.5), (23.5, 20.5), (21.5, 21.5)):
        T.box(dx - 0.07, dz - 0.07, dx + 0.07, dz + 0.07, 0, 1.6, 'wood_dark')
        T.obox((dx - 0.5, dz, 1.3), (dx + 0.5, dz, 1.3), 0.1, 0.1, 'wood_dark')
        T.blob(dx, dz, 1.65, 0.2, 0.2, 0.2, 'straw', jitter=0.02)
        T.box(dx - 0.2, dz - 0.2, dx + 0.2, dz + 0.2, 0.6, 1.3, 'straw')
        T.mark(dx - 0.3, dz - 0.3, dx + 0.3, dz + 0.3, 'o')
    for ty in (19.5, 22.5):
        T.cyl(43.5, ty, 0.6, 0.6, 0.9, 0.98, 10, 'cloth_white', sx=0.2, sz=1.0)
        T.cyl(43.45, ty, 0.35, 0.35, 0.95, 1.02, 10, 'cloth_red', sx=0.2, sz=1.0)
        T.box(43.4, ty - 0.05, 43.6, ty + 0.05, 0, 0.95, 'wood_dark')
        T.mark(43, ty - 0.5, 44, ty + 0.5, 'o')
    T.box(26.4, 20.1, 27.8, 20.35, 0.0, 1.3, 'wood_dark'); T.mark(26.4, 20.1, 27.8, 20.4, 'o')
    for k in range(4):
        T.obox((26.6 + k * 0.35, 20.2, 0.5), (26.6 + k * 0.35, 20.2, 1.5), 0.07, 0.07, 'iron')
    # barrels / crates near the gate inside, lamps
    for (bx, bz) in ((29.6, 22.8 - 3.2), (30.3, 20.0)):
        pass
    T.barrel(27.5, 17.4 + 0.5); T.barrel(28.3, 17.4 + 0.6); T.crate(26.8, 18.8 + 1.0)
    for lx, lz in ((30.4, 18.6), (34.6, 18.6)):
        T.lamp(lx, lz)
    # flag pole in courtyard
    T.cyl(32.5, 20.5, 0.15, 0.1, 0, 5.2, 6, 'wood_dark')
    T.obox((32.55, 20.5, 5.0), (33.7, 20.5, 4.55), 0.04, 0.8, 'banner_red')
    T.mark(32.2, 20.2, 32.8, 20.8, 'o')
    # trees inside castle
    for (tx, tz) in ((22.5, 9.5), (22.5, 14.5), (42.5, 9.5), (42.5, 14.5)):
        T.tree(tx, tz, 'round')

    # ---- TOWN BUILDINGS -------------------------------------------------------------------------
    # General store
    T.house(17, 31, 24, 38, 'S', roof='roof_red', plaster='plaster', door_a=20.5)
    T.sign(23.5, 38.7)
    T.barrel(17.8, 38.7); T.crate(18.9, 38.8, 0.6)
    # Houses
    T.house(11, 31, 16, 37, 'S', roof='roof_brown', plaster='plaster')
    T.house(11, 44, 16, 50, 'N', roof='roof_slate', plaster='plaster')
    T.house(55, 31, 60, 37, 'S', roof='roof_red')
    T.house(55, 44, 60, 50, 'N', roof='roof_brown')
    T.house(36, 50, 41, 56, 'N', roof='roof_red')
    T.house(24, 54, 29, 59, 'E', roof='roof_slate')
    T.house(35, 58, 40, 62, 'N', roof='roof_brown')
    T.house(46, 56, 52, 61, 'W', roof='roof_red')
    # Blacksmith (non-enterable) + forge yard
    T.house(17, 46, 24, 52, 'N', roof='roof_slate', plaster='plaster', door_a=20.5, chim=False)
    T.chimney(18.2, 49, 3.2, 6.2)
    T.box(24.3, 47.0, 26.5, 49.0, 0, 0.9, 'stone_dark'); T.box(24.5, 47.2, 26.3, 48.8, 0.9, 1.0, 'rock_dark')   # forge
    T.box(24.5, 47.2, 26.3, 48.8, 1.0, 1.06, 'glow')
    T.box(24.7, 47.0, 26.1, 47.4, 1.0, 2.0, 'stone_dark')
    T.mark(24.3, 47.0, 26.5, 49.0, 'o')
    T.box(25.3, 49.8, 26.3, 50.3, 0, 0.7, 'wood_dark'); T.box(25.1, 49.7, 26.5, 50.4, 0.7, 0.88, 'iron'); T.box(26.4, 49.9, 26.9, 50.2, 0.72, 0.84, 'iron')
    T.mark(25.1, 49.7, 26.9, 50.4, 'o')
    T.barrel(25.0, 51.2); T.crate(26.2, 51.4, 0.6)
    T.box(18.0, 52.1, 20.0, 52.4, 0, 1.2, 'wood_dark'); T.mark(18, 52.1, 20, 52.4, 'o')
    for k in range(3):
        T.obox((18.3 + k * 0.6, 52.2, 0.2), (18.3 + k * 0.6, 52.2, 1.5), 0.08, 0.08, 'iron')
    T.sign(23.6, 45.6)

    # Bank (enterable, stone, columns). A function: there is a second bank in Millbrook (offset by ox, oz).
    def bank_at(ox, oz, key):
        BANK = (42 + ox, 31 + oz, 50 + ox, 38 + oz)
        T.walls(BANK, 3.2, 'stone_light', 0.6, doors=[('W', 34 + oz, 36 + oz)], door_h=2.7)
        T.plinth_ring(BANK, 0.5)
        T.side_box('N', BANK, 44.0 + ox, 45.0 + ox, 1.2, 2.4, 0.1, 'timber')
        # windows (stone arch style)
        for side, aa in (('N', 44.5 + ox), ('N', 47.0 + ox), ('S', 44.5 + ox), ('S', 47.0 + ox), ('E', 33.5 + oz), ('E', 36.0 + oz)):
            T.window(side, BANK, aa, 1.2, 2.2, 0.8, shutters=False)
        # portico: two columns + small pediment over door
        for cz in (33.2 + oz, 36.8 + oz):
            T.cyl(41.0 + ox, cz, 0.28, 0.28, 0, 3.0, 8, 'stone_light')
            T.cyl(41.0 + ox, cz, 0.38, 0.38, 0, 0.25, 8, 'stone_dark'); T.cyl(41.0 + ox, cz, 0.38, 0.38, 2.9, 3.15, 8, 'stone_dark')
            T.mark(40.7 + ox, cz - 0.3, 41.3 + ox, cz + 0.3, 'o')
        T.box(40.6 + ox, 32.8 + oz, 42.0 + ox, 37.2 + oz, 3.15, 3.45, 'stone')
        T.tri_prism('x', 40.6 + ox, 42.0 + ox, 32.8 + oz, 37.2 + oz, 3.45, 4.3, 'stone_light')
        T.cyl(41.3 + ox, 35.0 + oz, 0.4, 0.4, 3.6, 3.66, 12, 'gold', sx=0.15, sz=1.0)
        # roof (hip-ish gable, red) separate so it hides when inside
        T.target = 'roof_' + key
        T.gable_roof(42 + ox, 31 + oz, 50 + ox, 38 + oz, 3.2, 2.0, 0.7, 'roof_red', axis='z', gable_mat='stone_light', trim='stone_dark')
        T.target = None
        T.roof_rects[key] = BANK
        # interior: a long counter with glass, the bankers stand behind it (east side); chests along the west wall
        T.box(46.3 + ox, 32.2 + oz, 47.0 + ox, 36.8 + oz, 0, 1.05, 'wood_dark'); T.box(46.2 + ox, 32.1 + oz, 47.2 + ox, 36.9 + oz, 1.05, 1.15, 'wood')
        for bz in (32.6 + oz, 34.2 + oz, 35.8 + oz):
            T.box(46.5 + ox, bz, 46.7 + ox, bz + 0.08, 1.15, 2.3, 'glass')
            T.box(46.5 + ox, bz + 0.0, 46.6 + ox, bz + 0.1, 1.15, 2.4, 'timber')
        T.mark(46 + ox, 32 + oz, 47.5 + ox, 37 + oz, 'o')
        for cz in (32.4 + oz, 36.0 + oz):
            T.box(43.2 + ox, cz, 44.2 + ox, cz + 0.8, 0, 0.7, 'wood_dark'); T.box(43.15 + ox, cz - 0.05, 44.25 + ox, cz + 0.85, 0.7, 0.82, 'wood')
            T.box(43.55 + ox, cz + 0.28, 43.85 + ox, cz + 0.45, 0.8, 0.9, 'gold'); T.mark(43.15 + ox, cz - 0.05, 44.25 + ox, cz + 0.85, 'o')
        T.floor((42 + ox, 34 + oz, 44 + ox, 36 + oz), 'carpet')
        T.box(48.4 + ox, 31.9 + oz, 48.8 + ox, 32.4 + oz, 0, 1.5, 'iron'); T.mark(48.3 + ox, 31.9 + oz, 49 + ox, 32.5 + oz, 'o')
    bank_at(0, 0, 'bank')

    # Inn (enterable, wood floor)
    INN = (42, 44, 52, 52)
    T.walls(INN, 2.9, 'plaster', 0.6, doors=[('W', 47, 49)], door_h=2.5)
    T.plinth_ring(INN, 0.45)
    for side in 'NSEW':
        T.timber(side, INN, 0.45, 2.9, mid=1.5, braces=True)
    for side, aa in (('N', 44.2), ('N', 47.0), ('N', 49.8), ('S', 44.2), ('S', 47.0), ('S', 49.8), ('E', 46.0), ('E', 50.0)):
        T.window(side, INN, aa, flowers=(side == 'S'))
    T.door('W', INN, 48.0, w=2.0, h=2.4, lantern=True)
    T.target = 'roof_inn'
    T.gable_roof(42, 44, 52, 52, 2.9, 2.2, 0.7, 'roof_red', axis='x', gable_mat='plaster')
    # dormers
    for dx in (45.5, 48.5):
        T.box(dx - 0.55, 43.5, dx + 0.55, 45.0, 3.5, 4.5, 'plaster')
        T.tri_prism('z', 43.5, 45.0, dx - 0.65, dx + 0.65, 4.5, 5.1, 'roof_red')
    T.chimney(50.6, 48.0, 5.0, 7.3); T.chimney(43.2, 47.0, 4.8, 6.9)
    T.target = None
    T.roof_rects['inn'] = INN
    # hanging sign
    T.obox((41.55, 45.2, 3.1), (40.5, 45.2, 3.1), 0.08, 0.08, 'iron')
    T.cyl(40.55, 44.9, 0.28, 0.28, 2.35, 2.55, 10, 'gold', sx=1, sz=0.12)
    T.mark(39.9, 44.8, 41.2, 45.2, 'o')
    # interior: bar, tables, hearth
    T.box(44.0, 45.0, 51.0, 46.0, 0, 1.0, 'wood_dark'); T.box(43.9, 44.9, 51.1, 46.1, 1.0, 1.1, 'wood')
    T.mark(44, 45, 51, 46, 'o')
    for bx in (45.0, 46.5, 48.0, 49.5):
        T.barrel(bx, 44.6, block=False)
    for tx, tz in ((44.6, 49.8), (47.2, 50.0), (49.3, 49.2)):
        T.cyl(tx, tz, 0.55, 0.55, 0.75, 0.85, 8, 'wood'); T.cyl(tx, tz, 0.1, 0.1, 0, 0.75, 6, 'wood_dark')
        for a in range(4):
            sx, sz = tx + math.cos(a * math.pi / 2) * 0.9, tz + math.sin(a * math.pi / 2) * 0.9
            T.cyl(sx, sz, 0.2, 0.2, 0, 0.45, 6, 'wood_dark')
        T.mark(tx - 0.6, tz - 0.6, tx + 0.6, tz + 0.6, 'o')
    T.box(50.2, 47.6, 51.6, 49.6, 0, 2.6, 'stone_dark'); T.box(50.2, 48.0, 50.6, 49.2, 0, 0.9, 'glow')  # hearth
    T.mark(50, 47, 52, 50, '#')
    # outdoor seating
    T.bench(46.0, 52.7); T.barrel(52.7, 50.5)

    # Fountain, well, market
    T.fountain(32.5, 40.5)
    T.well(27.2, 37.2)
    T.stall(34.8, 36.0, 38.2, 38.0, 'cloth_red', 'cloth_white', ('flower_r', 'flower_y', 'leaf_b'))
    T.stall(26.2, 42.6, 29.8, 44.6, 'cloth_green', 'cloth_white', ('flower_y', 'flower_w', 'cloth_red'))
    T.bench(30.0, 36.2); T.bench(35.0, 43.8); T.bench(37.8, 41.0)

    # lamps along roads and plaza
    for z in (27, 51, 61):
        T.lamp(30.5, z + 0.5); T.lamp(34.5, z + 0.5)
    for x in (12, 20, 50, 58):
        T.lamp(x + 0.5, 42.5)
    for (lx, lz) in ((25.5, 35.5), (39.0, 35.5), (25.5, 45.0), (39.0, 45.0)):
        T.lamp(lx, lz)

    # hedges / fences: simple low fences in front of houses' yards
    def fence(x0, z0, x1, z1):
        if x0 == x1:
            n = int(abs(z1 - z0) / 0.9)
            for i in range(n + 1):
                z = z0 + (z1 - z0) * i / max(1, n)
                T.box(x0 - 0.05, z - 0.05, x0 + 0.05, z + 0.05, 0, 0.9, 'wood')
            T.obox((x0, z0, 0.7), (x1, z1, 0.7), 0.06, 0.1, 'wood_dark')
            T.obox((x0, z0, 0.35), (x1, z1, 0.35), 0.06, 0.1, 'wood_dark')
        else:
            n = int(abs(x1 - x0) / 0.9)
            for i in range(n + 1):
                x = x0 + (x1 - x0) * i / max(1, n)
                T.box(x - 0.05, z0 - 0.05, x + 0.05, z0 + 0.05, 0, 0.9, 'wood')
            T.obox((x0, z0, 0.7), (x1, z1, 0.7), 0.06, 0.1, 'wood_dark')
            T.obox((x0, z0, 0.35), (x1, z1, 0.35), 0.06, 0.1, 'wood_dark')
    fence(11, 38.6, 16, 38.6); T.mark(11, 38.4, 16, 38.9, 'f')
    fence(55, 38.6, 60, 38.6); T.mark(55, 38.4, 60, 38.9, 'f')
    for (fx, fz) in ((12.5, 38.0), (14.5, 38.0), (56.5, 38.0), (58.5, 38.0)):
        T.flower_patch(fx, fz, 6, 0.5)

    # ---- animal pens: chicken coop and cow pen ---------------------------------------------------
    pr = random.Random(2024)   # own random stream: placing the pens must not move any tree
    RESERVED = [(COOP[0] - 1, COOP[1] - 1, COOP[2] + 1, COOP[3] + 5), (PEN[0] - 1, PEN[1] - 1, PEN[2] + 1, PEN[3] + 5)]

    def pen_fence(x0, z0, x1, z1, side, g0, g1):
        """Fence ring on the perimeter TILES x0..x1, z0..z1 (inclusive) with a gate gap of tiles g0..g1 on one side."""
        def run(a, b, fixed, axis):   # fence along tile centres a..b (inclusive) at tile row/col `fixed`
            if a > b:
                return
            if axis == 'x':
                fence(a + 0.5, fixed + 0.5, b + 0.5, fixed + 0.5)
                T.mark(a, fixed, b + 1, fixed + 1, 'f')
            else:
                fence(fixed + 0.5, a + 0.5, fixed + 0.5, b + 0.5)
                T.mark(fixed, a, fixed + 1, b + 1, 'f')
        for sd, fixed, a, b, axis in (('N', z0, x0, x1, 'x'), ('S', z1, x0, x1, 'x'), ('W', x0, z0, z1, 'z'), ('E', x1, z0, z1, 'z')):
            if sd == side:
                run(a, g0 - 1, fixed, axis)
                run(g1 + 1, b, fixed, axis)
            else:
                run(a, b, fixed, axis)
        # gate posts and an open gate leaf swung into the pen
        if side == 'S':
            for gx in (g0 + 0.08, g1 + 0.92):
                T.box(gx - 0.1, z1 + 0.4, gx + 0.1, z1 + 0.6, 0, 1.35, 'wood_dark')
                T.box(gx - 0.13, z1 + 0.37, gx + 0.13, z1 + 0.63, 1.35, 1.45, 'wood')
            for hh in (0.35, 0.8):
                T.obox((g0 + 0.08, z1 + 0.5, hh), (g0 + 0.08, z1 - 0.95, hh), 0.06, 0.1, 'wood')
            T.obox((g0 + 0.08, z1 + 0.5, 0.35), (g0 + 0.08, z1 - 0.95, 0.85), 0.06, 0.08, 'wood_dark')
            for gx in range(g0, g1 + 1):   # keep the gate gap open
                T.grid[z1][gx] = '.'

    def trough(x0, z0, x1, z1):
        T.box(x0 + 0.05, z0 + 0.12, x1 - 0.05, z1 - 0.12, 0, 0.5, 'wood_dark')
        T.box(x0 + 0.2, z0 + 0.25, x1 - 0.2, z1 - 0.25, 0.42, 0.47, 'water')
        T.mark(x0, z0, x1, z1, 'o')

    def hay_bale(x, z):
        T.box(x + 0.06, z + 0.1, x + 0.94, z + 0.9, 0, 0.78, 'straw')
        for bx in (0.3, 0.7):
            T.box(x + bx - 0.03, z + 0.08, x + bx + 0.03, z + 0.92, 0, 0.8, 'timber')
        T.mark(x, z, x + 1, z + 1, 'o')

    def bucket(x, z):
        T.cyl(x, z, 0.19, 0.15, 0, 0.32, 8, 'wood')
        T.cyl(x, z, 0.16, 0.16, 0.29, 0.31, 8, 'water')

    def straw_bits(x0, z0, x1, z1, n):
        for _ in range(n):
            sx, sz = pr.uniform(x0, x1), pr.uniform(z0, z1)
            ang = pr.uniform(0, math.pi)
            T.obox((sx, sz, 0.025), (sx + math.cos(ang) * 0.3, sz + math.sin(ang) * 0.3, 0.025), 0.04, 0.03, 'straw')

    def coop_house(x0, z0, x1, z1):
        """A small hen house on stilts with a thatched roof, a ramp to the pop-hole (south face) and nest boxes (east face)."""
        T.mark(x0, z0, x1, z1, '#')
        cx = (x0 + x1) / 2.0
        T.box(x0 + 0.1, z0 + 0.1, x1 - 0.1, z1 - 0.1, 0, 0.4, 'wood_dark')
        T.box(x0 + 0.06, z0 + 0.06, x1 - 0.06, z1 - 0.06, 0.4, 1.7, 'wood')
        for i in range(1, int((x1 - x0) * 2.4)):                                     # plank seams
            px = x0 + i * (x1 - x0) / int((x1 - x0) * 2.4)
            T.box(px - 0.015, z0 + 0.04, px + 0.015, z0 + 0.08, 0.4, 1.7, 'wood_dark')
            T.box(px - 0.015, z1 - 0.08, px + 0.015, z1 - 0.04, 0.4, 1.7, 'wood_dark')
        for pz in (z0 + 0.04, z1 - 0.04):                                            # corner posts
            for px in (x0 + 0.04, x1 - 0.04):
                T.box(px - 0.07, pz - 0.07, px + 0.07, pz + 0.07, 0, 1.75, 'timber')
        T.gable_roof(x0 - 0.05, z0 - 0.05, x1 + 0.05, z1 + 0.05, 1.7, 0.95, 0.3, 'roof_thatch', axis='x', gable_mat='wood', trim='wood_dark')
        T.box(cx - 0.32, z1 - 0.03, cx + 0.32, z1 + 0.06, 0.45, 1.3, 'timber')       # pop-hole frame...
        T.box(cx - 0.24, z1 - 0.03, cx + 0.24, z1 + 0.09, 0.5, 1.24, 'wood_dark')
        T.obox((cx, z1 + 1.15, 0.05), (cx, z1 + 0.08, 0.5), 0.5, 0.05, 'wood')       # ...and its ramp with cleats
        for k in range(5):
            cz = z1 + 0.2 + k * 0.2
            T.obox((cx - 0.25, cz, 0.05 + (z1 + 1.15 - cz) * 0.42), (cx + 0.25, cz, 0.05 + (z1 + 1.15 - cz) * 0.42), 0.04, 0.05, 'wood_dark')
        for k in range(3):                                                           # nest boxes with straw
            nz = z0 + 0.35 + k * 0.85
            T.box(x1 - 0.02, nz, x1 + 0.42, nz + 0.7, 0.55, 1.0, 'wood')
            T.box(x1 + 0.06, nz + 0.08, x1 + 0.4, nz + 0.62, 0.9, 1.02, 'straw')
        T.box(x0 - 0.04, z0 + 0.7, x0 + 0.05, z0 + 1.5, 0.95, 1.5, 'timber')         # a shuttered window on the west face
        T.box(x0 - 0.06, z0 + 0.78, x0 + 0.03, z0 + 1.42, 1.02, 1.42, 'glass')

    # chicken coop
    pen_fence(9, 8, 16, 19, 'S', 12, 13)
    coop_house(10, 9, 13, 12)
    trough(14, 10, 16, 11)
    bucket(10.7, 14.5)
    bucket(15.1, 17.2)
    straw_bits(10.2, 12.6, 15.8, 18.8, 40)
    T.sign(11.4, 20.4)
    T.pens.append(('coop', 10, 9, 16, 19))
    # cow pen
    pen_fence(50, 6, 61, 19, 'S', 54, 55)
    trough(52, 8, 54, 9)
    trough(56, 8, 58, 9)
    hay_bale(59, 8); hay_bale(60, 8); hay_bale(60, 9)
    bucket(52.5, 11.3)
    bucket(59.4, 17.2)
    straw_bits(51.2, 12.0, 60.8, 14.5, 26)
    T.sign(56.6, 20.4)
    T.pens.append(('cowpen', 51, 7, 61, 19))

    def free_tiles(x0, z0, x1, z1):
        return [(x, z) for z in range(z0, z1) for x in range(x0, x1) if T.grid[z][x] == '.']

    def populate(species, pen, count, tiles, taken):
        for _ in range(count):
            for _try in range(200):
                x, z = pr.choice(tiles)
                if all(abs(x - tx) + abs(z - tz) >= 2 for tx, tz in taken):
                    taken.append((x, z))
                    T.npcs.append((species, x + pr.uniform(0.3, 0.7), z + pr.uniform(0.3, 0.7), pen))
                    break
    taken = []
    coop_tiles = free_tiles(10, 9, 16, 19)
    populate('chicken', 'coop', 7, coop_tiles, taken)
    taken = []
    cow_tiles = free_tiles(51, 7, 61, 19)
    populate('cow', 'cowpen', 3, cow_tiles, taken)
    populate('calf', 'cowpen', 3, cow_tiles, taken)


    # ---- smithy and quarry (south-east corner) ---------------------------------------------------
    RESERVED.append((53, 42, 63, 63))

    def stone_wall(x0, z0, x1, z1, h=1.15):
        T.box(x0, z0, x1, z1, 0, h, 'stone')
        T.box(x0 - 0.04, z0 - 0.04, x1 + 0.04, z1 + 0.04, h, h + 0.08, 'stone_light')
        T.mark(x0, z0, x1, z1, 'o')

    # low stone walls on the north and the two short sides (low enough to see over), open to the south
    stone_wall(54, 51, 63, 51.5)
    stone_wall(54, 51.5, 54.5, 53)
    stone_wall(62.5, 51.5, 63, 53)
    # the furnace stands against the back wall; two anvils on stumps in front of it
    T.stations.append(('furnace', 57.5, 52.5, 0.0))
    T.mark(57, 52, 58, 53, 'o')
    T.stations.append(('anvil', 55.5, 54.5, 90.0))
    T.mark(55, 54, 56, 55, 'o')
    T.stations.append(('anvil', 60.5, 54.5, 90.0))
    T.mark(60, 54, 61, 55, 'o')
    trough(61, 52, 62, 53)                                              # the quenching trough
    T.blob(59.5, 52.45, 0.0, 0.42, 0.34, 0.34, 'iron', jitter=0.05, sub=1)   # a heap of coal
    T.blob(59.9, 52.7, 0.0, 0.22, 0.2, 0.2, 'iron', jitter=0.04, sub=1)
    T.mark(59, 52, 60, 53, 'o')
    # weapon rack against the wall: two posts, two rails, and blades hanging from them
    for rx in (54.7, 56.1):
        T.box(rx - 0.05, 51.52, rx + 0.05, 51.62, 0.1, 1.05, 'wood_dark')
    T.box(54.65, 51.52, 56.15, 51.6, 0.82, 0.88, 'wood')
    T.box(54.65, 51.52, 56.15, 51.6, 0.38, 0.44, 'wood')
    for i, bx in enumerate((54.85, 55.15, 55.45, 55.75)):
        T.box(bx - 0.018, 51.64, bx + 0.018, 51.68, 0.46, 1.02, 'iron')
        T.box(bx - 0.06, 51.64, bx + 0.06, 51.68, 0.80 + 0.05 * (i % 2), 0.83 + 0.05 * (i % 2), 'wood_dark')
    T.barrel(53.55, 55.4)
    T.barrel(62.45, 55.45)
    T.crate(61.6, 55.45, 0.7)
    T.lamp(53.6, 53.6)
    T.sign(53.2, 55.2)          # (signs stand on the outer edge, out of the camera's way)

    # the quarry: bare earth, rubble, a mine cart, and the rocks themselves
    ROCKS = [('copper', 55, 58), ('copper', 55, 60), ('tin', 58, 58), ('tin', 58, 61), ('iron', 61, 58), ('iron', 61, 60),
             ('coal', 55, 62), ('coal', 60, 62), ('mithril', 62, 62)]
    for kind, rx, rz in ROCKS:
        T.rocks.append((rx + 0.5, rz + 0.5, kind, pr.uniform(0, 360)))
        T.mark(rx, rz, rx + 1, rz + 1, 'o')
    for _ in range(14):                                                    # loose rubble (not blocking)
        qx, qz = pr.uniform(53.5, 62.5), pr.uniform(57.3, 62.7)
        if T.grid[int(qz)][int(qx)] == '.':
            T.blob(qx, qz, 0.0, pr.uniform(0.1, 0.22), pr.uniform(0.1, 0.18), pr.uniform(0.06, 0.12), pr.choice(['rock', 'rock_dark', 'stone']), jitter=0.03, sub=1)
    # a little mine cart full of stone, with wheels
    T.box(56.9, 59.55, 58.1, 60.45, 0.22, 0.34, 'wood_dark')
    for (x0, z0, x1, z1) in ((56.9, 59.55, 58.1, 59.65), (56.9, 60.35, 58.1, 60.45), (56.9, 59.55, 57.0, 60.45), (58.0, 59.55, 58.1, 60.45)):
        T.box(x0, z0, x1, z1, 0.34, 0.72, 'wood')
    T.blob(57.5, 60.0, 0.55, 0.45, 0.32, 0.2, 'rock', jitter=0.06, sub=1)
    for (cwx, cwz) in ((57.15, 59.5), (57.85, 59.5), (57.15, 60.5), (57.85, 60.5)):
        T.box(cwx - 0.11, cwz - 0.03, cwx + 0.11, cwz + 0.03, 0.02, 0.24, 'iron')
    T.mark(57, 59, 58, 61, 'o')
    # a sign at the quarry entrance (nothing tall in here: the camera must see the hero while he mines)
    T.sign(53.3, 59.0)
    T.lamp(62.5, 57.4)


    # ============================================================================================================
    # THE PEOPLE OF JOURNEY, THE ROAD SOUTH, THE GOBLIN CAMP AND MILLBROOK VILLAGE
    # ============================================================================================================
    VR = random.Random(7771)   # their own random stream: nothing in the castle town moves

    def person(key, x, z, yaw=0.0):
        T.persons.append((key, x, z, yaw))
        T.mark(int(x), int(z), int(x) + 1, int(z) + 1, 'o')   # people are solid: nobody walks through a shopkeeper

    # ---- people of the castle town: the general store keeper behind the first market stall, a guard, the smith
    person('aubrey', 36.5, 36.5, 0.0)
    person('guard_gate', 34.5, 26.5, 270.0)
    person('torvald', 59.5, 54.5, 90.0)
    person('oswin', 47.6, 33.5, 270.0)       # the bankers stand behind the bank counter
    person('ysolde', 47.6, 35.5, 270.0)

    # ---- the road south: signs and boulders
    RESERVED.append((GOBCAMP[0] - 1, GOBCAMP[1] - 1, GOBCAMP[2] + 1, GOBCAMP[3] + 1))
    RESERVED.append((45, 110, 63, 130))
    RESERVED.append((19, 126, 47, 147))
    RESERVED.append((V_FARM[0] - 1, V_FARM[1] - 1, V_FARM[2] + 1, V_FARM[3] + 1))
    T.sign(36.8, 66.5)
    T.sign(26.5, 94.5)
    for z in range(66, 96, 4):
        bx = path_x(z) + (-2.2 if (z // 4) % 2 else 4.2) + VR.uniform(-0.3, 0.3)
        T.rock(bx, z + VR.uniform(0, 1), VR.uniform(0.6, 1.2), 'rock' if VR.random() < .6 else 'rock_dark', block=True)

    # ---- the goblin camp: a ring of sharpened stakes (with gaps), hide tents, a fire, a pot, bones and a totem
    gx0, gz0, gx1, gz1 = GOBCAMP
    stakes = []
    for xx in range(gx0, gx1):
        stakes += [(xx + 0.5, gz0 + 0.5), (xx + 0.5, gz1 - 0.5)]
    for zz in range(gz0, gz1):
        stakes += [(gx0 + 0.5, zz + 0.5), (gx1 - 0.5, zz + 0.5)]
    for (sx_, sz_) in stakes:
        if sx_ < gx0 + 1 and 79 <= sz_ <= 85:     # the way in, from the road
            continue
        if VR.random() < 0.40:
            continue
        T.box(sx_ - 0.07, sz_ - 0.07, sx_ + 0.07, sz_ + 0.07, 0, 1.25 + VR.uniform(0, 0.4), 'wood_dark')
        T.cyl(sx_, sz_, 0.07, 0.0, 1.25, 1.55 + VR.uniform(0, 0.3), 4, 'wood_dark', rot=0.7)
        T.mark(int(sx_), int(sz_), int(sx_) + 1, int(sz_) + 1, 'o')

    def tent(cx, cz, r, h, mat):
        T.cyl(cx, cz, r, 0.0, 0.0, h, 7, mat, rot=0.4)
        T.cyl(cx, cz, r * 0.2, 0.2, 0.0, 0.12, 7, 'cloth_white')
        T.box(cx - 0.32, cz + r * 0.55, cx + 0.32, cz + r * 0.8, 0.0, 0.95, 'timber')          # the dark doorway
        T.box(cx - 0.05, cz - 0.05, cx + 0.05, cz + 0.05, h - 0.1, h + 0.7, 'wood_dark')         # the pole sticking out of the top
        T.mark(cx - r * 0.65, cz - r * 0.65, cx + r * 0.65, cz + r * 0.65, 'o')
    tent(46.0, 78.0, 1.7, 2.3, 'cloth_green')
    tent(52.5, 77.5, 1.5, 2.0, 'cloth_red')
    tent(45.5, 86.0, 1.6, 2.1, 'cloth_red')
    tent(52.0, 85.5, 1.8, 2.4, 'cloth_green')
    # fire pit in the middle: a ring of stones, logs and glowing coals (the fire itself is just for show)
    fx, fz = 48.8, 81.8
    for i in range(9):
        a = 2 * math.pi * i / 9
        T.blob(fx + math.cos(a) * 0.62, fz + math.sin(a) * 0.62, 0.0, 0.17, 0.15, 0.13, 'rock_dark', jitter=0.04, sub=1)
    T.cyl(fx, fz, 0.5, 0.45, 0.0, 0.07, 9, 'iron')
    for a in (0.3, 1.6, 2.9):
        T.obox((fx - math.cos(a) * 0.4, fz - math.sin(a) * 0.4, 0.17), (fx + math.cos(a) * 0.4, fz + math.sin(a) * 0.4, 0.17), 0.13, 0.13, 'trunk')
    T.cyl(fx, fz, 0.3, 0.2, 0.07, 0.26, 7, 'glow')
    T.mark(fx - 0.7, fz - 0.7, fx + 0.7, fz + 0.7, 'o')
    # a cooking pot on a tripod, a totem pole, a chest, barrels, bone piles
    px_, pz_ = 50.6, 82.6
    for a in (0.2, 2.3, 4.4):
        T.obox((px_ + math.cos(a) * 0.45, pz_ + math.sin(a) * 0.45, 0.0), (px_, pz_, 1.2), 0.05, 0.05, 'wood_dark')
    T.cyl(px_, pz_, 0.3, 0.25, 0.55, 0.95, 8, 'iron')
    T.mark(px_ - 0.4, pz_ - 0.4, px_ + 0.4, pz_ + 0.4, 'o')
    T.box(48.4, 88.2 - 0.9, 48.6, 88.2 - 0.7, 0, 2.8, 'wood_dark')                                     # totem pole near the south stakes
    for k, hh in enumerate((0.9, 1.5, 2.1)):
        T.box(48.5 - 0.22, 87.3 - 0.22, 48.5 + 0.22, 87.3 + 0.22 + 0.0, hh, hh + 0.28, 'cloth_white' if k != 1 else 'cloth_red') if False else None
        T.box(48.1, 87.15, 48.9, 87.65, hh, hh + 0.3, 'cloth_white' if k != 1 else 'cloth_red')
    T.mark(48.1, 87.15, 48.9, 87.65, 'o')
    T.crate(54.5, 81.2, 0.8)
    T.crate(41.8, 76.4, 0.7)
    T.barrel(42.5, 83.0)
    T.barrel(55.0, 87.0)
    for _ in range(12):
        bx, bz = VR.uniform(41.5, 55.5), VR.uniform(74.5, 87.5)
        if T.grid[int(bz)][int(bx)] == '.':
            T.blob(bx, bz, 0.0, VR.uniform(0.08, 0.16), VR.uniform(0.06, 0.1), VR.uniform(0.04, 0.07), 'cloth_white', jitter=0.02, sub=1)
    T.sign(38.4, 79.0)                    # a warning sign at the way in
    camp_tiles = free_tiles(41, 74, 56, 88)
    T.pens.append(('goblincamp', 41, 74, 56, 88))
    taken = []
    populate('goblin', 'goblincamp', 6, camp_tiles, taken)
    populate('goblin_warrior', 'goblincamp', 2, camp_tiles, taken)
    populate('hobgoblin', 'goblincamp', 2, camp_tiles, taken)

    # ---- Millbrook: the gate, the main street, houses, the windmill, the square with its shops
    for gxp in (29.4, 34.6):
        T.box(gxp - 0.22, 96.3, gxp + 0.22, 96.75, 0, 3.4, 'timber')
        T.mark(int(gxp), 96, int(gxp) + 1, 97, 'o')
    T.box(29.0, 96.25, 35.0, 96.8, 3.2, 3.6, 'wood_dark')
    T.box(29.2, 96.2, 34.8, 96.85, 3.6, 3.7, 'roof_brown')
    T.cyl(32.0, 96.55, 0.55, 0.55, 3.25, 3.55, 8, 'gold', sx=1.0, sz=0.1)          # the sign plate (blank)
    person('guard_village', 36.5, 99.5, 180.0)

    def cottage(x0, z0, x1, z1, door, roof, plaster='plaster'):
        T.house(x0, z0, x1, z1, door, roof=roof, plaster=plaster)
        # a bit of garden at the door
        if door == 'E':   T.flower_patch(x1 + 0.9, (z0 + z1) / 2 - 1.8, 5, 0.5)
        elif door == 'W': T.flower_patch(x0 - 0.9, (z0 + z1) / 2 + 1.8, 5, 0.5)
    ROOFS = ['roof_red', 'roof_brown', 'roof_blue', 'roof_slate', 'roof_thatch', 'roof_red']
    for i, z0 in enumerate((99, 108, 117)):
        cottage(21, z0, 28, z0 + 7, 'E', ROOFS[i])
    for i, z0 in enumerate((148, 157, 166)):
        cottage(21, z0, 28, z0 + 7, 'E', ROOFS[i + 2])
        cottage(36, z0, 43, z0 + 7, 'W', ROOFS[i + 3], plaster='stone_light' if i == 1 else 'plaster')
    cottage(36, 110, 43, 117, 'W', 'roof_brown')
    cottage(36, 119, 43, 126, 'W', 'roof_blue')
    # the Miller's Arms: a bigger inn on the north-east corner of the street
    T.house(36, 98, 47, 108, 'W', roof='roof_red', wall_h=3.0, rise=2.2, flowers=True)
    T.obox((35.55, 102.3, 3.2), (34.6, 102.3, 3.2), 0.08, 0.08, 'iron')
    T.cyl(34.65, 102.0, 0.28, 0.28, 2.45, 2.65, 10, 'gold', sx=1, sz=0.12)
    # the windmill by the river
    mx_, mz_ = 13.0, 113.0
    T.cyl(mx_, mz_, 2.3, 1.55, 0.0, 4.4, 9, 'plaster', rot=0.2)
    T.cyl(mx_, mz_, 2.45, 2.45, 0.0, 0.45, 9, 'stone_dark', rot=0.2)
    T.cyl(mx_, mz_, 1.8, 0.0, 4.4, 6.6, 9, 'roof_brown', rot=0.2)
    T.box(mx_ - 0.45, mz_ + 1.45, mx_ + 0.45, mz_ + 2.1, 0, 1.9, 'wood_dark')                   # door
    hub_y = 5.0
    for k in range(4):
        a = math.radians(25 + k * 90)
        ex, eh = mx_ + 2.4, hub_y
        T.obox((ex, mz_, eh), (ex, mz_ + math.cos(a) * 4.3, eh + math.sin(a) * 4.3), 0.16, 0.12, 'wood_dark')
        T.obox((ex + 0.07, mz_ + math.cos(a) * 1.4, eh + math.sin(a) * 1.4), (ex + 0.07, mz_ + math.cos(a) * 4.2 + math.cos(a + 1.5708) * 0.0, eh + math.sin(a) * 4.2), 0.85, 0.04, 'cloth_white')
    T.cyl(ex, mz_, 0.2, 0.15, hub_y - 0.2, hub_y + 0.2, 6, 'iron')
    T.mark(mx_ - 2.3, mz_ - 2.3, mx_ + 2.3, mz_ + 2.3, '#')
    # a chapel with a squat tower, west of the street
    T.house(9, 150, 18, 160, 'E', roof='roof_slate', wall_h=3.2, rise=2.4, plaster='stone_light')
    T.box(10.5, 147.5, 14.0, 151.0, 0.0, 5.6, 'stone_light'); T.mark(10.5, 147.5, 14.0, 151.0, '#')
    T.cyl(12.25, 149.25, 2.4, 0.0, 5.6, 7.6, 4, 'roof_slate', rot=0.785)
    T.box(12.0, 149.0, 12.5, 149.5, 7.4, 8.4, 'gold')

    # the square: fountain, well, benches, lamps, and the five shops (a market stall each, the shopkeeper behind it)
    T.fountain(32.5, 136.5)
    T.well(24.5, 138.0)
    for (bx_, bz_) in ((29.0, 133.0), (36.0, 133.0), (29.0, 140.2), (36.0, 140.2)):
        T.bench(bx_, bz_)
    for (lx, lz) in ((20.5, 128.5), (45.0, 128.5), (20.5, 144.5), (45.0, 144.5), (32.0, 128.0), (32.0, 145.2)):
        T.lamp(lx, lz)
    T.stall(21.0, 129.0, 24.6, 131.0, 'cloth_red', 'cloth_white', ('iron', 'cloth_white', 'wood'))                   # weapons
    T.stall(25.4, 129.0, 29.0, 131.0, 'cloth_blue' if 'cloth_blue' in PAL else 'banner_blue', 'cloth_white', ('iron', 'stone', 'banner_blue'))   # armour
    T.stall(35.0, 129.0, 38.6, 131.0, 'cloth_green', 'cloth_white', ('stone_dark', 'glow', 'iron'))                 # ore
    T.stall(39.4, 129.0, 43.0, 131.0, 'gold', 'cloth_white', ('straw', 'cloth_red', 'flower_y'))                    # cook
    T.stall(21.0, 141.0, 24.6, 143.0, 'cloth_green', 'cloth_white', ('flower_y', 'leaf_b', 'cloth_red'))            # general store
    for (sx2, sz2) in ((30.0, 130.0), (31.2, 130.0)):
        pass
    person('brannock', 22.8, 129.5, 0.0)
    person('mira', 27.2, 129.5, 0.0)
    person('dunmore', 36.8, 129.5, 0.0)
    person('hilda', 41.2, 129.5, 0.0)
    person('marta', 22.8, 141.5, 0.0)
    person('gwen', 38.5, 141.5, 270.0)
    # two more stalls in the south row: the bowyer and the wizard
    T.stall(25.0, 141.0, 28.4, 143.0, 'wood_dark', 'cloth_white', ('straw', 'wood', 'iron'))                            # bows and arrows
    T.stall(40.2, 141.0, 43.8, 143.0, 'cloth_blue' if 'cloth_blue' in PAL else 'banner_blue', 'cloth_white', ('glow', 'banner_blue', 'gold'))   # runes
    person('larson', 26.7, 141.5, 0.0)
    person('aldric', 42.0, 141.5, 0.0)
    # Millbrook Bank, east of the square: the same building as the castle bank
    bank_at(6, 101, 'bank2')
    person('cedric', 53.6, 134.5, 270.0)
    person('wren', 53.6, 136.5, 270.0)
    # lamps and a barrel here and there along the street
    for z in range(104, 176, 12):
        T.lamp(29.5, z + 0.5); T.lamp(34.6, z + 0.5)

    # the second smithy (north-east): a furnace against the back wall, two anvils, Wilf the smith between them
    stone_wall(46, 111, 62, 111.5)
    stone_wall(46, 111.5, 46.5, 114)
    stone_wall(61.5, 111.5, 62, 114)
    T.stations.append(('furnace', 50.5, 112.5, 0.0)); T.mark(50, 112, 51, 113, 'o')
    T.stations.append(('anvil', 53.5, 115.5, 90.0)); T.mark(53, 115, 54, 116, 'o')
    T.stations.append(('anvil', 57.5, 115.5, 90.0)); T.mark(57, 115, 58, 116, 'o')
    person('wilf', 54.5, 115.5, 270.0)
    trough(59, 112, 61, 113)
    T.blob(52.7, 112.5, 0.0, 0.42, 0.34, 0.34, 'iron', jitter=0.05, sub=1); T.mark(52, 112, 53, 113, 'o')
    T.barrel(47.0, 117.5); T.barrel(61.4, 117.6); T.crate(48.2, 117.6, 0.7)
    T.sign(45.0, 118.3)
    # Millbrook mine: bare earth, lots of rock, a cart
    VROCKS = [('copper', 49, 121), ('copper', 52, 123), ('copper', 55, 121), ('tin', 50, 126), ('tin', 53, 127), ('tin', 57, 125),
              ('iron', 58, 121), ('iron', 60, 123), ('iron', 61, 126), ('iron', 56, 128), ('coal', 48, 128), ('coal', 51, 120),
              ('coal', 60, 128), ('coal', 62, 121), ('mithril', 62, 124), ('mithril', 62, 127)]
    for kind, rx, rz in VROCKS:
        T.rocks.append((rx + 0.5, rz + 0.5, kind, VR.uniform(0, 360)))
        T.mark(rx, rz, rx + 1, rz + 1, 'o')
    for _ in range(16):
        qx, qz = VR.uniform(47.5, 62.5), VR.uniform(119.5, 128.5)
        if T.grid[int(qz)][int(qx)] == '.':
            T.blob(qx, qz, 0.0, VR.uniform(0.1, 0.22), VR.uniform(0.1, 0.18), VR.uniform(0.06, 0.12), VR.choice(['rock', 'rock_dark', 'stone']), jitter=0.03, sub=1)
    T.box(53.4, 123.55, 54.6, 124.45, 0.22, 0.34, 'wood_dark')
    for (x0, z0, x1, z1) in ((53.4, 123.55, 54.6, 123.65), (53.4, 124.35, 54.6, 124.45), (53.4, 123.55, 53.5, 124.45), (54.5, 123.55, 54.6, 124.45)):
        T.box(x0, z0, x1, z1, 0.34, 0.72, 'wood')
    T.blob(54.0, 124.0, 0.55, 0.45, 0.32, 0.2, 'rock', jitter=0.06, sub=1)
    T.mark(53, 123, 55, 125, 'o')
    T.lamp(47.6, 120.2)

    # the farm: a barn, a fenced yard with hay, rows of crops and a scarecrow; Old Tom looks after it
    T.house(47, 147, 57, 156, 'W', roof='roof_brown', wall_h=3.0, rise=2.4, plaster='wood')
    fence(45, 146.3, 62, 146.3)
    for hx, hz in ((58, 148), (59, 148), (58, 149), (60, 150)):
        hay_bale(hx, hz)
    for rz in range(160, 173, 2):                                                    # crop rows
        T.box(46, rz, 61, rz + 0.5, 0.0, 0.14, 'dirt')
        for cxp in range(46, 61):
            if VR.random() < 0.9:
                T.box(cxp + 0.2, rz + 0.1, cxp + 0.35, rz + 0.25, 0.1, 0.55 + VR.uniform(0, 0.15), 'leaf_b' if (cxp + rz) % 3 else 'leaf_a')
    T.box(53.9, 164.4, 54.1, 164.6, 0.0, 1.7, 'wood_dark')                            # the scarecrow
    T.box(53.1, 164.35, 54.9, 164.65, 1.2, 1.3, 'wood_dark')
    T.box(53.75, 164.25, 54.25, 164.75, 1.7, 2.0, 'straw')
    T.box(53.6, 164.2, 54.4, 164.8, 2.0, 2.06, 'wood_dark')
    T.mark(53, 164, 55, 165, 'o')
    person('old_tom', 52.5, 157.8, 180.0)
    person('fisher', 6.5, 125.5, 270.0)
    T.sign(30.0, 169.5)


    # ============================================================================================================
    # THE CRYPT, THE CRAFTERS' YARD, AND THE FAR SOUTH: the lake, the troll mountains, the caves, the desert and the Abyss
    # ============================================================================================================
    NR = random.Random(31337)   # its own random stream: nothing else moves

    def pillar(x, z, h=2.6, broken=False, mat='stone_light'):
        T.cyl(x, z, 0.36, 0.32, 0, 0.30, 8, 'stone_dark')
        T.cyl(x, z, 0.27, 0.25, 0.30, h * (0.55 if broken else 1.0), 8, mat)
        if not broken:
            T.cyl(x, z, 0.36, 0.36, h, h + 0.22, 8, 'stone_dark')
        T.mark(x - 0.38, z - 0.38, x + 0.38, z + 0.38, 'o')

    def rock_field(x0, z0, x1, z1, density, keep_clear=(), mat=('rock', 'rock_dark'), sc=(0.8, 1.5)):
        for zz in range(z0, z1):
            for xx in range(x0, x1):
                if T.grid[zz][xx] != '.' or NR.random() > density:
                    continue
                if any(c[0] <= xx < c[2] and c[1] <= zz < c[3] for c in keep_clear):
                    continue
                T.rock(xx + NR.uniform(0.2, 0.8), zz + NR.uniform(0.2, 0.8), NR.uniform(*sc), NR.choice(mat))

    def far_rock(kind, x, z):
        T.rocks.append((x + 0.5, z + 0.5, kind, NR.uniform(0, 360)))
        T.mark(x, z, x + 1, z + 1, 'o')

    def rock_wall(x0, z0, x1, z1, h=3.2):
        T.box(x0, z0, x1, z1, 0, h, 'rock_dark')
        T.box(x0 - 0.05, z0 - 0.05, x1 + 0.05, z1 + 0.05, h, h + 0.25, 'rock')
        T.mark(x0, z0, x1, z1, '#')

    # ---- the crypt: a graveyard building in the south-west of the castle town, entered from the north
    CRYPT = (9, 53, 23, 63)
    RESERVED.append((8, 51, 24, 64))
    T.walls(CRYPT, 3.0, 'stone_dark', 0.6, doors=[('N', 15, 17)], door_h=2.7)
    T.plinth_ring(CRYPT, 0.4)
    for side, aa in (('S', 12.5), ('S', 19.0), ('W', 57.5), ('E', 57.5)):
        T.window(side, CRYPT, aa, 1.4, 2.3, 0.7, shutters=False)
    T.target = 'roof_crypt'
    T.gable_roof(9, 53, 23, 63, 3.0, 1.7, 0.6, 'roof_slate', axis='x', gable_mat='stone_dark', trim='stone_dark')
    T.target = None
    T.roof_rects['crypt'] = CRYPT
    for zz in (55.0, 58.0):                         # stone coffins along both walls (a clear lane runs between the rows)
        for (xa, xb) in ((10.6, 12.6), (19.4, 21.4)):
            T.box(xa, zz, xb, zz + 2.0, 0, 0.7, 'stone')
            T.box(xa - 0.1, zz - 0.1, xb + 0.1, zz + 2.1, 0.7, 0.86, 'stone_light')
            T.mark(xa, zz, xb, zz + 2.0, 'o')
    for (cx, cz) in ((14.0, 56.5), (18.0, 56.5), (14.0, 59.0), (18.0, 59.0)):   # four pillars with candles
        pillar(cx, cz, 2.9)
        T.cyl(cx, cz + 0.55, 0.04, 0.04, 0.0, 0.5, 5, 'cloth_white')
        T.cyl(cx, cz + 0.55, 0.045, 0.0, 0.5, 0.66, 5, 'glow')
    T.box(15.0, 61.4, 17.0, 62.2, 0, 0.9, 'stone')                              # an altar at the back
    T.box(14.9, 61.3, 17.1, 62.3, 0.9, 1.0, 'stone_light')
    T.mark(15.0, 61.4, 17.0, 62.2, 'o')
    T.sign(13.4, 50.6)
    T.pens.append(('crypt', 10, 54, 22, 62))
    taken = []
    crypt_tiles = free_tiles(10, 54, 22, 62)
    populate('skeleton', 'crypt', 5, crypt_tiles, taken)
    populate('zombie', 'crypt', 3, crypt_tiles, taken)
    populate('skeleton_warrior', 'crypt', 2, crypt_tiles, taken)
    populate('imp', 'crypt', 3, crypt_tiles, taken)
    populate('dark_wizard', 'crypt', 2, crypt_tiles, taken)
    person('king', 32.5, 7.5, 0.0)                                              # the king, in the throne hall
    person('barnaby', 6.5, 36.5, 270.0)                                         # the ferryman at the pier

    # ---- Millbrook's crafters' yard: a tanning rack, a potter's wheel, a jeweller's bench and Ellis's shop
    for (kind, sx, sz) in (('tanner', 50.5, 106.5), ('wheel', 54.5, 106.5), ('jeweller', 58.5, 106.5)):
        T.stations.append((kind, sx, sz, 0.0))
        T.mark(int(sx), int(sz), int(sx) + 1, int(sz) + 1, 'o')
    T.stall(47.6, 99.4, 51.2, 101.4, 'cloth_blue' if 'cloth_blue' in PAL else 'banner_blue', 'cloth_white', ('leaf_b', 'leaf_a', 'flower_y'))
    person('ellis', 49.4, 99.9, 0.0)
    T.stations.append(('range', 55.5, 101.5, 0.0)); T.mark(55, 101, 56, 102, 'o')       # cooking ranges: here, by the castle inn and at Lakeside
    T.stations.append(('range', 49.5, 54.5, 0.0)); T.mark(49, 54, 50, 55, 'o')
    T.stations.append(('range', 30.5, 193.5, 0.0)); T.mark(30, 193, 31, 194, 'o')
    T.lamp(61.4, 99.6)
    T.barrel(61.2, 103.8)
    T.sign(47.3, 108.6)
    RESERVED.append((46, 98, 63, 111))
    person('priest', 19.6, 154.5, 270.0)                                        # outside the chapel

    # ---- Lakeside (z 176-215): the lake in the west with a dock, a fishing hut, willows, and the wolf wood to the east
    DZ = 196
    for zz in (DZ, DZ + 1):
        for xx in range(8, wx(zz) + 1):
            T.grid[zz][xx] = '.'
        T.box(8.0, zz, wx(zz) + 0.4, zz + 1, 0.0, 0.12, 'wood', c=(10, zz))
    for xx in (9.5, 12.5, 15.5, 18.5):
        for zz in (DZ - 0.05, DZ + 2.05):
            T.box(xx - 0.1, zz - 0.1, xx + 0.1, zz + 0.1, -0.35, 0.5, 'wood_dark', c=(xx, zz))
    for (sx, sz, kind) in ((7, DZ, 'harpoon'), (7, DZ + 1, 'cage'), (10, DZ - 1, 'rod'), (13, DZ + 2, 'rod'), (16, DZ - 1, 'net'),
                           (10, DZ + 2, 'harpoon'), (17, DZ + 2, 'cage'), (14, DZ - 1, 'rod')):
        assert (sx, sz) in water and T.grid[sz][sx] == '~', (sx, sz)
        T.spots.append((sx + 0.5, sz + 0.5, kind))
    person('silas', wx(DZ) + 2.5, DZ + 0.5, 270.0)                              # the ferryman
    T.sign(wx(DZ) + 2.2, DZ - 1.8)
    T.stall(wx(DZ) + 3.0, DZ + 3.6, wx(DZ) + 6.6, DZ + 5.6, 'cloth_blue' if 'cloth_blue' in PAL else 'banner_blue', 'cloth_white', ('flower_y', 'wood', 'iron'))
    person('gerrant', wx(DZ) + 4.8, DZ + 4.1, 0.0)                              # fishing supplies
    T.house(21, 184, 28, 191, 'S', roof='roof_blue', wall_h=2.6, rise=1.7)      # a fisher's cottage
    T.flower_patch(25.0, 193.0, 6, 0.6)
    for zz in range(182, 214, 3):                                               # willows at the water's edge, oaks along the road
        T.tree(wx(zz) + 2.4, zz + 0.5, 'willow')
    for zz in range(178, 216, 5):
        T.tree(road2(zz) - 3.2, zz + 0.4, 'oak')
        T.tree(road2(zz) + 6.2, zz + 0.4, 'oak')
    fplaced = []
    for _ in range(120):                                                        # the wolf wood: yews and oaks and the odd pine
        xx, zz = NR.randint(40, 62), NR.randint(178, 214)
        if T.grid[zz][xx] == '.' and not any((xx - a) ** 2 + (zz - b) ** 2 < 5 for a, b in fplaced):
            T.tree(xx + NR.uniform(0.2, 0.8), zz + NR.uniform(0.2, 0.8), NR.choice(['yew', 'yew', 'oak', 'pine']))
            fplaced.append((xx, zz))
    def grove(kind, spots):
        placed = 0
        for (gx, gz) in spots:
            if 0 <= gz < H and 0 <= gx < W and T.grid[gz][gx] == '.':
                T.tree(gx + 0.5, gz + 0.5, kind)
                placed += 1
        print('grove', kind, placed, 'of', len(spots))
    grove('maple', [(44, 181), (47, 185), (43, 189), (48, 193), (44, 197), (50, 199), (41, 201)])     # the edge of the wolf wood
    grove('teak', [(30, 200), (32, 203), (29, 207), (33, 210), (31, 213), (35, 205)])                  # south of the lake, beside the clay pit
    grove('mahogany', [(55, 204), (58, 207), (56, 211), (59, 213), (52, 209)])                         # deep in the dark wood
    for (cx, cz) in ((24, 206), (26, 209), (23, 211), (27, 205)):               # a clay pit on the lake's south shore
        if T.grid[cz][cx] == '.':
            far_rock('clay', cx, cz)
    for _ in range(18):
        xx, zz = NR.randint(wx(200) + 3, 62), NR.randint(177, 215)
        if T.grid[zz][xx] == '.' and all(abs(xx - road2(zz) - 1) > 3 for _ in (0,)):
            T.flower_patch(xx + .5, zz + .5, NR.randint(4, 8), 0.7)
    T.pens.append(('wolfwood', 42, 178, 62, 200))
    T.pens.append(('darkwood', 46, 202, 62, 216))
    taken = []
    populate('wolf', 'wolfwood', 5, free_tiles(42, 178, 62, 200), taken)
    taken = []
    populate('dark_wolf', 'darkwood', 2, free_tiles(46, 202, 62, 216), taken)
    populate('wolf', 'darkwood', 2, free_tiles(46, 202, 62, 216), taken)
    taken = []
    populate('bandit', 'wolfwood', 3, free_tiles(42, 178, 62, 200), taken)
    T.sign(road2(178) + 4.6, 177.6)

    # ---- the troll mountains (z 216-239): a rocky climb, a highland quarry and a pass full of trolls
    rock_field(0, 216, 64, 240, 0.12, keep_clear=((road2(216) - 5, 216, road2(216) + 8, 240), (3, 218, 21, 235), (36, 217, 62, 239)))
    rock_field(36, 217, 62, 239, 0.03)
    for (kind, qx, qz) in (('gold', 6, 222), ('gold', 10, 224), ('gold', 14, 222), ('silver', 7, 227), ('silver', 11, 229), ('silver', 15, 227),
                           ('gems', 9, 232), ('gems', 16, 231), ('clay', 18, 224)):
        far_rock(kind, qx, qz)
    T.pens.append(('trollpass', 38, 218, 60, 238))
    taken = []
    populate('mountain_troll', 'trollpass', 3, free_tiles(38, 218, 60, 238), taken)
    populate('ogre', 'trollpass', 2, free_tiles(38, 218, 60, 238), taken)
    grove('magic', [(46, 221), (49, 224), (53, 222), (57, 226), (44, 229), (55, 232)])                   # a hidden grove, guarded by the trolls
    T.pens.append(('highland', 3, 219, 21, 235))
    taken = []
    populate('barbarian', 'highland', 3, free_tiles(3, 219, 21, 235), taken)
    populate('hill_giant', 'highland', 2, free_tiles(3, 219, 21, 235), taken)
    T.sign(road2(217) + 4.6, 217.6)
    person('bram', 29.6, 237.6, 0.0)                                            # at the mouth of the caves
    for xx in (28.9, 34.1):                                                     # a timber gate over the cave road
        T.box(xx - 0.25, 239.3, xx + 0.25, 239.8, 0, 3.6, 'timber')
        T.mark(int(xx), 239, int(xx) + 1, 240, 'o')
    T.box(28.6, 239.25, 34.4, 239.85, 3.4, 3.8, 'wood_dark')

    # ---- the mining caves (z 240-271): a walled cavern the road runs straight through, spiders and rats in both wings
    rock_wall(0, 240, 29, 241)
    rock_wall(35, 240, 64, 241)
    rock_wall(0, 271, 29, 272)
    rock_wall(35, 271, 64, 272)
    rock_wall(0, 241, 5, 271)
    rock_wall(59, 241, 64, 271)
    for xx in (29.0, 34.0):                                                     # the entrance and exit are framed by rock pillars
        for zz in (240.0, 271.0):
            T.cyl(xx, zz + 0.5, 0.9, 0.7, 0, 4.2, 7, 'rock')
    for (cx, cz) in ((14, 245), (20, 259), (47, 251), (52, 262), (12, 266), (44, 245)):   # stalagmites
        T.cyl(cx, cz, 0.5, 0.0, 0, 2.0 + NR.uniform(0, 1.2), 6, 'rock_dark')
        T.cyl(cx + 0.7, cz + 0.4, 0.3, 0.0, 0, 1.2, 5, 'rock')
    for zz in range(244, 270, 8):                                               # lanterns on the walls and timber supports in the corridor
        T.lamp(6.0, zz + 0.5)
        T.lamp(57.5, zz + 0.5)
        for xx in (29.8, 35.2):
            T.box(xx - 0.12, zz - 0.12, xx + 0.12, zz + 0.12, 0, 3.0, 'wood_dark')
        T.box(29.7, zz - 0.14, 35.3, zz + 0.14, 3.0, 3.25, 'wood_dark')
    for (kind, qx, qz) in (('coal', 7, 246), ('coal', 9, 253), ('iron', 8, 259), ('iron', 12, 263), ('mithril', 6, 268), ('gold', 15, 246),
                           ('silver', 18, 256), ('gems', 11, 243), ('clay', 21, 267),
                           ('coal', 52, 246), ('coal', 55, 254), ('iron', 56, 261), ('mithril', 53, 267), ('gold', 46, 250), ('silver', 49, 265),
                           ('gems', 57, 244), ('clay', 43, 259), ('mithril', 45, 268)):
        far_rock(kind, qx, qz)
    rock_field(6, 242, 28, 271, 0.05, mat=('rock_dark', 'rock'))
    rock_field(38, 242, 59, 271, 0.05, mat=('rock_dark', 'rock'))
    T.pens.append(('cavew', 6, 242, 26, 270))
    T.pens.append(('cavee', 38, 242, 58, 270))
    taken = []
    populate('giant_spider', 'cavew', 5, free_tiles(6, 242, 26, 270), taken)
    populate('giant_rat', 'cavew', 4, free_tiles(6, 242, 26, 270), taken)
    populate('cave_spider', 'cavew', 4, free_tiles(6, 242, 26, 270), taken)
    populate('giant_mole', 'cavew', 2, free_tiles(6, 242, 26, 270), taken)
    taken = []
    populate('giant_spider', 'cavee', 5, free_tiles(38, 242, 58, 270), taken)
    populate('giant_rat', 'cavee', 4, free_tiles(38, 242, 58, 270), taken)
    populate('cave_spider', 'cavee', 4, free_tiles(38, 242, 58, 270), taken)
    populate('giant_mole', 'cavee', 2, free_tiles(38, 242, 58, 270), taken)

    # ---- the desert ruins (z 272-303): colonnades, obelisks, mummies, and the dragon's temple in the south-east
    for zz in range(276, 303, 5):                                               # a ruined colonnade either side of the road
        pillar(27.5, zz + 0.5, 3.0, broken=NR.random() < 0.4)
        pillar(37.5, zz + 0.5, 3.0, broken=NR.random() < 0.4)
    for (ox, oz) in ((9, 280), (55, 280), (10, 298), (22, 292)):                # obelisks
        T.box(ox - 0.35, oz - 0.35, ox + 0.35, oz + 0.35, 0, 0.5, 'stone_dark')
        T.box(ox - 0.25, oz - 0.25, ox + 0.25, oz + 0.25, 0.5, 3.4, 'stone_light')
        T.cyl(ox + 0.0, oz + 0.0, 0.25, 0.0, 3.4, 3.9, 4, 'gold', rot=0.78)
        T.mark(ox - 0.35, oz - 0.35, ox + 0.35, oz + 0.35, 'o')
    for (wx0, wz0, wx1, wz1) in ((6, 286, 16, 286.7), (6, 286, 6.7, 292), (46, 276, 56, 276.7), (56, 276, 56.7, 282), (12, 300, 22, 300.7)):   # broken walls
        T.box(wx0, wz0, wx1, wz1, 0, 1.4 + NR.uniform(-0.3, 0.5), 'stone')
        T.mark(wx0, wz0, wx1, wz1, 'o')
    for _ in range(40):                                                         # dunes of rubble and dead scrub
        xx, zz = NR.randint(1, 62), NR.randint(273, 302)
        if T.grid[zz][xx] == '.' and abs(xx - 33) > 5:
            T.blob(xx + NR.uniform(0.2, 0.8), zz + NR.uniform(0.2, 0.8), 0.0, NR.uniform(0.3, 0.8), NR.uniform(0.3, 0.7), NR.uniform(0.15, 0.4), 'sand', jitter=0.1, sub=1)
    for _ in range(16):
        xx, zz = NR.randint(2, 61), NR.randint(273, 302)
        if T.grid[zz][xx] == '.' and abs(xx - 33) > 5:
            T.rock(xx + .5, zz + .5, NR.uniform(0.6, 1.2), 'stone')
    # the dragon's temple: a walled platform with columns, bone piles and a hoard
    for (a, b, c, d) in ((44, 290, 64, 291), (44, 290, 45, 297), (44, 299.5, 45, 304), (62, 290, 64, 304)):
        T.box(a, b, c, d, 0, 1.6, 'stone_dark')
        T.mark(a, b, c, d, 'o')
    for (cx, cz) in ((48, 293), (48, 301), (58, 293), (58, 301)):
        pillar(cx + 0.5, cz + 0.5, 3.4, broken=(cx + cz) % 2 == 0)
    for _ in range(14):
        xx, zz = NR.randint(46, 61), NR.randint(292, 302)
        T.blob(xx + NR.uniform(0.2, 0.8), zz + NR.uniform(0.2, 0.8), 0.0, NR.uniform(0.15, 0.3), NR.uniform(0.12, 0.25), NR.uniform(0.1, 0.18), 'bone', jitter=0.05, sub=1)
    for _ in range(26):                                                         # the hoard
        T.blob(54.0 + NR.uniform(-2, 2), 297.0 + NR.uniform(-1.5, 1.5), 0.0, NR.uniform(0.15, 0.3), NR.uniform(0.15, 0.3), NR.uniform(0.08, 0.16), 'gold', jitter=0.04, sub=1)
    T.sign(road2(274) + 4.6, 273.6)
    T.pens.append(('ruinsw', 6, 274, 24, 290))
    T.pens.append(('ruinse', 40, 274, 58, 288))
    T.pens.append(('lair', 46, 292, 61, 303))
    taken = []
    populate('desert_mummy', 'ruinsw', 3, free_tiles(6, 274, 24, 290), taken)
    populate('desert_mummy', 'ruinse', 3, free_tiles(40, 274, 58, 288), taken)
    populate('scorpion', 'ruinsw', 4, free_tiles(6, 274, 24, 290), taken)
    populate('scorpion', 'ruinse', 4, free_tiles(40, 274, 58, 288), taken)
    populate('jackal', 'ruinsw', 3, free_tiles(6, 274, 24, 290), taken)
    populate('jackal', 'ruinse', 3, free_tiles(40, 274, 58, 288), taken)
    taken = []
    populate('dragon', 'lair', 1, free_tiles(51, 296, 58, 300), taken)
    populate('baby_dragon', 'lair', 2, free_tiles(47, 293, 60, 302), taken)

    # ---- the Abyss (z 304-335): black ground, red cracks, braziers and the wanderers
    for zz in (304.0,):                                                         # the warning gate
        for xx in (28.0, 36.0):
            T.cyl(xx, zz + 0.5, 0.7, 0.55, 0, 4.4, 7, 'obsidian')
            T.mark(int(xx) - 1, int(zz), int(xx) + 1, int(zz) + 1, 'o')
        T.box(27.4, zz + 0.1, 36.6, zz + 0.9, 4.3, 4.9, 'obsidian')
        for kx in (30.0, 32.0, 34.0):
            T.blob(kx, zz + 0.5, 4.95, 0.25, 0.25, 0.3, 'bone', jitter=0.03, sub=1)
    T.sign(road2(303) - 3.8, 302.6)
    for _ in range(46):                                                         # glowing cracks in the ground
        cx, cz = NR.uniform(2, 62), NR.uniform(306, 334)
        if abs(cx - 33) < 4.5:
            continue
        ang = NR.uniform(0, 3.14)
        for k in range(NR.randint(3, 6)):
            T.box(cx + math.cos(ang) * k * 0.7 - 0.07, cz + math.sin(ang) * k * 0.7 - 0.05, cx + math.cos(ang) * k * 0.7 + 0.35, cz + math.sin(ang) * k * 0.7 + 0.07,
                  0.0, 0.025, 'lava')
    for (bx, bz) in ((26, 310), (40, 310), (14, 318), (52, 318), (26, 326), (40, 326), (8, 330), (58, 330)):   # braziers
        T.cyl(bx + 0.5, bz + 0.5, 0.42, 0.30, 0, 0.9, 8, 'obsidian')
        T.cyl(bx + 0.5, bz + 0.5, 0.34, 0.30, 0.9, 1.0, 8, 'lava')
        T.cyl(bx + 0.5, bz + 0.5, 0.18, 0.0, 1.0, 1.55, 6, 'glow')
        T.mark(bx, bz, bx + 1, bz + 1, 'o')
    for _ in range(28):                                                         # broken pillars and bone heaps
        xx, zz = NR.randint(2, 61), NR.randint(307, 334)
        if T.grid[zz][xx] == '.' and abs(xx - 33) > 4:
            if NR.random() < 0.5:
                pillar(xx + 0.5, zz + 0.5, NR.uniform(2.2, 3.6), broken=NR.random() < 0.6, mat='obsidian')
            else:
                for _k in range(4):
                    T.blob(xx + NR.uniform(0.1, 0.9), zz + NR.uniform(0.1, 0.9), 0.0, NR.uniform(0.12, 0.25), NR.uniform(0.1, 0.2), NR.uniform(0.08, 0.15), 'bone', jitter=0.04, sub=1)
    for _ in range(10):                                                         # dead trees
        xx, zz = NR.randint(2, 61), NR.randint(307, 334)
        if T.grid[zz][xx] == '.' and abs(xx - 33) > 4:
            T.cyl(xx + 0.5, zz + 0.5, 0.2, 0.08, 0, 2.2, 5, 'obsidian')
            T.obox((xx + 0.5, zz + 0.5, 1.5), (xx + 1.2, zz + 0.2, 2.4), 0.1, 0.1, 'obsidian')
            T.mark(xx, zz, xx + 1, zz + 1, 'o')
    T.pens.append(('abyss', 4, 307, 60, 334))
    taken = []
    abyss_tiles = free_tiles(4, 307, 60, 334)
    populate('abyss_warrior', 'abyss', 4, abyss_tiles, taken)
    populate('abyss_archer', 'abyss', 3, abyss_tiles, taken)
    populate('abyss_mage', 'abyss', 3, abyss_tiles, taken)
    populate('abyss_champion', 'abyss', 2, abyss_tiles, taken)
    populate('hellhound', 'abyss', 3, abyss_tiles, taken)
    populate('black_knight', 'abyss', 2, abyss_tiles, taken)
    populate('black_dragon', 'abyss', 1, free_tiles(20, 326, 44, 334), taken)
    for (qx, qz) in ((7, 312), (57, 315), (9, 326), (56, 328)):
        far_rock('adamant', qx, qz)

    # ---- trees, rocks & flowers ----------------------------------------------------------------
    def free(x, z, pad=1):
        for dz in range(-pad, pad + 1):
            for dx in range(-pad, pad + 1):
                xx, zz = x + dx, z + dz
                if not (0 <= xx < W and 0 <= zz < H):
                    continue
                if T.grid[zz][xx] != '.':
                    return False
                if any(r[0] <= xx < r[2] and r[1] <= zz < r[3] for r in RESERVED):   # animal pens and their gates
                    return False
                for rect, a, b in T.floors:
                    if a in ('cobble_a', 'plaza_a', 'floor_a', 'carpet', 'wood_floor', 'dirt') and rect[0] <= xx < rect[2] and rect[1] <= zz < rect[3]:
                        return False
        return True
    placed = []
    tries = 0
    while len(placed) < 210 and tries < 6000:
        tries += 1
        x, z = R.randint(1, W - 2), R.randint(1, HT - 2)
        edge = min(x, z, W - 1 - x, HT - 1 - z)
        if x < wx(z) + 2:
            continue
        if not free(x, z, 1):
            continue
        if R.random() > (0.95 if edge < 5 else 0.15):
            continue
        if any((x - px) ** 2 + (z - pz) ** 2 < 6 for px, pz in placed):
            continue
        placed.append((x, z))
        T.tree(x + R.uniform(0.2, 0.8), z + R.uniform(0.2, 0.8))
    for _ in range(90):
        x, z = R.randint(wx(1) + 2, W - 2), R.randint(1, HT - 2)
        if free(x, z, 1):
            T.flower_patch(x + .5, z + .5, R.randint(4, 9), 0.7)
    for _ in range(26):
        x, z = R.randint(wx(1) + 2, W - 2), R.randint(1, HT - 2)
        if free(x, z, 1):
            T.bush(x + .5, z + .5, flowers=R.random() < 0.4)


    # ---- the road and Millbrook: trees to chop, flowers, bushes, boulders on the bank, and more fishing spots ------------------
    VH = 176   # (these loops stop at the end of Millbrook: the far south has its own scatter above)
    vplaced = []
    tries = 0
    while len(vplaced) < 170 and tries < 12000:
        tries += 1
        x, z = VR.randint(1, W - 2), VR.randint(HT, VH - 2)
        edge = min(x, W - 1 - x, VH - 1 - z)
        if x < wx(z) + 2 or not free(x, z, 1):
            continue
        if VR.random() > (0.9 if edge < 6 else (0.20 if z < 96 else 0.10)):
            continue
        if any((x - px) ** 2 + (z - pz) ** 2 < 6 for px, pz in vplaced):
            continue
        vplaced.append((x, z))
        T.tree(x + VR.uniform(0.2, 0.8), z + VR.uniform(0.2, 0.8))
    for _ in range(110):
        x, z = VR.randint(wx(HT) + 2, W - 2), VR.randint(HT, VH - 2)
        if free(x, z, 1):
            T.flower_patch(x + .5, z + .5, VR.randint(4, 9), 0.7)
    for _ in range(30):
        x, z = VR.randint(wx(HT) + 2, W - 2), VR.randint(HT, VH - 2)
        if free(x, z, 1):
            T.bush(x + .5, z + .5, flowers=VR.random() < 0.4)
    for z in range(HT + 2, VH - 2, 3):                                              # boulders along the bank
        T.rock(wx(z) + VR.uniform(0.3, 1.1), z + VR.uniform(0.2, 0.8), VR.uniform(0.7, 1.3), 'cliff' if VR.random() < .5 else 'rock')
    def shore_ok2(z):
        e = wx(z)
        return HT + 2 <= z < VH - 2 and T.grid[z][e] == '.' and T.grid[z][e + 1] == '.' and (e - 1, z) in water
    for z0 in (101, 112, 122, 132, 143, 152, 162, 171):
        for dz in (0, 1, -1, 2, -2, 3, -3):
            if shore_ok2(z0 + dz) and not any(abs(z0 + dz - int(sz_)) < 4 for sx_, sz_, _ in T.spots if sz_ > HT):
                T.spots.append((wx(z0 + dz) - 1 + 0.5, z0 + dz + 0.5, 'net'))
                break
    rr2 = random.Random(556)                                                       # reeds and lily pads for the river down here
    spot_tiles2 = {(int(sx_), int(sz_)) for sx_, sz_, _ in T.spots}
    for z in range(HT, VH - 1):
        x = wx(z) - 1
        if (x, z) in spot_tiles2 or (x, z) not in water or rr2.random() > 0.30:
            continue
        cx0, cz0 = x + rr2.uniform(0.55, 0.9), z + rr2.uniform(0.2, 0.8)
        for _ in range(rr2.randint(3, 5)):
            rx_, rz_, rh = cx0 + rr2.uniform(-0.18, 0.18), cz0 + rr2.uniform(-0.18, 0.18), rr2.uniform(0.55, 1.0)
            T.cyl(rx_, rz_, 0.02, 0.009, -0.34, rh, 5, 'leaf_c')
            if rr2.random() < 0.6:
                T.cyl(rx_, rz_, 0.028, 0.026, rh - 0.1, rh + 0.1, 5, 'trunk')
    for _ in range(40):
        z = rr2.randint(HT + 2, VH - 3)
        x = rr2.randint(0, max(0, wx(z) - 3))
        if (x, z) not in water or (x, z) in spot_tiles2:
            continue
        px, pz = x + rr2.uniform(0.2, 0.8), z + rr2.uniform(0.2, 0.8)
        T.cyl(px, pz, 0.30, 0.30, -0.335, -0.325, 8, 'leaf_b', rot=rr2.uniform(0, 6.28))
        if rr2.random() < 0.35:
            T.cyl(px + 0.04, pz, 0.07, 0.01, -0.325, -0.24, 6, 'flower_w')

    if B:   # grass tufts + pebbles on free grass tiles
        V = T.vrng
        for z in range(H):
            for x in range(W):
                if T.grid[z][x] != '.' or (x, z) in water or x <= wx(z) + 1:
                    continue
                if not (tile_mat(x, z) or '').startswith('grass'):
                    continue
                dense = z < 70 or 92 <= z < 180
                for _ in range(V.choice([1, 2, 2, 3]) if dense else (V.choice([1, 1, 2]) if z < 216 else V.choice([0, 1, 1]))):
                    T.grass_tuft(x + V.uniform(0.1, 0.9), z + V.uniform(0.1, 0.9), 0.34, V.choice([3, 4, 5]) if z < 216 else 3)
                if V.random() < 0.05:
                    T.blob(x + V.uniform(.2, .8), z + V.uniform(.2, .8), 0.02, 0.1, 0.08, 0.06, V.choice(['rock', 'stone', 'rock_dark']), jitter=0.02, sub=1)

    if WORLD:
        return T

    # ---- skirt: far ground + forest ring -------------------------------------------------------
    S = 56
    RW = 8   # the river runs RW tiles past the map's west edge; beyond that is a far bank with forest on it
    xe_n, xe_s = wx(0), wx(H - 1)   # where the near bank meets the skirt at the north / south map edge
    for (a, b, c, d) in ((-S, -S, -RW, 0), (xe_n, -S, W + S, 0), (-S, H, -RW, H + S), (xe_s, H, W + S, H + S),
                         (-S, 0, -RW, H), (W, 0, W + S, H)):
        T.quad(P(a, d, 0), P(c, d, 0), P(c, b, 0), P(a, b, 0), 'grass_dark', ((a + c) / 2, (b + d) / 2))
    # the river carries on through the skirt: west of the map, and north and south of it
    for (a, b, c, d) in ((-RW, 0, 0, H), (-RW, -S, xe_n, 0), (-RW, H, xe_s, H + S)):
        T.quad(P(a, d, -0.35), P(c, d, -0.35), P(c, b, -0.35), P(a, b, -0.35), 'water', ((a + c) / 2, (b + d) / 2))
    # far bank: a low cliff face down to the water and a sandy strip on top
    T.quad(P(-RW, -S, -0.35), P(-RW, H + S, -0.35), P(-RW, H + S, 0), P(-RW, -S, 0), 'cliff', (-RW, H / 2))
    T.quad(P(-RW - 1.4, H + S, 0.012), P(-RW, H + S, 0.012), P(-RW, -S, 0.012), P(-RW - 1.4, -S, 0.012), 'sand', (-RW - 1, H / 2))
    for _ in range(int(240 * (H + 2 * S) / (HT + 2 * S))):
        x, z = R.uniform(-S + 4, W + S - 4), R.uniform(-S + 4, H + S - 4)
        if -1 <= x <= W + 1 and -1 <= z <= H + 1:
            continue
        if -RW - 2.5 < x < 6:   # in the river (the far bank's trees start past the sand)
            continue
        T.tree(x, z, lod=1)
    return T


def make_objects(T):
    if B:
        T.build_shadow_bvh()
    for key, bm in T.bms.items():
        T.apply_ao(bm)
        if B:
            T.apply_uv(bm)
    objs = {}
    for key, bm in T.bms.items():
        if key == '_discard':
            bm.free()
            continue
        name = key if isinstance(key, str) else 'chunk_%d_%d' % key
        me = bpy.data.meshes.new(name)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(ob)
        for m in T.matlist:
            ob.data.materials.append(MATS[m])
        objs[name] = ob
    return objs


MATS = {}


def build_tree_templates(T):
    """One standalone model per tree kind plus the stump, all at the origin (game scales/rotates/places them)."""
    for seed, kind in ((11, 'round'), (12, 'pine'), (13, 'tall'), (14, 'oak'), (15, 'willow'), (16, 'yew'), (17, 'teak'), (18, 'maple'), (19, 'mahogany'), (20, 'magic')):
        T.vrng = random.Random(seed)
        T.target = 'tree_' + kind
        T.tree_geom(0.0, 0.0, kind, 1.0, 0.0, 0)
    T.target = 'stump'
    T.stump()
    T.target = None


def srgb_to_lin(a):
    return np.where(a <= 0.04045, a / 12.92, ((a + 0.055) / 1.055) ** 2.4)


def lin_to_srgb(a):
    a = np.clip(a, 0, 1)
    return np.where(a <= 0.0031308, a * 12.92, 1.055 * a ** (1 / 2.4) - 0.055)


def tinted_texture(mat):
    """Procedural texture tinted so its average colour equals the palette colour (contrast preserved)."""
    try:
        from PIL import Image
    except ImportError:
        Image = None
    sys.path.insert(0, os.path.dirname(__file__))
    import textures_proc as tp
    name = TEXMAP[mat][0]
    if name not in _TEX_CACHE:
        _TEX_CACHE[name] = tp.TEXTURES[name]()
    lin = srgb_to_lin(_TEX_CACHE[name])
    target = np.array(hexlin(PAL[mat])[:3])
    ratio = target / np.maximum(lin.reshape(-1, 3).mean(0), 1e-4)
    out = lin_to_srgb(lin * ratio[None, None, :])
    d = os.path.join(ROOT, 'build', 'tex_b')
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, mat + '.png')
    px = (out * 255 + 0.5).astype(np.uint8)
    if Image is not None:
        Image.fromarray(px).save(path)
    else:   # Blender's bundled Python has no Pillow: write the PNG with bpy instead (rows run bottom-to-top there)
        h, w = px.shape[:2]
        im = bpy.data.images.new('_tinted_tmp', w, h, alpha=False)
        im.colorspace_settings.name = 'sRGB'
        rgba = np.ones((h, w, 4), dtype=np.float32)
        rgba[..., :3] = np.flipud(px[..., :3]).astype(np.float32) / 255.0
        im.pixels.foreach_set(rgba.ravel())
        im.filepath_raw = path
        im.file_format = 'PNG'
        im.save()
        bpy.data.images.remove(im)
    return path


_TEX_CACHE = {}


def make_materials(names):
    for n in names:
        m = bpy.data.materials.new(n)
        m.use_nodes = True
        bsdf = m.node_tree.nodes.get('Principled BSDF')
        bsdf.inputs['Base Color'].default_value = hexlin(PAL[n])
        bsdf.inputs['Roughness'].default_value = 1.0
        if 'Specular IOR Level' in bsdf.inputs:
            bsdf.inputs['Specular IOR Level'].default_value = 0.0
        if n in ('glow', 'lava'):
            bsdf.inputs['Emission Color'].default_value = hexlin(PAL[n])
            bsdf.inputs['Emission Strength'].default_value = 1.0
        if B and n in TEXMAP:
            img = bpy.data.images.load(tinted_texture(n))
            img.colorspace_settings.name = 'sRGB'
            img.name = n
            tex = m.node_tree.nodes.new('ShaderNodeTexImage')
            tex.image = img
            tex.interpolation = 'Linear'
            m.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
        MATS[n] = m


def export_all(objs):
    os.makedirs(OUT_MODELS, exist_ok=True)
    for f in os.listdir(OUT_MODELS):
        if f.endswith('.glb'):
            os.remove(os.path.join(OUT_MODELS, f))
    for name, ob in objs.items():
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.gltf(filepath=os.path.join(OUT_MODELS, name + '.glb'), export_format='GLB',
                                  use_selection=True, export_yup=True, export_apply=True, export_normals=True,
                                  export_vertex_color='ACTIVE', export_cameras=False, export_lights=False,
                                  export_materials='EXPORT', export_image_format=('JPEG' if B else 'NONE'), export_jpeg_quality=90)
        print('exported', name, len(ob.data.polygons), 'faces')


AREAS = [  # later entries win where they overlap
    (0, 0, 64, 64, 'Journey Valley'),
    (0, 0, 8, 64, 'River Bank'),
    (0, 30, 8, 38, 'Fishing Pier'),
    (9, 8, 17, 20, 'Chicken Coop'),
    (50, 6, 62, 20, 'Cow Pen'),
    (9, 26, 57, 64, 'Castle Town'),
    (54, 51, 63, 56, 'Smithy'),
    (53, 57, 63, 63, 'Quarry'),
    (0, 64, 64, 176, 'Millbrook Road'),
    (39, 72, 58, 90, 'Goblin Camp'),
    (0, 96, 64, 176, 'Millbrook Village'),
    (19, 127, 46, 146, 'Millbrook Square'),
    (46, 111, 63, 119, 'Millbrook Smithy'),
    (47, 119, 64, 130, 'Millbrook Mine'),
    (45, 146, 63, 176, 'Old Tom\'s Farm'),
    (8, 146, 19, 162, 'Millbrook Chapel'),
    (25, 35, 40, 46, 'Market Square'),
    (18, 3, 47, 25, 'Castle Courtyard'),
    (24, 6, 41, 16, 'Throne Hall'),
    (42, 31, 50, 38, 'Bank'),
    (48, 132, 56, 139, 'Millbrook Bank'),
    (47, 99, 62, 110, "Millbrook Crafters' Yard"),
    (42, 44, 52, 52, 'The Journey\'s Rest Inn'),
    (10, 54, 22, 62, 'Castle Crypt'),
    (0, 176, 64, 216, 'Lakeside'),
    (42, 178, 62, 214, 'Wolf Wood'),
    (0, 216, 64, 240, 'Troll Mountains'),
    (0, 240, 64, 272, 'Mining Caves'),
    (0, 272, 64, 304, 'Desert Ruins'),
    (44, 290, 62, 304, "The Dragon's Temple"),
    (0, 304, 64, 336, 'The Abyss'),
]


def write_map(T, spawn):
    os.makedirs(os.path.dirname(OUT_MAP), exist_ok=True)
    with open(OUT_MAP, 'w') as f:
        f.write('// Journey tile map. Generated by tools/build_town.py - edit the script, not this file.\n')
        f.write('// . walkable   D door   # wall/building   ~ water   T tree   o prop (blocked)   f fence (blocked, but arrows and spells fly over)\n')
        f.write('@size %d %d\n@spawn %d %d\n' % (W, H, spawn[0], spawn[1]))
        for name, r in T.roof_rects.items():
            f.write('@roof %s %d %d %d %d\n' % (name, *[int(v) for v in r]))
        areas = list(AREAS)
        if WORLD:   # the wild places first (they are broad), then the core's own, shifted to where it stands now
            areas = list(WM.WILD_AREAS) + [(x0 + WM.OX, z0 + WM.OZ, x1 + WM.OX, z1 + WM.OZ, name) for (x0, z0, x1, z1, name) in AREAS] + list(getattr(T, 'extra_areas', []))
        for (x0, z0, x1, z1, name) in areas:
            f.write('@area %d %d %d %d %s\n' % (x0, z0, x1, z1, name))
        for (x, z, kind, sc, rot) in T.trees:
            f.write('@tree %.2f %.2f %s %.2f %.1f\n' % (x, z, kind, sc, math.degrees(rot)))
        for (x, z, kind) in T.spots:
            f.write('@spot %.2f %.2f %s\n' % (x, z, kind))
        for (x, z, kind, yaw) in T.rocks:
            f.write('@rock %.2f %.2f %s %.1f\n' % (x, z, kind, yaw))
        for (kind, x, z, yaw) in T.stations:
            f.write('@station %s %.2f %.2f %.1f\n' % (kind, x, z, yaw))
        for (key, x, z, yaw) in T.persons:
            f.write('@person %s %.2f %.2f %.1f\n' % (key, x, z, yaw))
        for (name, x0, z0, x1, z1) in T.pens:
            f.write('@pen %s %d %d %d %d\n' % (name, x0, z0, x1, z1))
        for (species, x, z, pen) in T.npcs:
            f.write('@npc %s %.2f %.2f %s\n' % (species, x, z, pen))
        f.write('@rows\n')
        for row in T.grid:
            f.write(''.join(row) + '\n')
    if WORLD:   # the Abyss is where the best resources are (tools/abyss_riches.py)
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import abyss_riches
        abyss_riches.apply_file(OUT_MAP)


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    make_materials(PAL.keys())
    T = build()
    spawn = (32, 30)
    if WORLD:
        import world_wilds
        T = world_wilds.expand(T, sys.modules[__name__])
        spawn = (32 + WM.OX, 30 + WM.OZ)
    build_tree_templates(T)
    objs = make_objects(T)
    export_all(objs)
    write_map(T, spawn)
    if '--blend' in sys.argv:
        src = os.path.join(ROOT, 'source')
        os.makedirs(src, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(src, 'town.blend'))
    walk = sum(1 for r in T.grid for c in r if c in '.D')
    print('map %dx%d, walkable tiles: %d' % (W, H, walk))
