#!/usr/bin/env python3
"""
Journey - the layout of the whole world (pure Python: no Blender needed).

The world is 192 x 400 tiles. The original long strip (the castle town, the road south, Millbrook, the lake, the mountains, the caves,
the desert and the Abyss: 64 x 336 tiles, built by the hand-written part of build_town.py) sits in the middle of it, at CORE = (64, 48)..(128, 384).
Everything around the core is described here: the land to the west across the river, the east down to the sea, the mountains in the north and
the dark cliffs in the south. This file decides, for every tile of the world,

    biome_at(x, z)      what kind of country it is (meadow, pine forest, swamp, desert...), with soft, dithered edges between neighbours,
    is_water(x, z)      river, lake, ponds and sea,
    road_tiles()        the roads (and the three bridges),
    AREAS               the named places for the map file and the HUD,

and build_town.py / world_wilds.py turn that into ground, props, trees, monsters and people.

Run `python tools/worldmap.py` to write a picture of the layout (build/worldmap.png) without opening Blender.
"""
import math

WW, HH = 192, 400
OX, OZ = 64, 48                     # where the core's tile (0, 0) is in the world
CORE_W, CORE_H = 64, 336
CORE = (OX, OZ, OX + CORE_W, OZ + CORE_H)   # x0, z0, x1, z1 (exclusive)
CHUNK = 16


# ------------------------------------------------------------------------------------------------ noise
def hash01(x, z, seed=0):
    """A fixed pseudo-random number in [0, 1) for a tile (the same on every run)."""
    n = (int(x) * 73856093) ^ (int(z) * 19349663) ^ (seed * 83492791)
    n = (n ^ (n >> 13)) * 1274126177
    n = (n ^ (n >> 16)) & 0x7FFFFFFF
    return n / 2147483648.0


def smooth(t):
    return t * t * (3 - 2 * t)


def vnoise(x, z, scale, seed=0):
    """Smooth value noise in [0, 1): features about `scale` tiles across."""
    gx, gz = x / scale, z / scale
    x0, z0 = int(math.floor(gx)), int(math.floor(gz))
    fx, fz = smooth(gx - x0), smooth(gz - z0)
    a, b = hash01(x0, z0, seed), hash01(x0 + 1, z0, seed)
    c, d = hash01(x0, z0 + 1, seed), hash01(x0 + 1, z0 + 1, seed)
    return (a * (1 - fx) + b * fx) * (1 - fz) + (c * (1 - fx) + d * fx) * fz


def in_rect(x, z, r):
    return r[0] <= x < r[2] and r[1] <= z < r[3]


def in_core(x, z):
    return in_rect(x, z, CORE)


# ------------------------------------------------------------------------------------------------ water
def core_wx(zl):
    """The river's east edge in the core (local tiles): the same formula as build_town.py."""
    base = 5 + int(round(1.5 * math.sin(zl / 6.0)))
    if 180 <= zl <= 214:
        base += int(round(15 * math.sin(math.pi * (zl - 180) / 34.0)))
    return base


def river_edges(z):
    """World x of the river's west bank (first water tile) and east bank (first land tile) at row z."""
    zl = z - OZ
    west = 56 + int(round(1.5 * math.sin(z / 9.0)))
    if 176 <= zl <= 218:   # the lake swells to the west as well
        west -= int(round(13 * math.sin(math.pi * (zl - 176) / 42.0)))
    return west, OX + core_wx(zl)


SEA_Z = (60, 252)     # the east coast: the sea runs along the edge of the map between these rows


def coast_x(z):
    """First sea tile at row z (192 = no sea here)."""
    if not (SEA_Z[0] <= z < SEA_Z[1]):
        return WW
    edge = min(z - SEA_Z[0], SEA_Z[1] - 1 - z) / 18.0
    t = smooth(min(1.0, max(0.0, edge)))
    base = 184 + int(round(2.5 * math.sin(z / 13.0)))
    return int(round(WW - t * (WW - base)))


PONDS = [  # (cx, cz, rx, rz): small lakes in the swamp and a pool at the oasis
    (22, 238, 6, 4), (40, 252, 5, 4), (14, 268, 7, 5), (34, 280, 5, 3), (8, 250, 4, 3), (46, 290, 4, 3), (26, 258, 3, 2),
    (158, 340, 7, 5),   # the oasis
]


def is_water(x, z):
    """River, lake, ponds and sea. Only meaningful outside the core (inside it the core builder decides)."""
    if not (0 <= x < WW and 0 <= z < HH):
        return False
    w, e = river_edges(z)
    if w <= x < e:
        return True
    if x >= coast_x(z):
        return True
    for (cx, cz, rx, rz) in PONDS:
        if ((x - cx) / rx) ** 2 + ((z - cz) / rz) ** 2 <= 1.0:
            return True
    return False


# ------------------------------------------------------------------------------------------------ biomes
# (name, rect, biome): painted in order, later ones win. The rects are rough: their edges are warped and dithered (see biome_at).
REGIONS = [
    ((0, 0, WW, HH), 'meadow'),
    ((0, 0, WW, 15), 'mountain'),                 # the Frostpeak Range along the north edge
    ((0, 15, WW, 32), 'snow'),                    # snowy pines below the peaks
    ((0, 32, WW, 56), 'pine'),                    # pine forest
    ((0, 56, 56, 112), 'pine'),                   # Whispering Pines (west of the castle)
    ((0, 112, 56, 172), 'forest'),                # Elmwood
    ((0, 172, 56, 226), 'farm'),                  # Greenmarch
    ((0, 226, 56, 302), 'swamp'),                 # Mirewood Swamp
    ((0, 302, 56, 368), 'barrow'),                # Barrow Downs
    ((0, 368, 56, HH), 'ash'),                    # the black cliffs at the south-west corner
    ((128, 56, 184, 114), 'dry_meadow'),          # Eastwatch Downs
    ((128, 114, 184, 226), 'farm'),               # the Sunfields
    ((128, 226, 192, 266), 'darkforest'),         # Darkwood
    ((128, 266, 192, 292), 'moor'),               # Giant's Rise
    ((128, 292, 192, 324), 'mountain'),           # the Iron Range
    ((128, 324, 192, 372), 'desert'),             # the Dune Sea
    ((128, 372, 192, HH), 'ash'),                 # the black cliffs at the south-east corner
    ((128, 350, 138, 384), 'ash'),                # the Abyss's east wall
    ((56, 384, 138, HH), 'ash'),                  # ...and its south wall
]

# Patches of a different kind of country inside a big region: (biome, variant, scale in tiles, noise threshold)
VARIANTS = [
    ('meadow', 'dry_meadow', 30.0, 0.66),
    ('meadow', 'pine', 26.0, 0.72),
    ('forest', 'meadow', 22.0, 0.70),
    ('forest', 'darkforest', 28.0, 0.66),
    ('farm', 'meadow', 24.0, 0.68),
    ('swamp', 'forest', 30.0, 0.74),
    ('moor', 'mountain', 20.0, 0.70),
    ('desert', 'moor', 26.0, 0.78),
    ('darkforest', 'pine', 24.0, 0.72),
    ('barrow', 'swamp', 26.0, 0.74),
]

# Where the black rock is a wall (nobody can climb it): the south-west, south-east and south edges and the Abyss's flanks
CLIFFS = [(0, 368, 56, HH), (128, 372, 192, HH), (128, 350, 138, 384), (56, 384, 138, HH)]
# And the mountains: the Frostpeak Range, and the Iron Range apart from the passes the roads go through
MOUNTAIN_PASSES = [(150, 322, 164, 326)]

# the ground of each biome: (material A, material B) as in T.floor(); B-style builds use one material and vary the colour
GROUND = {
    'meadow': ('grass_a', 'grass_b'),
    'dry_meadow': ('dry_grass', 'grass_c'),
    'pine': ('grass_dark', 'moss'),
    'snow': ('snow', 'grass_dark'),
    'mountain': ('highland', 'highland_b'),
    'forest': ('grass_dark', 'grass_b'),
    'darkforest': ('moss', 'grass_dark'),
    'farm': ('grass_a', 'wheat'),
    'swamp': ('mud', 'moss'),
    'barrow': ('highland_b', 'moss'),
    'moor': ('highland', 'dry_grass'),
    'desert': ('red_sand', 'sand'),
    'ash': ('abyss', 'abyss_b'),
    'beach': ('sand', 'sand'),
}
NATURAL = {'meadow', 'dry_meadow', 'pine', 'snow', 'forest', 'darkforest', 'farm', 'swamp', 'barrow', 'moor', 'desert'}   # (open to walk through)


def _region(x, z):
    b = 'meadow'
    for rect, name in REGIONS:
        if in_rect(x, z, rect):
            b = name
    return b


def biome_at(x, z):
    """The kind of country at world tile (x, z). Edges between regions are warped by slow noise and then dithered tile by tile, so one
    kind of country fades into the next over a band of about ten tiles instead of changing along a ruled line."""
    wx = (vnoise(x, z, 23.0, 1) - 0.5) * 16.0
    wz = (vnoise(x, z, 19.0, 2) - 0.5) * 16.0
    jx = (hash01(x, z, 3) - 0.5) * 9.0
    jz = (hash01(x, z, 4) - 0.5) * 9.0
    qx = min(WW - 1.0, max(0.0, x + wx + jx))
    qz = min(HH - 1.0, max(0.0, z + wz + jz))
    b = _region(qx, qz)
    for (base, variant, scale, thr) in VARIANTS:
        if b == base and vnoise(x, z, scale, 40 + len(base)) > thr:
            b = variant
            break
    cx = coast_x(z)
    if b in NATURAL and cx - 6 <= x < cx:     # the beach before the sea
        return 'beach'
    return b


# ------------------------------------------------------------------------------------------------ roads
# Roads are polylines in world tiles (x, z): 'dirt' roads 3 tiles wide. The bridges are (z, x0, x1) across the river.
ROADS = [
    # the east highway: from the castle's east road to the harbour at Saltmere
    [(128, 88), (140, 88), (152, 90), (164, 90), (172, 92)],
    # the west highway: over the bridge at Castle Town to Elmswick
    [(73, 88), (58, 88), (48, 90), (38, 94), (30, 98)],
    # Elmswick north to the woodcutters in the pines, and south into Elmwood
    [(30, 98), (26, 84), (20, 70), (14, 58)],
    [(30, 98), (28, 114), (26, 130), (30, 146)],
    # north: from the highway round the castle's east side to the old watchtower
    [(134, 88), (136, 72), (132, 58), (124, 46), (118, 38)],
    # Millbrook east: out to the Sunfields and the coast
    [(128, 178), (140, 178), (152, 174), (164, 180), (176, 188)],
    # the ford west of Millbrook, the farm tracks, and the way down the west bank to the swamp
    [(73, 190), (58, 190), (46, 192), (38, 198), (32, 206), (28, 220), (28, 234)],
    [(44, 192), (40, 178), (36, 166)],
    [(38, 198), (22, 204), (12, 200)],
    # Lakeside east: into Darkwood
    [(128, 250), (140, 252), (152, 248), (166, 246), (180, 244)],
    # the old bridge south of the lake, west to the barrows
    [(73, 262), (58, 262), (46, 266), (36, 280), (28, 298), (22, 318), (24, 338)],
    # east of the troll pass: up to Giant's Rise
    [(128, 285), (142, 284), (156, 282), (170, 280)],
    # the desert road east to the oasis
    [(128, 331), (140, 333), (152, 335)],
    # a track up to the Iron Range
    [(150, 283), (152, 296), (156, 310), (156, 324)],
    # the Sunfields to Darkwood
    [(176, 186), (172, 206), (170, 224), (172, 240)],
]
BRIDGES = [(88, 3), (190, 3), (262, 3)]   # (row of the middle of the deck, width) over the whole river
# Strips of earth laid over the core's own ground where a road comes in from outside: (x0, z0, x1, z1) world tiles, x1 / z1 exclusive.
# (trees in them are cleared; the tiles must be open ground)
CORE_LINKS = [
    (66, 189, 95, 192),     # the ford road to Millbrook's main street
    (110, 177, 128, 180),   # from Millbrook square east
    (98, 249, 128, 252),    # through the Wolf Wood from the Lakeside road
    (68, 261, 92, 264),     # from the south bridge to the road below the lake
    (99, 284, 128, 287),    # east through the troll pass
    (100, 330, 128, 333),   # the desert road east
]
ROAD_WIDTH = 3


def road_tiles():
    """{(x, z): 'dirt'} for every road tile outside the core, with a little wobble so they are not ruled lines."""
    out = {}
    for line in ROADS:
        for (x0, z0), (x1, z1) in zip(line, line[1:]):
            n = int(max(abs(x1 - x0), abs(z1 - z0))) * 2
            for i in range(n + 1):
                t = i / max(1, n)
                x = x0 + (x1 - x0) * t
                z = z0 + (z1 - z0) * t
                wob = (vnoise(x, z, 9.0, 7) - 0.5) * 2.4
                if abs(x1 - x0) >= abs(z1 - z0):
                    z += wob
                else:
                    x += wob
                for dz in range(-1, 2):
                    for dx in range(-1, 2):
                        if abs(dx) + abs(dz) <= 1 or ROAD_WIDTH > 3:
                            out[(int(x) + dx, int(z) + dz)] = 'dirt'
    return out


# ------------------------------------------------------------------------------------------------ places
# (x0, z0, x1, z1, name): later entries win where they overlap. The core's own places are in build_town.py (AREAS) and keep their names.
WILD_AREAS = [
    (0, 0, WW, 15, 'Frostpeak Range'),
    (0, 15, WW, 56, 'The Northern Reach'),
    (56, 15, 128, 50, 'Northwatch Moor'),
    (0, 56, 56, 112, 'Whispering Pines'),
    (0, 112, 56, 172, 'Elmwood'),
    (14, 82, 48, 112, 'Elmswick'),
    (0, 172, 56, 226, 'Greenmarch'),
    (0, 226, 56, 302, 'Mirewood Swamp'),
    (128, 56, 184, 114, 'Eastwatch Downs'),
    (140, 78, 182, 106, 'Saltmere'),
    (184, 56, 192, 252, 'The Eastern Sea'),
    (128, 114, 184, 226, 'The Sunfields'),
    (128, 226, 192, 266, 'Darkwood'),
    (128, 266, 192, 292, "Giant's Rise"),
    (128, 292, 192, 324, 'The Iron Range'),
    (128, 324, 192, 372, 'The Dune Sea'),
    (0, 302, 56, 368, 'Barrow Downs'),
]


# ------------------------------------------------------------------------------------------------ blending into the core
def mirror_into_wild(xw, zw):
    """For a tile just inside the core's edge: the world tile just outside it, mirrored across the edge, whose country it should resemble."""
    x, z = xw, zw
    if x >= CORE[2] - 20:
        x = 2 * CORE[2] - 1 - x
    elif x < CORE[0] + 6:
        x = 2 * CORE[0] - 1 - x
    if z < CORE[1] + 14:
        z = 2 * CORE[1] - 1 - z
    elif z >= CORE[3] - 14:
        z = 2 * CORE[3] - 1 - z
    return x, z


EDGE_BAND = 16   # how far into the core the neighbouring country creeps


def core_edge_material(m, xl, zl):
    """Called by the core builder for every ground tile (core-local coordinates, `m` the material it chose): near the core's edges, natural
    ground gives way to the neighbouring country so there is no seam where the hand-built world ends."""
    if not (m and (m.startswith('grass') or m in ('highland', 'highland_b', 'sand', 'abyss', 'abyss_b'))):
        return m
    d = min(CORE_W - 1 - xl, zl, CORE_H - 1 - zl)    # distance to the north / east / south edge (the west edge is the river)
    if d >= EDGE_BAND:
        return m
    p = ((EDGE_BAND - d) / EDGE_BAND) ** 1.4
    if hash01(xl, zl, 11) >= p:
        return m
    xw, zw = mirror_into_wild(xl + OX, zl + OZ)
    b = biome_at(xw, zw)
    if b in GROUND:
        a, bb = GROUND[b]
        return a if ((xl + zl) & 1) == 0 else bb
    return m


# seams inside the core between its hand-built regions (core-local rows): (row, band, material above, material below)
CORE_SEAMS = [
    (216, 8, 'grass', 'highland'),    # the lake country climbs into the Troll Mountains
    (304, 7, 'sand', 'abyss'),        # the darkness creeps into the desert
]


def core_seam_material(m, xl, zl):
    for (row, band, above, below) in CORE_SEAMS:
        if abs(zl - row) >= band:
            continue
        d = abs(zl - row)
        p = 0.5 * ((band - d) / band) ** 1.3         # at the seam itself half the tiles are the other kind
        if hash01(xl, zl, 21) >= p:
            return m
        if zl < row and m.startswith(above) and above == 'grass':
            return 'highland' if ((xl + zl) & 1) == 0 else 'highland_b'
        if zl < row and m == above:
            return 'abyss' if ((xl + zl) & 1) == 0 else 'abyss_b'
        if zl >= row and m in ('highland', 'highland_b') and below == 'highland':
            return 'grass_a' if ((xl + zl) & 1) == 0 else 'grass_b'
        if zl >= row and m in ('abyss', 'abyss_b') and below == 'abyss':
            return 'sand'
    return m


# ------------------------------------------------------------------------------------------------ the picture
def _preview(path):
    from PIL import Image, ImageDraw
    S = 4
    colour = {
        'meadow': (110, 170, 70), 'dry_meadow': (160, 160, 80), 'pine': (50, 110, 70), 'snow': (225, 232, 238), 'mountain': (130, 124, 112),
        'forest': (50, 130, 60), 'darkforest': (40, 90, 60), 'farm': (190, 175, 90), 'swamp': (88, 100, 70), 'barrow': (112, 106, 96),
        'moor': (140, 130, 100), 'desert': (212, 160, 100), 'ash': (50, 42, 58), 'beach': (230, 214, 160),
    }
    img = Image.new('RGB', (WW * S, HH * S), (0, 0, 0))
    px = img.load()
    roads = road_tiles()
    for z in range(HH):
        for x in range(WW):
            if in_core(x, z):
                c = (170, 150, 120)
                xl, zl = x - OX, z - OZ
                if zl < 64:
                    c = (150, 150, 140)
                elif zl >= 304:
                    c = (60, 46, 70)
                elif zl >= 272:
                    c = (214, 190, 130)
                elif zl >= 240:
                    c = (84, 76, 70)
                elif zl >= 216:
                    c = (130, 124, 108)
                elif zl >= 176:
                    c = (100, 160, 80)
                else:
                    c = (116, 168, 84)
                if xl < core_wx(zl):
                    c = (60, 120, 200)
            elif is_water(x, z):
                c = (60, 120, 200)
            elif (x, z) in roads:
                c = (150, 110, 70)
            else:
                c = colour[biome_at(x, z)]
            for dy in range(S):
                for dx in range(S):
                    px[x * S + dx, z * S + dy] = c
    d = ImageDraw.Draw(img)
    d.rectangle([CORE[0] * S, CORE[1] * S, CORE[2] * S - 1, CORE[3] * S - 1], outline=(255, 255, 255))
    for (x0, z0, x1, z1, name) in WILD_AREAS:
        d.text(((x0 + 2) * S, (z0 + 2) * S), name, fill=(255, 255, 255))
    for (z, w) in BRIDGES:
        wst, est = river_edges(z)
        d.rectangle([(wst - 1) * S, (z - 1) * S, (est + 1) * S, (z + 2) * S], outline=(120, 70, 30))
    for (x0, z0, x1, z1) in CORE_LINKS:
        d.rectangle([x0 * S, z0 * S, x1 * S, z1 * S], outline=(200, 140, 60))
    img.save(path)
    print('wrote', path, img.size)


if __name__ == '__main__':
    import os, sys
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'build', 'worldmap.png')
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    _preview(out)
