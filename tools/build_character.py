#!/usr/bin/env python3
"""
Journey - modular character builder (Blender / bpy)

Builds every piece of the character creator as its own small GLB, in "pivot space" so the game can
animate rigid parts (swing about hip / shoulder, bob, ...) and tint each piece at runtime:

  body_<m|f>_<head|torso>.glb, body_arm_<l|r>.glb, body_leg_<l|r>.glb
  hair_<id>.glb
  shirt_<id>_torso_<m|f>.glb, shirt_<id>_sleeve_<l|r>.glb
  pants_<id>_hips_<m|f>.glb,  pants_<id>_leg_<l|r>.glb
  shoes_<id>_<l|r>.glb
  catalog.txt   (joint pivots + the style names the creator lists)

Materials are white (the game multiplies by the colour you pick); darker vertex colours give trims,
soles, eyes and other two-tone detail that always stays a shade of the chosen colour.

Character space: y up, +z = the way the hero faces, +x = the hero's left. 1 unit = 1 metre. Height ~1.78.
Run:  python3 tools/build_character.py
"""
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT = os.path.join(ROOT, 'assets', 'models', 'character')

# ---------------------------------------------------------------- skeleton (shared with the game via catalog.txt)
JOINTS = {
    'torso': (0.0, 0.92, 0.0),
    'head': (0.0, 1.50, 0.0),
    'arm_l': (0.235, 1.44, 0.0), 'arm_r': (-0.235, 1.44, 0.0),
    'leg_l': (0.10, 0.92, 0.0), 'leg_r': (-0.10, 0.92, 0.0),
}

HAIR = [('short', 'Short'), ('buzz', 'Buzz Cut'), ('spiky', 'Spiky'), ('curly', 'Curly'),
        ('long', 'Long'), ('ponytail', 'Ponytail'), ('bob', 'Bob'), ('braid', 'Braid'),
        ('slick', 'Slicked Back'), ('sidepart', 'Side Part'), ('mohawk', 'Mohawk'), ('afro', 'Afro'),
        ('manbun', 'Man Bun'), ('undercut', 'Undercut'), ('shaggy', 'Shaggy')]
# who can pick it: m = men only, f = women only, u = both
HAIR_FOR = {'short': 'u', 'buzz': 'u', 'spiky': 'u', 'curly': 'u', 'long': 'f', 'ponytail': 'f', 'bob': 'f', 'braid': 'f',
            'slick': 'm', 'sidepart': 'm', 'mohawk': 'm', 'afro': 'm', 'manbun': 'm', 'undercut': 'm', 'shaggy': 'm'}
SHIRTS = [('tee', 'T-Shirt'), ('long', 'Long Sleeve'), ('tank', 'Tank Top'), ('hoodie', 'Hoodie'), ('tunic', 'Tunic')]
PANTS = [('trousers', 'Trousers'), ('shorts', 'Shorts'), ('cargo', 'Cargo Pants'), ('breeches', 'Breeches'), ('skirt', 'Skirt')]
SHOES = [('none', 'Barefoot'), ('boots', 'Tall Boots'), ('ankle', 'Ankle Boots'), ('sneakers', 'Sneakers'), ('sandals', 'Sandals')]


def P(x, y, z):
    """character space -> Blender space (Blender -Y == forward, exported glTF Y-up gives +z forward)."""
    return Vector((x, -z, y))


def lerp(a, b, t):
    return a + (b - a) * t


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- procedural grey textures (tinted at runtime)
def make_textures():
    from PIL import Image
    sys.path.insert(0, os.path.dirname(__file__))
    import textures_proc as tp
    d = os.path.join(ROOT, 'build', 'tex_char')
    os.makedirs(d, exist_ok=True)
    n = 128
    rng = np.random.default_rng(7)
    yy, xx = np.mgrid[0:n, 0:n]
    # fabric: woven grid + soft noise, bright so tint colours stay true
    weave = 0.5 + 0.25 * np.sin(xx * 2 * np.pi / 4) * np.sin(yy * 2 * np.pi / 4) + 0.25 * np.sin((xx + yy) * 2 * np.pi / 8)
    fab = 0.86 + 0.10 * (weave - 0.5) + 0.06 * (tp.fbm(n, 6, 3, rng) - 0.5)
    # hair: long vertical streaks
    streak = tp.value_noise(n, 3, rng)
    hair = 0.72 + 0.28 * (0.5 + 0.5 * np.sin(xx / n * 2 * np.pi * 14 + streak * 6.0)) * (0.6 + 0.4 * tp.fbm(n, 4, 3, rng))
    # leather: pebbled
    f1, f2, v = tp.voronoi(n, 12, rng, 1.0)
    leather = 0.80 + 0.12 * np.clip((f2 - f1) * 5, 0, 1) + 0.08 * tp.fbm(n, 16, 3, rng)
    out = {}
    for name, a in (('fabric', fab), ('hair', hair), ('leather', leather)):
        a = np.clip(a, 0, 1)
        path = os.path.join(d, name + '.png')
        Image.fromarray((np.stack([a, a, a], -1) * 255 + 0.5).astype(np.uint8)).save(path)
        out[name] = path
    return out


# ---------------------------------------------------------------- mesh builder
class Part:
    def __init__(s, name, mat, pivot, uvscale=0.2):
        s.name, s.mat, s.pivot, s.uvscale = name, mat, Vector(pivot), uvscale
        s.bm = bmesh.new()
        s.col = s.bm.loops.layers.color.new('Col')

    # -- faces
    def _face(s, pairs, smooth, ref=None):
        """pairs = [(bmvert, k)], ref = point the face normal should point away from."""
        try:
            f = s.bm.faces.new([p[0] for p in pairs])
        except ValueError:
            return None
        f.smooth = smooth
        for l, (v, k) in zip(f.loops, pairs):
            l[s.col] = (k, k, k, 1.0)
        if ref is not None:
            f.normal_update()
            c = sum((v.co for v in f.verts), Vector()) / len(f.verts)
            if f.normal.dot(c - ref) < 0:
                f.normal_flip()
        return f

    def box(s, x0, y0, z0, x1, y1, z1, k=1.0):
        xs, ys, zs = sorted((x0, x1)), sorted((y0, y1)), sorted((z0, z1))
        c = P((xs[0] + xs[1]) / 2, (ys[0] + ys[1]) / 2, (zs[0] + zs[1]) / 2)
        vs = {}
        for i in (0, 1):
            for j in (0, 1):
                for m in (0, 1):
                    vs[(i, j, m)] = (s.bm.verts.new(P(xs[i], ys[j], zs[m])), k)
        for quad in (((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)), ((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)),
                     ((0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0)), ((1, 0, 0), (1, 0, 1), (1, 1, 1), (1, 1, 0)),
                     ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)), ((0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1))):
            s._face([vs[q] for q in quad], False, c)

    def grid(s, rows, closed_cols=True, smooth=True, ref=None):
        """rows = list of rings; each ring = list of (Vector world, k). Skins consecutive rings with quads."""
        vr = [[(s.bm.verts.new(p), k) for p, k in ring] for ring in rows]
        n = len(vr[0])
        fl = []
        for i in range(len(vr) - 1):
            for j in range(n if closed_cols else n - 1):
                a, b = vr[i][j], vr[i][(j + 1) % n]
                c, d = vr[i + 1][(j + 1) % n], vr[i + 1][j]
                f = s._face([a, b, c, d], smooth, None)
                if f:
                    fl.append(f)
        return vr, fl

    def _orient(s, faces, centre_of):
        """flip a whole connected skin so its normals point away from centre_of(face_centre)."""
        if not faces:
            return
        votes = 0
        for f in faces:
            f.normal_update()
            c = sum((v.co for v in f.verts), Vector()) / len(f.verts)
            votes += 1 if f.normal.dot(c - centre_of(c)) > 0 else -1
        if votes < 0:
            for f in faces:
                f.normal_flip()

    def loft(s, rings, n=12, axis='y', caps=(True, True), smooth=True, k=1.0):
        """rings: (t, c1, c2, r1, r2[, k]); axis y: x=c1+r1 sin, z=c2+r2 cos along y=t; axis z: x, y=c2 .. along z=t."""
        rows, centres = [], []
        for rg in rings:
            t, c1, c2, r1, r2 = rg[:5]
            kk = rg[5] if len(rg) > 5 else k
            ring = []
            for j in range(n):
                a = 2 * math.pi * j / n
                u, w = c1 + r1 * math.sin(a), c2 + r2 * math.cos(a)
                ring.append((P(u, t, w) if axis == 'y' else P(u, w, t), kk))
            rows.append(ring)
            centres.append(P(c1, t, c2) if axis == 'y' else P(c1, c2, t))
        vr, fl = s.grid(rows, True, smooth)
        # orient by the radial direction of the nearest ring centre
        def away(c):
            best = min(centres, key=lambda q: (q - c).length)
            return best
        s._orient(fl, away)
        for end, ring, other in ((0, vr[0], vr[1] if len(vr) > 1 else vr[0]), (1, vr[-1], vr[-2] if len(vr) > 1 else vr[-1])):
            if caps[end]:
                f = s._face(ring, False, None)
                if f:
                    f.normal_update()
                    cc = centres[0] if end == 0 else centres[-1]
                    oc = centres[1] if end == 0 else centres[-2]
                    if f.normal.dot(cc - oc) < 0:
                        f.normal_flip()
        return vr

    def ellipsoid(s, cx, cy, cz, rx, ry, rz, rows=7, cols=10, k=1.0, smooth=True):
        rings = []
        for i in range(rows + 1):
            th = math.pi * i / rows
            r = max(math.sin(th), 0.02)
            rings.append((cy + ry * math.cos(th), cx, cz, rx * r, rz * r, k))
        s.loft(rings, cols, 'y', (False, False), smooth)

    def tube(s, pts, radii, n=8, k=1.0, caps=(True, True), kfn=None, smooth=True):
        """Tube along a polyline of char-space points with a radius per point (spikes, ponytails, braids, ...)."""
        pts = [Vector(p) for p in pts]
        rows = []
        up = Vector((0, 1, 0))
        for i, p in enumerate(pts):
            d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
            u = d.cross(up)
            if u.length < 1e-3:
                u = d.cross(Vector((0, 0, 1)))
            u.normalize()
            v = d.cross(u).normalized()
            kk = kfn(i / max(1, len(pts) - 1)) if kfn else k
            ring = []
            for j in range(n):
                a = 2 * math.pi * j / n
                q = p + (u * math.cos(a) + v * math.sin(a)) * radii[i]
                ring.append((P(q.x, q.y, q.z), kk))
            rows.append(ring)
        vr, fl = s.grid(rows, True, smooth)
        cents = [P(*p) for p in pts]
        s._orient(fl, lambda c: min(cents, key=lambda q: (q - c).length))
        for end in (0, 1):
            if caps[end] and radii[0 if end == 0 else -1] > 0.001:
                f = s._face(vr[0] if end == 0 else vr[-1], False, None)
                if f:
                    f.normal_update()
                    cc = cents[0] if end == 0 else cents[-1]
                    oc = cents[1] if end == 0 else cents[-2]
                    if f.normal.dot(cc - oc) < 0:
                        f.normal_flip()

    def cap(s, cx, cy, cz, rx, ry, rz, limit, rows=7, cols=16, kfn=None):
        """Open ellipsoid shell from the top pole down to a per-direction latitude limit(phi) (hair caps)."""
        rowsv = []
        for i in range(rows + 1):
            ring = []
            for j in range(cols):
                phi = 2 * math.pi * j / cols
                th = max(limit(phi), 0.1) * i / rows
                th = max(th, 0.03)
                p = (cx + rx * math.sin(th) * math.sin(phi), cy + ry * math.cos(th), cz + rz * math.sin(th) * math.cos(phi))
                ring.append((P(*p), kfn(i / rows, phi) if kfn else 1.0))
            rowsv.append(ring)
        vr, fl = s.grid(rowsv, True, True)
        c0 = P(cx, cy, cz)
        s._orient(fl, lambda c: c0)

    # -- finish
    def project_uv(s):
        uvl = s.bm.loops.layers.uv.verify()
        s.bm.normal_update()
        sc = s.uvscale
        for f in s.bm.faces:
            n = f.normal
            if abs(n.z) > 0.8:
                for l in f.loops:
                    l[uvl].uv = (l.vert.co.x / sc, -l.vert.co.y / sc)
                continue
            L = max(math.hypot(n.x, n.y), 1e-4)
            tx, ty = -n.y / L, n.x / L
            vx, vy, vz = -n.z * n.x / L, -n.z * n.y / L, L
            for l in f.loops:
                p = l.vert.co
                l[uvl].uv = ((p.x * tx + p.y * ty) / sc, (p.x * vx + p.y * vy + p.z * vz) / sc)

    def to_object(s):
        s.project_uv()
        bmesh.ops.translate(s.bm, verts=s.bm.verts, vec=-P(*s.pivot))
        me = bpy.data.meshes.new(s.name)
        s.bm.to_mesh(me)
        s.bm.free()
        ob = bpy.data.objects.new(s.name, me)
        bpy.context.scene.collection.objects.link(ob)
        me.materials.append(MATS[s.mat])
        return ob


MATS = {}


def make_materials(tex):
    for name, tn in (('skin', None), ('cloth', 'fabric'), ('hair', 'hair'), ('leather', 'leather')):
        m = bpy.data.materials.new(name)
        m.use_nodes = True
        b = m.node_tree.nodes.get('Principled BSDF')
        b.inputs['Base Color'].default_value = (1, 1, 1, 1)
        b.inputs['Roughness'].default_value = 1.0
        if 'Specular IOR Level' in b.inputs:
            b.inputs['Specular IOR Level'].default_value = 0.0
        if tn:
            img = bpy.data.images.load(tex[tn])
            img.colorspace_settings.name = 'sRGB'
            t = m.node_tree.nodes.new('ShaderNodeTexImage')
            t.image = img
            m.node_tree.links.new(t.outputs['Color'], b.inputs['Base Color'])
        MATS[name] = m


def export(ob):
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, ob.name + '.glb'), export_format='GLB', use_selection=True,
                              export_yup=True, export_apply=True, export_normals=True, export_vertex_color='ACTIVE',
                              export_cameras=False, export_lights=False, export_materials='EXPORT',
                              export_image_format='JPEG', export_jpeg_quality=88)
    bpy.data.objects.remove(ob, do_unlink=True)


# ================================================================================================
# BODY SHAPES
# ================================================================================================
# torso rings: (y, rx, rz, cz)
TORSO = {
    'm': [(0.86, .180, .108, 0), (0.92, .188, .114, 0), (1.00, .172, .108, 0), (1.08, .158, .102, 0), (1.20, .172, .108, 0),
          (1.32, .192, .118, .004), (1.40, .205, .112, 0), (1.455, .170, .095, 0), (1.49, .085, .075, 0), (1.52, .055, .055, 0)],
    'f': [(0.86, .185, .112, 0), (0.92, .200, .120, 0), (1.00, .168, .108, 0), (1.08, .130, .092, 0), (1.20, .140, .098, .002),
          (1.28, .152, .118, .012), (1.34, .160, .116, .010), (1.40, .182, .104, 0), (1.455, .150, .088, 0),
          (1.49, .080, .070, 0), (1.52, .052, .052, 0)],
}


def torso_at(g, y):
    r = TORSO[g]
    if y <= r[0][0]:
        return r[0][1:]
    for a, b in zip(r, r[1:]):
        if a[0] <= y <= b[0]:
            t = (y - a[0]) / (b[0] - a[0])
            return tuple(lerp(a[i], b[i], t) for i in (1, 2, 3))
    return r[-1][1:]


def torso_rings(g, y0, y1, grow, extra=()):
    """Rings following the body torso between y0 and y1 (grown by `grow`), plus any extra (y,rx,rz,cz,k)."""
    ys = sorted(set([y0, y1] + [r[0] for r in TORSO[g] if y0 < r[0] < y1] + [e[0] for e in extra]))
    out = []
    ex = {e[0]: e for e in extra}
    for y in ys:
        if y in ex:
            e = ex[y]
            out.append((y, 0, e[3], e[1], e[2], e[4] if len(e) > 4 else 1.0))
        else:
            rx, rz, cz = torso_at(g, y)
            out.append((y, 0, cz, rx + grow, rz + grow * 0.9))
    return out


LEG = [(0.97, .086), (0.92, .094), (0.80, .092), (0.60, .076), (0.50, .068), (0.34, .066), (0.14, .042), (0.08, .040)]


def leg_r(y):
    if y >= LEG[0][0]:
        return LEG[0][1]
    for a, b in zip(LEG, LEG[1:]):
        if b[0] <= y <= a[0]:
            return lerp(b[1], a[1], (y - b[0]) / (a[0] - b[0]))
    return LEG[-1][1]


ARM = [(1.485, .02), (1.47, .05), (1.44, .058), (1.34, .050), (1.16, .042), (0.98, .033), (0.935, .030)]


def arm_r(y):
    if y >= ARM[0][0]:
        return ARM[0][1]
    for a, b in zip(ARM, ARM[1:]):
        if b[0] <= y <= a[0]:
            return lerp(b[1], a[1], (y - b[0]) / (a[0] - b[0]))
    return ARM[-1][1]


def arm_cz(y):
    return 0.012 * smoothstep((1.44 - y) / 0.3) * (1 - smoothstep((0.98 - y) / 0.2) * 0.0)


def foot_rings(grow=0.0, hgrow=0.0, toe=0.0):
    """Foot cross-sections along z: (z, cx, cy, half-width, half-height)."""
    base = [(-0.085, .028, .030), (-0.055, .040, .042), (0.0, .043, .040), (0.10, .046, .031), (0.17, .040, .022), (0.205, .020, .012)]
    out = []
    for z, w, h in base:
        zz = z + (toe if z > 0.15 else 0.0) - (grow if z < 0 else 0.0)
        out.append((zz, 0.0, h + 0.0 + hgrow * 0.3, w + grow, h + hgrow))
    return out


# ---------------------------------------------------------------- body parts
def build_head(g):
    p = Part('body_%s_head' % g, 'skin', JOINTS['head'])
    f = 0.95 if g == 'f' else 1.0
    rings = [(1.495, 0, 0, .050, .050), (1.56, 0, 0.005, .052, .054), (1.585, 0, 0.012, .072 * f, .082 * f),
             (1.60, 0, 0.010, .098 * f, .104 * f), (1.64, 0, 0.004, .113, .122), (1.68, 0, 0, .115, .125),
             (1.72, 0, -0.002, .108, .115), (1.76, 0, -0.004, .075, .082), (1.785, 0, -0.004, .026, .03)]
    p.loft(rings, 14, 'y', (False, True))
    # face: nose, eyes, brows, mouth, ears
    p.ellipsoid(0, 1.638, 0.126, .016, .024, .022, 12)
    nose_k = 0.97
    for sx in (-1, 1):
        p.ellipsoid(sx * 0.046, 1.668, 0.112, .014, .019, .012, rows=5, cols=8, k=0.07)           # eyes
        p.box(sx * 0.046 - 0.026, 1.703, 0.113, sx * 0.046 + 0.026, 1.711, 0.121, 0.28)         # brows
        p.ellipsoid(sx * 0.115, 1.648, -0.004, .016, .030, .024, rows=5, cols=8, k=0.97)          # ears
    p.box(-0.030, 1.598, 0.108, 0.030, 1.606, 0.118, 0.52 if g == 'm' else 0.46)                # mouth
    if g == 'f':
        p.box(-0.028, 1.606, 0.109, 0.028, 1.612, 0.117, 0.42)
        for sx in (-1, 1):   # lashes
            p.box(sx * 0.046 - 0.020, 1.684, 0.112, sx * 0.046 + 0.020, 1.688, 0.121, 0.18)
    return p


def build_torso(g):
    p = Part('body_%s_torso' % g, 'skin', JOINTS['torso'])
    rings = [(y, 0, cz, rx, rz) for (y, rx, rz, cz) in TORSO[g]]
    p.loft(rings, 14, 'y', (True, False))
    return p


def build_arm(side):
    sx = 1 if side == 'l' else -1
    p = Part('body_arm_%s' % side, 'skin', JOINTS['arm_' + side])
    cx = sx * 0.235
    rings = [(y, cx, arm_cz(y), r, r) for (y, r) in ARM]
    p.loft(rings, 10, 'y', (False, False))
    p.ellipsoid(cx, 0.865, 0.016, .034, .072, .024, 7, 10)                 # hand
    p.ellipsoid(cx - sx * 0.030, 0.885, 0.030, .012, .032, .013, 5, 8)      # thumb
    return p


def build_leg(side):
    sx = 1 if side == 'l' else -1
    p = Part('body_leg_%s' % side, 'skin', JOINTS['leg_' + side])
    cx = sx * 0.10
    p.loft([(y, cx, 0.0, r, r) for (y, r) in LEG], 12, 'y', (False, False))
    p.loft([(z, cx, cy, w, h) for (z, _, cy, w, h) in foot_rings()], 10, 'z', (True, True))
    return p


# ================================================================================================
# HAIR
# ================================================================================================
HC = (0.0, 1.655, -0.004)   # head centre used for hair shells


def wrap(a):
    return (a + math.pi) % (2 * math.pi) - math.pi


def limit_fn(front, side, back):
    def f(phi):
        w = wrap(phi)
        # phi = 0 is the front. Blend side->back by |phi|
        t = smoothstep((abs(w) - 0.6) / (math.pi - 0.6))
        base = lerp(side, back, t)
        base = lerp(front, base, smoothstep(abs(w) / 1.1))
        return base
    return f


def hair_k(root=0.78):
    return lambda t, phi: lerp(root, 1.0, smoothstep(t))


def build_hair(hid):
    p = Part('hair_' + hid, 'hair', JOINTS['head'], uvscale=0.12)
    cx, cy, cz = HC
    if hid == 'short':
        p.cap(cx, cy, cz, .129, .152, .139, limit_fn(0.80, 1.50, 1.72), 8, 18, hair_k())
        for fx, fy, fz, fr in ((-0.055, 1.768, 0.105, .042), (0.0, 1.785, 0.095, .046), (0.060, 1.770, 0.100, .040), (0.095, 1.745, 0.075, .034)):
            p.ellipsoid(fx, fy, fz, fr, fr * 0.8, fr, 5, 8, 0.95)      # tousled fringe
    elif hid == 'buzz':
        p.cap(cx, cy, cz, .127, .150, .137, limit_fn(0.95, 1.40, 1.60), 6, 18, lambda t, phi: lerp(0.65, 0.9, t))
    elif hid == 'spiky':
        p.cap(cx, cy, cz, .125, .148, .135, limit_fn(0.85, 1.45, 1.65), 6, 16, hair_k())
        import random
        r = random.Random(5)
        for i in range(11):
            a = 2 * math.pi * i / 11 + r.uniform(-.2, .2)
            th = r.uniform(0.35, 1.05)
            base = Vector((cx + .13 * math.sin(th) * math.sin(a), cy + .15 * math.cos(th), cz + .14 * math.sin(th) * math.cos(a)))
            d = Vector((math.sin(th) * math.sin(a), math.cos(th) * 1.4 + 0.3, math.sin(th) * math.cos(a))).normalized()
            p.tube([base - d * 0.02, base + d * 0.09, base + d * 0.18], [0.034, 0.028, 0.004], 6, kfn=lambda t: lerp(0.8, 1.0, t))
    elif hid == 'long':
        p.cap(cx, cy, cz, .130, .153, .141, limit_fn(0.80, 1.55, 1.85), 8, 18, hair_k())
        p.loft([(1.66, 0, -0.085, .125, .062, 0.8), (1.55, 0, -0.095, .130, .066, 0.85), (1.40, 0, -0.100, .128, .066, 0.92),
                (1.24, 0, -0.098, .115, .060, 0.98), (1.14, 0, -0.092, .090, .050, 1.0)], 12, 'y', (False, True))
        for sx in (-1, 1):   # side locks over the shoulders
            p.tube([(sx * 0.125, 1.64, 0.02), (sx * 0.135, 1.50, 0.03), (sx * 0.14, 1.36, 0.03), (sx * 0.135, 1.26, 0.035)],
                   [0.030, 0.032, 0.030, 0.022], 8, kfn=lambda t: lerp(0.85, 1.0, t))
    elif hid == 'ponytail':
        p.cap(cx, cy, cz, .129, .152, .139, limit_fn(0.82, 1.50, 1.72), 8, 18, hair_k())
        p.tube([(0, 1.74, -0.12), (0, 1.69, -0.20), (0, 1.58, -0.235), (0, 1.43, -0.22), (0, 1.32, -0.20)],
               [0.045, 0.042, 0.040, 0.032, 0.008], 8, kfn=lambda t: lerp(0.85, 1.0, t))
        p.tube([(0, 1.745, -0.115), (0, 1.735, -0.135)], [0.052, 0.052], 8, k=0.5)     # tie
    elif hid == 'bob':
        p.cap(cx, cy, cz, .140, .160, .150, limit_fn(0.80, 2.20, 2.38), 11, 20, hair_k(0.72))
    elif hid == 'braid':
        p.cap(cx, cy, cz, .129, .152, .139, limit_fn(0.82, 1.50, 1.72), 8, 18, hair_k())
        for i in range(7):
            y = 1.66 - i * 0.065
            ox = 0.022 * (1 if i % 2 == 0 else -1)
            p.ellipsoid(ox, y, -0.14 - 0.012 * i, .040, .045, .036, 5, 8, 0.82 + 0.03 * (i % 2))
        p.tube([(0, 1.20, -0.20), (0, 1.17, -0.205)], [0.050, 0.050], 8, k=0.5)
        p.ellipsoid(0, 1.12, -0.205, .028, .05, .024, 5, 8, 0.95)
    elif hid == 'curly':
        import random
        r = random.Random(11)
        p.cap(cx, cy, cz, .135, .155, .145, limit_fn(0.85, 1.55, 1.85), 7, 18, hair_k(0.7))
        for i in range(16):
            a = 2 * math.pi * i / 16 + r.uniform(-.15, .15)
            th = r.uniform(0.15, 1.35)
            rad = r.uniform(0.045, 0.065)
            c = Vector((cx + .15 * math.sin(th) * math.sin(a) * 1.05, cy + .17 * math.cos(th) + 0.02, cz + .155 * math.sin(th) * math.cos(a) * 1.0 - 0.01))
            if abs(wrap(a)) < 0.5 and th > 0.9:   # keep the face clear
                continue
            p.ellipsoid(c.x, c.y, c.z, rad, rad * 0.95, rad, 5, 8, r.uniform(0.82, 1.0))
    elif hid == 'slick':       # slicked back, high forehead
        p.cap(cx, cy, cz, .130, .153, .141, limit_fn(0.62, 1.50, 1.78), 8, 20, hair_k(0.8))
        for x in (-0.085, -0.045, 0.0, 0.045, 0.085):
            p.tube([(x, 1.775, 0.075), (x * 1.05, 1.815, 0.0), (x * 1.1, 1.790, -0.085), (x * 1.0, 1.700, -0.150), (x * 0.7, 1.600, -0.150)],
                   [0.016, 0.026, 0.026, 0.024, 0.010], 6, kfn=lambda t: lerp(0.82, 1.0, t))
        p.ellipsoid(0, 1.62, -0.115, .105, .075, .055, 5, 12, 0.88)       # nape
    elif hid == 'sidepart':    # neat side part with a swept fringe
        p.cap(cx, cy, cz, .138, .162, .149, limit_fn(0.74, 1.50, 1.74), 8, 20, hair_k(0.8))
        p.ellipsoid(-0.045, 1.775, 0.060, .075, .045, .085, 5, 10, 0.95)  # swept side volume
        p.tube([(0.050, 1.815, 0.030), (0.000, 1.800, 0.110), (-0.070, 1.760, 0.140), (-0.115, 1.715, 0.120)],
               [0.020, 0.040, 0.036, 0.012], 8, kfn=lambda t: lerp(0.85, 1.0, t))
        p.box(0.045, 1.795, 0.015, 0.050, 1.815, 0.125, 0.45)                 # the parting line
    elif hid == 'mohawk':      # shaved sides, tall ridge
        p.cap(cx, cy, cz, .124, .148, .134, limit_fn(1.05, 1.40, 1.60), 6, 18, lambda t, phi: lerp(0.55, 0.75, t))
        for i, (y, z, h) in enumerate(((1.775, 0.105, .055), (1.810, 0.060, .075), (1.830, 0.010, .085), (1.825, -0.045, .085),
                                       (1.800, -0.095, .075), (1.750, -0.135, .060), (1.690, -0.150, .045))):
            p.ellipsoid(0, y + h * 0.35, z, .024, h, .038, 5, 8, 0.8 + 0.03 * i)
    elif hid == 'afro':
        import random
        r = random.Random(3)
        p.cap(cx, cy + 0.035, cz - 0.01, .205, .205, .205, limit_fn(0.82, 1.95, 2.40), 9, 22, hair_k(0.72))
        for i in range(18):
            a = 2 * math.pi * i / 18 + r.uniform(-.1, .1)
            th = r.uniform(0.2, 1.7)
            if abs(wrap(a)) < 0.6 and th > 0.8:
                continue
            rad = r.uniform(0.035, 0.05)
            p.ellipsoid(cx + .205 * math.sin(th) * math.sin(a), cy + 0.035 + .205 * math.cos(th), cz - 0.01 + .205 * math.sin(th) * math.cos(a),
                        rad, rad, rad, 4, 8, r.uniform(0.85, 1.0))
    elif hid == 'manbun':
        p.cap(cx, cy, cz, .129, .152, .139, limit_fn(0.76, 1.50, 1.72), 8, 18, hair_k())
        p.ellipsoid(0, 1.830, -0.105, .062, .062, .062, 6, 10, 0.95)
        p.tube([(0, 1.775, -0.075), (0, 1.805, -0.095)], [0.040, 0.040], 8, k=0.5)      # tie
        p.loft([(1.82, 0, -0.098, .045, .028, 0.6), (1.84, 0, -0.100, .040, .026, 0.6)], 10, 'y', (False, False))
    elif hid == 'undercut':    # tight sides, long swept top
        p.cap(cx, cy, cz, .125, .149, .135, limit_fn(1.00, 1.35, 1.55), 6, 18, lambda t, phi: lerp(0.6, 0.8, t))
        p.ellipsoid(0, 1.800, 0.020, .092, .040, .120, 5, 12, 0.95)
        for fx, fy, fz, fr in ((-0.060, 1.795, 0.110, .046), (0.005, 1.815, 0.115, .050), (0.070, 1.800, 0.100, .044), (0.0, 1.835, 0.050, .048)):
            p.ellipsoid(fx, fy, fz, fr, fr * 1.0, fr * 1.2, 5, 8, 0.97)          # swept-up fringe / quiff
    elif hid == 'shaggy':      # messy, ear-length
        import random
        r = random.Random(21)
        p.cap(cx, cy, cz, .136, .158, .146, limit_fn(0.80, 1.85, 2.05), 8, 20, hair_k(0.72))
        for i in range(10):
            a = 2 * math.pi * i / 10 + r.uniform(-.2, .2)
            if abs(wrap(a)) < 0.55:
                continue
            th = r.uniform(1.1, 1.75)
            rad = r.uniform(0.035, 0.05)
            p.ellipsoid(cx + .15 * math.sin(th) * math.sin(a), cy + .16 * math.cos(th), cz + .15 * math.sin(th) * math.cos(a), rad, rad * 1.3, rad, 4, 8, r.uniform(0.85, 1.0))
        for fx, fy, fz, fr in ((-0.075, 1.775, 0.105, .040), (-0.020, 1.790, 0.112, .040), (0.040, 1.785, 0.108, .040), (0.095, 1.760, 0.090, .036)):
            p.ellipsoid(fx, fy, fz, fr, fr * 1.1, fr, 5, 8, 0.95)
    return p


# ================================================================================================
# SHIRTS
# ================================================================================================
def build_shirt_torso(sid, g):
    p = Part('shirt_%s_torso_%s' % (sid, g), 'cloth', JOINTS['torso'])
    if sid == 'tee':
        rings = [(0.895, 0, 0, .222, .150, 0.8), (0.915, 0, 0, .212, .146, 0.9)] + torso_rings(g, 0.93, 1.485, 0.016)[1:]
        p.loft(rings, 14, 'y', (False, False))
        p.loft([(1.478, 0, 0, .092, .082, 0.62), (1.505, 0, 0, .082, .074, 0.62)], 14, 'y', (False, False))        # rib collar
    elif sid == 'long':
        rings = [(0.895, 0, 0, .222, .150, 0.8), (0.915, 0, 0, .212, .146, 0.9)] + torso_rings(g, 0.93, 1.485, 0.016)[1:]
        p.loft(rings, 14, 'y', (False, False))
        p.loft([(1.478, 0, 0, .092, .082, 0.62), (1.505, 0, 0, .082, .074, 0.62)], 14, 'y', (False, False))
        p.loft([(0.93, 0, 0, 0.0, 0.0, 1.0)] if False else [(0.935, 0, 0, .172, .125, 0.62), (0.985, 0, 0, .171, .124, 0.62)], 14, 'y', (False, False))  # hem band
    elif sid == 'tank':
        extra = [(1.36, .175, .110, 0, 1.0), (1.42, .125, .102, 0, 1.0), (1.47, .082, .092, 0, 0.8)]
        rings = [(0.895, 0, 0, .222, .150, 0.8), (0.915, 0, 0, .212, .146, 0.9)] + [r for r in torso_rings(g, 0.93, 1.36, 0.016)][1:] + [(y, 0, 0, rx, rz, kk) for (y, rx, rz, _, kk) in extra[1:]]
        p.loft(rings, 14, 'y', (False, False))
        p.loft([(0.93, 0, 0, 0.0, 0.0)] if False else [(0.935, 0, 0, .172, .125, 0.62), (0.985, 0, 0, .171, .124, 0.62)], 14, 'y', (False, False))
    elif sid == 'hoodie':
        rings = torso_rings(g, 0.90, 1.47, 0.030)
        p.loft(rings, 16, 'y', (False, False))
        p.loft([(0.885, 0, 0, .228, .158, 0.6), (0.95, 0, 0, .214, .150, 0.6)], 16, 'y', (False, False))     # waist rib
        p.loft([(1.43, 0, 0.0, .105, .098, 0.9), (1.50, 0, 0.0, .098, .090, 0.9), (1.545, 0, -0.01, .092, .095, 0.8)], 14, 'y', (False, False))   # neck cowl
        p.ellipsoid(0, 1.47, -0.095, .115, .075, .070, 6, 12, 0.85)                       # hood bunched at the back
        p.box(-0.085, 1.00, 0.115, 0.085, 1.10, 0.150 if g == 'm' else 0.150, 0.82)       # kangaroo pocket
        p.box(-0.035, 1.20, 0.118, -0.028, 1.40, 0.130, 0.55)                              # drawstrings
        p.box(0.028, 1.20, 0.118, 0.035, 1.40, 0.130, 0.55)
    elif sid == 'tunic':
        extra = [(0.80, .222, .165, 0, 0.9), (0.86, .21, .15, 0, 0.9)]
        base = [r for r in torso_rings(g, 0.93, 1.47, 0.024) if r[0] > 0.93]
        rings = [(0.77, 0, 0, .250, .190, 0.7), (0.82, 0, 0, .240, .180, 0.8), (0.88, 0, 0, .215, .158), (0.93, 0, 0.0, .192, .140)] + base
        p.loft(rings, 16, 'y', (False, False))
        p.loft([(1.04, 0, 0, .194, .136, 0.55), (1.10, 0, 0, .190, .132, 0.55)], 16, 'y', (False, False))        # belt
        p.box(-0.028, 1.045, 0.120, 0.028, 1.095, 0.138, 0.9)                                                  # buckle
        p.loft([(1.465, 0, 0, .098, .088, 0.62), (1.50, 0, 0, .085, .076, 0.62)], 14, 'y', (False, False))
        p.box(-0.020, 1.30, 0.108, 0.020, 1.46, 0.118, 0.6)                                                    # laced placket
    return p


def sleeve_end(sid):
    return {'tee': 1.30, 'long': 0.985, 'hoodie': 0.985, 'tunic': 0.985}.get(sid)


def build_sleeve(sid, side):
    y_end = sleeve_end(sid)
    if y_end is None:
        return None
    sx = 1 if side == 'l' else -1
    p = Part('shirt_%s_sleeve_%s' % (sid, side), 'cloth', JOINTS['arm_' + side])
    cx = sx * 0.235
    grow = 0.034 if sid == 'hoodie' else 0.014
    ys = [1.50, 1.49, 1.47, 1.45] + [y for y in (1.34, 1.16, 1.06) if y > y_end] + [y_end]
    rings = []
    for y in ys:
        r = {1.50: 0.022, 1.49: 0.048, 1.47: 0.062}.get(y, None)
        r = (r + grow * 0.5) if r is not None else arm_r(y) + grow
        flare = 0.006 if y == y_end and sid == 'tee' else 0.0
        rings.append((y, cx, arm_cz(y), r + flare, r + flare))
    p.loft(rings, 12, 'y', (True, False))
    if False:   # cuff band (removed: twisted at the wrist)
        r = arm_r(y_end) + grow * 0.6
        p.loft([(y_end + 0.005, cx, arm_cz(y_end), r + 0.006, r + 0.006, 0.62), (y_end + 0.055, cx, arm_cz(y_end), r + 0.005, r + 0.005, 0.62)], 12, 'y', (False, False))
    return p


# ================================================================================================
# PANTS
# ================================================================================================
def leg_sleeve(p, side, y_top, y_end, grow, flare=0.0, k=1.0, band=None):
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    ys = sorted(set([y_top, y_end] + [a for a, _ in LEG if y_end < a < y_top] + [y for y in (0.95, 0.91) if y_end < y < y_top]), reverse=True)
    rings = []
    for y in ys:
        tuck = 1.0 if y <= 0.91 else 1.0 - 0.18 * (y - 0.91) / 0.08     # thigh tucks in under the shirt hem
        r = (leg_r(y) + grow) * tuck + (flare if y == y_end else 0.0)
        if y > 0.93:   # never wider than the shirt that hangs over this spot
            r = min(r, min(torso_at('m', y)[0], torso_at('f', y)[0]) + 0.008 - 0.10)
        rings.append((y, cx, 0.0, r, r, k))
    p.loft(rings, 12, 'y', (False, False))
    if band:   # darker cuff
        r = leg_r(y_end) + grow
        p.loft([(y_end + 0.003, cx, 0.0, r + 0.004 + flare, r + 0.004 + flare, 0.6), (y_end + band, cx, 0.0, r + 0.004, r + 0.004, 0.6)],
               12, 'y', (False, False))


PANTS_END = {'trousers': 0.05, 'shorts': 0.60, 'cargo': 0.05, 'breeches': 0.36}


def build_pants_hips(pid, g):
    p = Part('pants_%s_hips_%s' % (pid, g), 'cloth', JOINTS['torso'])
    ta = lambda y, d: (torso_at(g, y)[0] + d, torso_at(g, y)[1] + d * 0.9)
    if pid == 'skirt':
        rings = [(1.02, 0, 0, *ta(1.02, .006), 0.6), (0.97, 0, 0, *ta(0.97, .008), 0.6), (0.92, 0, 0, .190, .140),
                 (0.80, 0, 0, .225, .178), (0.66, 0, 0, .258, .208, 0.88), (0.62, 0, 0, .262, .212, 0.7)]
        p.loft(rings, 18, 'y', (False, False))
        return p
    rings = [(1.02, 0, 0, *ta(1.02, .006), 0.6), (0.97, 0, 0, *ta(0.97, .007), 0.6), (0.92, 0, 0, *ta(0.92, .008)),
             (0.87, 0, 0, .175, .114), (0.83, 0, 0, .140, .100), (0.79, 0, 0, .105, .082), (0.755, 0, 0, .05, .045)]
    p.loft(rings, 16, 'y', (False, True))
    if pid == 'cargo':   # belt loops
        for sx in (-1, 1):
            p.box(sx * 0.12 - 0.012, 0.95, 0.092, sx * 0.12 + 0.012, 1.03, 0.108, 0.5)
        p.box(-0.012, 0.95, 0.108, 0.012, 1.03, 0.124, 0.5)
    return p


def build_pants_leg(pid, side):
    if pid == 'skirt':
        return None
    p = Part('pants_%s_leg_%s' % (pid, side), 'cloth', JOINTS['leg_' + side])
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    end = PANTS_END[pid]
    if pid == 'trousers':
        leg_sleeve(p, side, 0.99, end, 0.013, flare=0.012, band=0.04)
    elif pid == 'shorts':
        leg_sleeve(p, side, 0.99, end, 0.015, flare=0.012, band=0.03)
    elif pid == 'cargo':
        leg_sleeve(p, side, 0.99, end, 0.018, flare=0.016, band=0.06)
        p.box(cx + sx * 0.085, 0.62, -0.05, cx + sx * 0.128, 0.80, 0.07, 0.88)       # cargo pocket
        p.box(cx + sx * 0.085, 0.76, -0.05, cx + sx * 0.132, 0.80, 0.07, 0.62)       # flap
        p.box(cx - 0.05, 0.46, 0.062, cx + 0.05, 0.55, 0.082, 0.82)                   # knee patch
    elif pid == 'breeches':
        leg_sleeve(p, side, 0.99, end, 0.020, flare=0.0, band=0.05)
    return p


# ================================================================================================
# SHOES
# ================================================================================================
def build_shoe(sid, side):
    if sid == 'none':
        return None
    sx = 1 if side == 'l' else -1
    cx = sx * 0.10
    p = Part('shoes_%s_%s' % (sid, side), 'leather', JOINTS['leg_' + side], uvscale=0.12)
    if sid == 'boots':
        top = 0.36
        p.loft([(top + 0.02, cx, -0.002, leg_r(top) + .034, leg_r(top) + .036, 0.6), (top, cx, -0.002, leg_r(top) + .033, leg_r(top) + .035), (0.26, cx, 0.0, leg_r(0.26) + .030, leg_r(0.26) + .033), (0.14, cx, 0.006, .064, .072), (0.085, cx, 0.012, .060, .074)],
               14, 'y', (False, False))
        p.loft([(z, cx, cy + 0.002, w + 0.012, h + 0.012) for (z, _, cy, w, h) in foot_rings()], 12, 'z', (True, True))
        p.box(cx - 0.062, 0.0, -0.092, cx + 0.062, 0.022, 0.215, 0.42)         # sole
        p.box(cx - 0.052, 0.0, -0.095, cx + 0.052, 0.040, -0.040, 0.45)        # heel
    elif sid == 'ankle':
        top = 0.17
        p.loft([(top + 0.012, cx, 0.0, leg_r(top) + .032, leg_r(top) + .034, 0.65), (top, cx, 0.0, leg_r(top) + .030, leg_r(top) + .032), (0.11, cx, 0.006, .062, .070), (0.085, cx, 0.012, .060, .074)], 14, 'y', (False, False))
        p.loft([(z, cx, cy + 0.002, w + 0.011, h + 0.011) for (z, _, cy, w, h) in foot_rings()], 12, 'z', (True, True))
        p.box(cx - 0.058, 0.0, -0.090, cx + 0.058, 0.020, 0.212, 0.42)
        p.box(cx - 0.050, 0.0, -0.093, cx + 0.050, 0.034, -0.045, 0.45)
    elif sid == 'sneakers':
        p.loft([(0.115, cx, 0.0, .056, .060, 0.7), (0.095, cx, 0.008, .054, .066), (0.075, cx, 0.012, .054, .070)], 14, 'y', (False, False))
        p.loft([(z, cx, cy + 0.003, w + 0.010, h + 0.014) for (z, _, cy, w, h) in foot_rings()], 12, 'z', (True, True))
        p.box(cx - 0.056, 0.0, -0.090, cx + 0.056, 0.022, 0.218, 0.5)          # sole
        p.box(cx - 0.030, 0.062, 0.035, cx + 0.030, 0.082, 0.115, 0.7)         # tongue
        for z in (0.05, 0.085):
            p.box(cx - 0.036, 0.083, z, cx + 0.036, 0.088, z + 0.008, 0.55)    # laces
    elif sid == 'sandals':
        p.box(cx - 0.056, 0.0, -0.090, cx + 0.056, 0.020, 0.215, 0.5)          # sole
        for z in (0.0, 0.08):
            p.box(cx - 0.050, 0.018, z, cx + 0.050, 0.050, z + 0.022, 0.8)     # straps across the foot
        p.loft([(0.078, cx, -0.052, .048, .008, 0.8), (0.040, cx, -0.058, .046, .008, 0.8)], 8, 'y', (False, False))
        p.box(cx - 0.040, 0.050, -0.070, cx + 0.040, 0.078, -0.044, 0.8)       # ankle strap
    return p


# ================================================================================================
def write_catalog():
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'catalog.txt'), 'w') as f:
        f.write('// Journey character catalog. Generated by tools/build_character.py\n')
        for k, v in JOINTS.items():
            f.write('joint %s %.4f %.4f %.4f\n' % ((k,) + v))
        f.write('height 1.78\n')
        for lst, key in ((HAIR, 'hair'), (SHIRTS, 'shirt'), (PANTS, 'pants'), (SHOES, 'shoes')):
            for i, n in lst:
                extra = ''
                if key == 'hair':
                    extra = ' for=%s' % HAIR_FOR[i]
                if key == 'shirt':
                    extra = ' sleeves=%d' % (1 if sleeve_end(i) else 0)
                if key == 'pants':
                    extra = ' legs=%d' % (0 if i == 'skirt' else 1)
                f.write('%s %s %s%s\n' % (key, i, n.replace(' ', '_'), extra))


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    make_materials(make_textures())
    count = 0

    def emit(part):
        nonlocal count
        if part is None:
            return
        export(part.to_object())
        count += 1

    for g in 'mf':
        emit(build_head(g))
        emit(build_torso(g))
    for sd in 'lr':
        emit(build_arm(sd))
        emit(build_leg(sd))
    for hid, _ in HAIR:
        emit(build_hair(hid))
    for sid, _ in SHIRTS:
        for g in 'mf':
            emit(build_shirt_torso(sid, g))
        for sd in 'lr':
            emit(build_sleeve(sid, sd))
    for pid, _ in PANTS:
        for g in 'mf':
            emit(build_pants_hips(pid, g))
        for sd in 'lr':
            emit(build_pants_leg(pid, sd))
    for sid, _ in SHOES:
        for sd in 'lr':
            emit(build_shoe(sid, sd))
    write_catalog()
    if '--blend' in sys.argv:
        pass
    print('character parts exported:', count)


if __name__ == '__main__':
    main()
