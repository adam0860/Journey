#!/usr/bin/env python3
"""Quick Blender-side look at assembled characters (shape check). usage: preview_character.py out.png g hair shirt pants shoes"""
import bpy, sys, os, math
from mathutils import Vector
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
D = os.path.join(ROOT, 'assets', 'models', 'character')
out = sys.argv[1]
combos = [a.split(',') for a in sys.argv[2:]]   # g,hair,shirt,pants,shoes
JOINTS = {}
for l in open(os.path.join(D, 'catalog.txt')):
    t = l.split()
    if t and t[0] == 'joint':
        JOINTS[t[1]] = (float(t[2]), float(t[3]), float(t[4]))
bpy.ops.wm.read_factory_settings(use_empty=True)
COL = {'skin': (0.9, 0.65, 0.5), 'hair': (0.25, 0.15, 0.08), 'shirt': (0.2, 0.4, 0.8), 'pants': (0.35, 0.3, 0.25), 'shoes': (0.15, 0.1, 0.08)}

def part(name, kind, joint, dx):
    p = os.path.join(D, name + '.glb')
    if not os.path.exists(p): return
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=p)
    for ob in set(bpy.data.objects) - before:
        if ob.type == 'MESH':
            ob.parent = None
            j = JOINTS[joint]
            ob.location = Vector((j[0] + dx, -j[2], j[1]))   # char space -> blender space
            ob.color = COL[kind] + (1,)
        else:
            bpy.data.objects.remove(ob)

for i, (g, hair, shirt, pants, shoes) in enumerate(combos):
    dx = i * 0.9
    part('body_%s_head' % g, 'skin', 'head', dx); part('body_%s_torso' % g, 'skin', 'torso', dx)
    for s in 'lr':
        part('body_arm_' + s, 'skin', 'arm_' + s, dx); part('body_leg_' + s, 'skin', 'leg_' + s, dx)
        part('shirt_%s_sleeve_%s' % (shirt, s), 'shirt', 'arm_' + s, dx)
        part('pants_%s_leg_%s' % (pants, s), 'pants', 'leg_' + s, dx)
        part('shoes_%s_%s' % (shoes, s), 'shoes', 'leg_' + s, dx)
    part('hair_' + hair, 'hair', 'head', dx)
    part('shirt_%s_torso_%s' % (shirt, g), 'shirt', 'torso', dx)
    part('pants_%s_hips_%s' % (pants, g), 'pants', 'torso', dx)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading
sh.light = 'STUDIO'; sh.color_type = 'OBJECT'; sh.show_cavity = False
sc.world = bpy.data.worlds.new('w'); sc.world.color = (0.55, 0.62, 0.72)
n = len(combos)
cx = (n - 1) * 0.45
cam = bpy.data.cameras.new('c'); cam.type = 'ORTHO'; cam.ortho_scale = max(2.1, n * 0.95 + 0.2)
co = bpy.data.objects.new('c', cam); sc.collection.objects.link(co); sc.camera = co
sc.render.resolution_x = int(260 * n * 1.0) + 40; sc.render.resolution_y = 560
import mathutils
def shot(path, front=True, yaw=0.0):
    # front view looks toward -Blender Y?  forward in Blender is -Y, so a viewer in front sits at -Y looking +Y
    d = -1 if front else 1
    co.location = (cx, d * 10, 0.9)
    co.rotation_euler = (math.radians(90), 0, 0 if front else math.radians(180))
    if yaw:
        co.location = (cx + 10 * math.sin(yaw), -10 * math.cos(yaw), 0.9)
        co.rotation_euler = (math.radians(90), 0, yaw)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
shot(out.replace('.png', '_front.png'), True)
shot(out.replace('.png', '_back.png'), False)
shot(out.replace('.png', '_side.png'), True, math.radians(75))
