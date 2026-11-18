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

W = H = 64
CH = 16  # chunk size in tiles
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
    'leaf_a': '#3f9030', 'leaf_b': '#52a63a', 'leaf_c': '#2e7b2c', 'leaf_pine': '#2a6a35',
    'trunk': '#6b4527', 'flower_y': '#f5d33f', 'flower_r': '#e2473d', 'flower_w': '#f5f2ea',
    'flower_p': '#a65ec4', 'cloth_white': '#f1ece0', 'cloth_red': '#c4372f', 'cloth_green': '#3f8050',
    'glow': '#ffd37a', 'straw': '#d9bb62', 'hedge': '#2d6b2a', 'skin': '#e0b48a',
    'tuft': '#78b648', 'foam': '#d3e8ee',
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
    'hedge': ('leaves', 2.6), 'trunk': ('bark', 1.4), 'carpet': ('carpet', 1.0),
}
GROUND_MATS = {'tuft', 'foam', 'grass_a', 'grass_b', 'grass_c', 'grass_dark', 'dirt', 'sand', 'cobble_a', 'cobble_b',
               'plaza_a', 'plaza_b', 'water', 'water_b', 'water_c', 'water_deep', 'floor_a', 'floor_b', 'wood_floor'}


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
            key = (min(3, max(0, int(c[0] // CH))), min(3, max(0, int(c[1] // CH))))
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
        if B:
            return s.tree_b(x, z, kind, sc, rot, lod)
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
        mats = ['leaf_a', 'leaf_b', 'leaf_c']
        shade = (base - 1.1 * sc, base + 1.9 * sc)
        s.blob(cx, cz, base + 0.15 * sc, 1.25 * sc, 1.25 * sc, 0.95 * sc, 'leaf_a', jitter=0.14, sub=sub, shade=shade)
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
                   r.choice(['leaf_b', 'leaf_a']), jitter=0.1, sub=sub, shade=shade)

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
                        if mname.startswith('grass') or mname in ('dirt', 'sand'):
                            v = (math.sin(gx * 0.31) + math.sin(gz * 0.27 + 1.3) + math.sin((gx + gz) * 0.13 + 0.6)) / 3.0
                            k *= 0.90 + 0.10 * v + 0.0
                        if z > -0.2:
                            k *= s.corner_ao(gx, gz)
                    if B and s.bvh is not None and z > -0.2 and not wet:
                        k *= s.shadow_k(l.vert.co.x, l.vert.co.y)
                elif B and mname == 'leaf_pine':
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
        return 5 + int(round(1.5 * math.sin(z / 6.0)))

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
    T.floor((42, 31, 50, 38), 'floor_a', 'floor_b')                    # bank interior
    T.floor((42, 44, 52, 52), 'wood_floor', 'wood_floor')              # inn interior

    # grass colour noise
    def tile_mat(x, z):
        m = None
        for rect, a, b in T.floors:
            if rect[0] <= x < rect[2] and rect[1] <= z < rect[3]:
                m = a if ((x + z) & 1) == 0 else b
        if m in ('grass_a', 'grass_b'):
            n = (x * 73856093 ^ z * 19349663) & 7
            m = ('grass_a', 'grass_b', 'grass_c', 'grass_a', 'grass_b', 'grass_a', 'grass_c', 'grass_dark')[n]
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
                if B:   # 4x4 sub-quads so baked sun shadows have 1/4-tile resolution
                    n = 4
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
    for z in range(2, H - 2, 3):
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

    # Bank (enterable, stone, columns)
    BANK = (42, 31, 50, 38)
    T.walls(BANK, 3.2, 'stone_light', 0.6, doors=[('W', 34, 36)], door_h=2.7)
    T.plinth_ring(BANK, 0.5)
    for a in (32.0, 37.0):
        T.side_box('N', BANK, 44.0, 45.0, 1.2, 2.4, 0.1, 'timber') if a == 32 else None
    for a in (33.0, 36.5):
        pass
    # windows (stone arch style)
    for side, aa in (('N', 44.5), ('N', 47.0), ('S', 44.5), ('S', 47.0), ('E', 33.5), ('E', 36.0)):
        T.window(side, BANK, aa, 1.2, 2.2, 0.8, shutters=False)
    # portico: two columns + small pediment over door
    for cz in (33.2, 36.8):
        T.cyl(41.0, cz, 0.28, 0.28, 0, 3.0, 8, 'stone_light')
        T.cyl(41.0, cz, 0.38, 0.38, 0, 0.25, 8, 'stone_dark'); T.cyl(41.0, cz, 0.38, 0.38, 2.9, 3.15, 8, 'stone_dark')
        T.mark(40.7, cz - 0.3, 41.3, cz + 0.3, 'o')
    T.box(40.6, 32.8, 42.0, 37.2, 3.15, 3.45, 'stone')
    T.tri_prism('x', 40.6, 42.0, 32.8, 37.2, 3.45, 4.3, 'stone_light')
    T.cyl(41.3, 35.0, 0.4, 0.4, 3.6, 3.66, 12, 'gold', sx=0.15, sz=1.0)
    # roof (hip-ish gable, red) separate so it hides when inside
    T.target = 'roof_bank'
    T.gable_roof(42, 31, 50, 38, 3.2, 2.0, 0.7, 'roof_red', axis='z', gable_mat='stone_light', trim='stone_dark')
    T.target = None
    T.roof_rects['bank'] = BANK
    # interior: counter + booths + chests
    T.box(46.3, 32.2, 47.0, 36.8, 0, 1.05, 'wood_dark'); T.box(46.2, 32.1, 47.2, 36.9, 1.05, 1.15, 'wood')
    for z in (33.4, 35.0, 36.6):
        pass
    for bz in (32.6, 34.2, 35.8):
        T.box(46.5, bz, 46.7, bz + 0.08, 1.15, 2.3, 'glass')
        T.box(46.5, bz + 0.0, 46.6, bz + 0.1, 1.15, 2.4, 'timber')
    T.mark(46, 32, 47.5, 37, 'o')
    for cz in (32.4, 36.0):
        T.box(43.2, cz, 44.2, cz + 0.8, 0, 0.7, 'wood_dark'); T.box(43.15, cz - 0.05, 44.25, cz + 0.85, 0.7, 0.82, 'wood')
        T.box(43.55, cz + 0.28, 43.85, cz + 0.45, 0.8, 0.9, 'gold'); T.mark(43.15, cz - 0.05, 44.25, cz + 0.85, 'o')
    T.floor((42, 34, 44, 36), 'carpet')
    T.box(48.4, 31.9, 48.8, 32.4, 0, 1.5, 'iron'); T.mark(48.3, 31.9, 49, 32.5, 'o')
    # bank sign above door

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
    fence(11, 38.6, 16, 38.6); T.mark(11, 38.4, 16, 38.9, 'o')
    fence(55, 38.6, 60, 38.6); T.mark(55, 38.4, 60, 38.9, 'o')
    for (fx, fz) in ((12.5, 38.0), (14.5, 38.0), (56.5, 38.0), (58.5, 38.0)):
        T.flower_patch(fx, fz, 6, 0.5)

    # ---- trees, rocks & flowers ----------------------------------------------------------------
    def free(x, z, pad=1):
        for dz in range(-pad, pad + 1):
            for dx in range(-pad, pad + 1):
                xx, zz = x + dx, z + dz
                if not (0 <= xx < W and 0 <= zz < H):
                    continue
                if T.grid[zz][xx] != '.':
                    return False
                for rect, a, b in T.floors:
                    if a in ('cobble_a', 'plaza_a', 'floor_a', 'carpet', 'wood_floor') and rect[0] <= xx < rect[2] and rect[1] <= zz < rect[3]:
                        return False
        return True
    placed = []
    tries = 0
    while len(placed) < 210 and tries < 6000:
        tries += 1
        x, z = R.randint(1, W - 2), R.randint(1, H - 2)
        edge = min(x, z, W - 1 - x, H - 1 - z)
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
        x, z = R.randint(wx(1) + 2, W - 2), R.randint(1, H - 2)
        if free(x, z, 1):
            T.flower_patch(x + .5, z + .5, R.randint(4, 9), 0.7)
    for _ in range(26):
        x, z = R.randint(wx(1) + 2, W - 2), R.randint(1, H - 2)
        if free(x, z, 1):
            T.bush(x + .5, z + .5, flowers=R.random() < 0.4)

    if B:   # grass tufts + pebbles on free grass tiles
        V = T.vrng
        for z in range(H):
            for x in range(W):
                if T.grid[z][x] != '.' or (x, z) in water or x <= wx(z) + 1:
                    continue
                if not (tile_mat(x, z) or '').startswith('grass'):
                    continue
                for _ in range(V.choice([1, 2, 2, 3])):
                    T.grass_tuft(x + V.uniform(0.1, 0.9), z + V.uniform(0.1, 0.9), 0.34, V.choice([3, 4, 5]))
                if V.random() < 0.05:
                    T.blob(x + V.uniform(.2, .8), z + V.uniform(.2, .8), 0.02, 0.1, 0.08, 0.06, V.choice(['rock', 'stone', 'rock_dark']), jitter=0.02, sub=1)

    # ---- skirt: far ground + forest ring -------------------------------------------------------
    S = 56
    for (a, b, c, d) in ((-S, -S, W + S, 0), (-S, H, W + S, H + S), (-S, 0, 0, H), (W, 0, W + S, H)):
        T.quad(P(a, d, 0), P(c, d, 0), P(c, b, 0), P(a, b, 0), 'grass_dark', ((a + c) / 2, (b + d) / 2))
    # river continues in the skirt
    T.quad(P(-S, H + S, -0.35), P(0, H + S, -0.35), P(0, -S, -0.35), P(-S, -S, -0.35), 'water', (-10, 32))
    for _ in range(240):
        x, z = R.uniform(-S + 4, W + S - 4), R.uniform(-S + 4, H + S - 4)
        if -1 <= x <= W + 1 and -1 <= z <= H + 1:
            continue
        if x < 2:   # river
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
    for seed, kind in ((11, 'round'), (12, 'pine'), (13, 'tall')):
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
        if n == 'glow':
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
    (9, 26, 57, 64, 'Castle Town'),
    (25, 35, 40, 46, 'Market Square'),
    (18, 3, 47, 25, 'Castle Courtyard'),
    (24, 6, 41, 16, 'Throne Hall'),
    (42, 31, 50, 38, 'Bank'),
    (42, 44, 52, 52, 'The Journey\'s Rest Inn'),
]


def write_map(T, spawn):
    os.makedirs(os.path.dirname(OUT_MAP), exist_ok=True)
    with open(OUT_MAP, 'w') as f:
        f.write('// Journey tile map. Generated by tools/build_town.py - edit the script, not this file.\n')
        f.write('// . walkable   D door   # wall/building   ~ water   T tree   o prop (blocked)\n')
        f.write('@size %d %d\n@spawn %d %d\n' % (W, H, spawn[0], spawn[1]))
        for name, r in T.roof_rects.items():
            f.write('@roof %s %d %d %d %d\n' % (name, *[int(v) for v in r]))
        for (x0, z0, x1, z1, name) in AREAS:
            f.write('@area %d %d %d %d %s\n' % (x0, z0, x1, z1, name))
        for (x, z, kind, sc, rot) in T.trees:
            f.write('@tree %.2f %.2f %s %.2f %.1f\n' % (x, z, kind, sc, math.degrees(rot)))
        f.write('@rows\n')
        for row in T.grid:
            f.write(''.join(row) + '\n')


if __name__ == '__main__':
    bpy.ops.wm.read_factory_settings(use_empty=True)
    make_materials(PAL.keys())
    T = build()
    build_tree_templates(T)
    objs = make_objects(T)
    export_all(objs)
    write_map(T, (32, 30))
    if '--blend' in sys.argv:
        src = os.path.join(ROOT, 'source')
        os.makedirs(src, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(src, 'town.blend'))
    walk = sum(1 for r in T.grid for c in r if c in '.D')
    print('map %dx%d, walkable tiles: %d' % (W, H, walk))
