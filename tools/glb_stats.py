#!/usr/bin/env python3
"""Print per-primitive vertex counts of GLBs (raylib needs < 65536 per primitive)."""
import sys, json, struct, glob, os
worst = 0; total = 0
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.glb'))):
    d = open(f, 'rb').read()
    n = struct.unpack('<I', d[12:16])[0]
    j = json.loads(d[20:20 + n])
    mx = 0
    for m in j['meshes']:
        for p in m['primitives']:
            v = j['accessors'][p['attributes']['POSITION']]['count']
            mx = max(mx, v); total += v
    worst = max(worst, mx)
    print('%-18s prims=%-3d maxverts=%-6d imgs=%-3d %6.0f KB' % (os.path.basename(f), sum(len(m['primitives']) for m in j['meshes']), mx, len(j.get('images', [])), len(d) / 1024))
print('WORST primitive: %d   total verts: %d' % (worst, total))
