#!/usr/bin/env python3
"""Imports the exported town GLBs into Blender and renders preview stills (Cycles, CPU).
Usage: python3 tools/render_preview.py out_dir [view ...]     views: overview castle plaza throne bank inn"""
import bpy, math, os, sys
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MODELS = os.path.join(ROOT, 'assets', 'models', 'town')
out_dir = sys.argv[1] if len(sys.argv) > 1 else '/tmp/previews'
views = sys.argv[2:] or ['overview']
os.makedirs(out_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
hide_roofs = 'throne' in views or 'bank' in views or 'inn' in views
for f in sorted(os.listdir(MODELS)):
    if not f.endswith('.glb'):
        continue
    bpy.ops.import_scene.gltf(filepath=os.path.join(MODELS, f))

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 24
scene.cycles.use_denoising = True
scene.render.resolution_x, scene.render.resolution_y = 1280, 720
scene.view_settings.view_transform = 'Standard'

world = bpy.data.worlds.new('w'); scene.world = world; world.use_nodes = True
bg = world.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (0.45, 0.62, 0.85, 1); bg.inputs['Strength'].default_value = 1.1

sun = bpy.data.lights.new('sun', 'SUN'); sun.energy = 3.2; sun.angle = math.radians(8)
so = bpy.data.objects.new('sun', sun); scene.collection.objects.link(so)
so.rotation_euler = (math.radians(48), math.radians(8), math.radians(-52))

cam = bpy.data.cameras.new('cam'); cam.lens = 35
co = bpy.data.objects.new('cam', cam); scene.collection.objects.link(co); scene.camera = co


def look(eye, target):
    """eye/target given in TILE coords (x, z, h)."""
    e = Vector((eye[0], -eye[1], eye[2])); t = Vector((target[0], -target[1], target[2]))
    co.location = e
    co.rotation_euler = (t - e).to_track_quat('-Z', 'Y').to_euler()


def hide(name):
    for ob in bpy.data.objects:
        if ob.name.startswith(name):
            ob.hide_render = True


VIEWS = {
    'overview': ((32, 78, 40), (32, 30, 0)),
    'castle':   ((32, 42, 14), (32, 14, 2.5)),
    'plaza':    ((32, 60, 12), (32, 40, 1)),
    'throne':   ((32, 26, 12), (32, 10, 0.5)),
    'bank':     ((38, 46, 9), (46, 34, 1)),
    'inn':      ((38, 58, 9), (47, 48, 1)),
    'river':    ((14, 28, 6), (3, 34, 0)),
}
for v in views:
    for ob in bpy.data.objects:
        ob.hide_render = False
    if v == 'throne': hide('roof_keep')
    if v == 'bank': hide('roof_bank')
    if v == 'inn': hide('roof_inn')
    look(*VIEWS[v])
    scene.render.filepath = os.path.join(out_dir, v + '.png')
    bpy.ops.render.render(write_still=True)
    print('rendered', v)
