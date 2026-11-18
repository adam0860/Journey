"""
Small low-poly modelling helpers shared by build_items.py and build_animals.py (Blender / bpy).

Everything is a convex hull of a few points, so the faces are always closed and correctly oriented, and the look is the
same chunky faceted style as the town. Colours are Principled-BSDF base colours (no textures), so a model looks the same
in art styles A and B. Space: y up, +z = forward, +x = left (the same as the character); 1 unit = 1 metre.
"""
import bpy, bmesh, math, os
from mathutils import Euler, Vector

_MAT_CACHE = {}


def s2l(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hexlin(h):
    h = h.lstrip('#')
    return tuple(s2l(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (1.0,)


def P(x, y, z):
    """model space (y up, +z forward) -> Blender space (Z up, -Y forward); exported glTF is then y-up, +z forward."""
    return Vector((x, -z, y))


def material(hexcolor):
    """One flat-coloured material per colour (created once)."""
    if hexcolor not in _MAT_CACHE:
        m = bpy.data.materials.new('c' + hexcolor.lstrip('#'))
        m.use_nodes = True
        b = m.node_tree.nodes.get('Principled BSDF')
        b.inputs['Base Color'].default_value = hexlin(hexcolor)
        b.inputs['Roughness'].default_value = 1.0
        if 'Specular IOR Level' in b.inputs:
            b.inputs['Specular IOR Level'].default_value = 0.0
        _MAT_CACHE[hexcolor] = m
    return _MAT_CACHE[hexcolor]


class Model:
    """A mesh built from convex hulls. Points are given in model space; `origin` becomes the object's pivot (so the game
    can rotate a part about its joint), `scale` resizes everything, `zoom` resizes just this part about its origin."""

    def __init__(s, name, origin=(0.0, 0.0, 0.0), palette=None):
        s.name = name
        s.bm = bmesh.new()
        s.col = s.bm.loops.layers.color.new('Col')
        s.mats = []
        s.origin = Vector(origin)
        s.scale = 1.0
        s.zoom = 1.0
        s.xf = None          # optional function (x, y, z) -> (x, y, z) applied to every point (lets one design be exported in another pose)
        s.palette = palette or {}

    def _mi(s, mat):
        mat = s.palette.get(mat, mat)
        if mat not in s.mats:
            s.mats.append(mat)
        return s.mats.index(mat)

    def _pt(s, p):
        v = Vector(p)
        v = s.origin + (v - s.origin) * s.zoom
        return P(*((v - s.origin) * s.scale))      # relative to the pivot

    def hull(s, pts, mat, shade=1.0):
        """Convex solid through these points."""
        if s.xf:
            pts = [s.xf(tuple(p)) for p in pts]
        verts = [s.bm.verts.new(s._pt(p)) for p in pts]
        res = bmesh.ops.convex_hull(s.bm, input=verts)
        faces = [g for g in res['geom'] if isinstance(g, bmesh.types.BMFace)]
        mi = s._mi(mat)
        for f in faces:
            f.material_index = mi
            f.smooth = False
            for l in f.loops:
                l[s.col] = (shade, shade, shade, 1.0)
        loose = [v for v in verts if v.is_valid and not v.link_faces]   # points that ended up inside the hull
        if loose:
            bmesh.ops.delete(s.bm, geom=loose, context='VERTS')

    def box(s, c, h, mat, shade=1.0):
        s.hull([(c[0] + sx * h[0], c[1] + sy * h[1], c[2] + sz * h[2]) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)], mat, shade)

    def rod(s, a, b, r0, r1, mat, n=8, shade=1.0):
        """Tapered cylinder from point a (radius r0) to point b (radius r1)."""
        a, b = Vector(a), Vector(b)
        d = (b - a).normalized()
        ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
        u = d.cross(ref).normalized()
        v = d.cross(u).normalized()
        pts = []
        for end, r in ((a, r0), (b, r1)):
            for i in range(n):
                t = 2 * math.pi * i / n
                q = end + (u * math.cos(t) + v * math.sin(t)) * r
                pts.append((q.x, q.y, q.z))
        s.hull(pts, mat, shade)

    def ellipsoid(s, c, r, mat, rot=(0.0, 0.0, 0.0), rows=5, cols=8, shade=1.0):
        """Faceted egg: centre c, radii r=(rx, ry, rz), rotated by Euler angles `rot` (radians) about its centre."""
        e = Euler(rot, 'XYZ').to_matrix()
        pts = [(0, 1, 0), (0, -1, 0)]
        for i in range(1, rows):
            th = math.pi * i / rows
            for j in range(cols):
                ph = 2 * math.pi * j / cols + (0.5 * math.pi / cols if i % 2 else 0.0)
                pts.append((math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)))
        out = []
        for (x, y, z) in pts:
            v = e @ Vector((x * r[0], y * r[1], z * r[2]))
            out.append((c[0] + v.x, c[1] + v.y, c[2] + v.z))
        s.hull(out, mat, shade)

    def finish(s, folder):
        os.makedirs(folder, exist_ok=True)
        me = bpy.data.meshes.new(s.name)
        s.bm.to_mesh(me)
        s.bm.free()
        ob = bpy.data.objects.new(s.name, me)
        bpy.context.scene.collection.objects.link(ob)
        for m in s.mats:
            me.materials.append(material(m))
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.gltf(filepath=os.path.join(folder, s.name + '.glb'), export_format='GLB', use_selection=True,
                                  export_yup=True, export_apply=True, export_normals=True, export_vertex_color='ACTIVE',
                                  export_cameras=False, export_lights=False, export_materials='EXPORT', export_image_format='NONE')
        n = len(me.polygons)
        bpy.data.objects.remove(ob, do_unlink=True)
        return n
