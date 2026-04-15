"""
generate_town.py  —  Member 4 (World, Collision & Scene) - Phase 2
====================================================================
Procedurally generates the "Until Dawn" zombie-shooter scene and writes
it to config/game-phase2.jsonc.

Map Design
----------
• A 3×3 grid of buildings surrounds an open CENTRAL ARENA (~30×30 units)
  where gameplay occurs.  Block size = 8, street width = 5.
• The centre cell (0,0) is left open — this is the player spawn + fight area.
• 8 cover props (crates/barriers) are placed inside the arena for tactical cover.
• 4 perimeter INVISIBLE collision walls fence the entire map so neither the
  player nor zombies can escape.
• Lighting:
    - 1 directional moonlight  (eerie cold blue)
    - 1 centre spotlight       (harsh cone pointing straight down — dramatic)
    - 4 arena ring lamps       (warm sodium glow around the open area)
    - Per-building street lamps (warm point lights on alternate buildings)
"""

import json
import os
import random

# ── Config ────────────────────────────────────────────────────────────────────
SEED          = 42          # fixed seed → reproducible layout
CONFIG_PATH   = 'config/game-phase2.jsonc'
BLOCK         = 8           # building footprint (units)
STREET        = 5           # gap between buildings
CITY_HALF     = 3           # buildings in each direction (3×3 grid)
ARENA_HALF    = 14          # half-width of the open play area
MAP_BOUND     = 52          # half-extent of the invisible perimeter walls
WALL_HEIGHT   = 20          # perimeter wall height (taller than buildings)
CRATE_COUNT   = 8           # cover props in the arena

random.seed(SEED)

# Online environment mesh candidates.
# If files exist, they are registered and used automatically.
ONLINE_MESH_CANDIDATES = {
    "env_house_a": "assets/environment/modular/env_house_a.obj",
    "env_house_b": "assets/environment/modular/env_house_b.obj",
    "env_tower_a": "assets/environment/modular/env_tower_a.obj",
    "prop_barrel_a": "assets/environment/props/prop_barrel_a.obj",
    "prop_crate_a": "assets/environment/props/prop_crate_a.obj",
    "prop_cart_a": "assets/environment/props/prop_cart_a.obj",
}


def register_online_meshes(data):
    """Register locally available online meshes in the config asset table."""
    meshes = data["scene"]["assets"].setdefault("meshes", {})
    available = {}
    for mesh_name, mesh_path in ONLINE_MESH_CANDIDATES.items():
        if os.path.exists(mesh_path):
            meshes[mesh_name] = mesh_path
            available[mesh_name] = mesh_path
    return available


def compute_mesh_bounds(obj_path):
    """Return mesh stats for OBJ vertices; fallback to unit cube stats."""
    try:
        min_x = min_y = min_z = float("inf")
        max_x = max_y = max_z = float("-inf")
        has_vertex = False
        with open(obj_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("v "):
                    parts = line.split()
                    if len(parts) >= 4:
                        x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                        min_x, max_x = min(min_x, x), max(max_x, x)
                        min_y, max_y = min(min_y, y), max(max_y, y)
                        min_z, max_z = min(min_z, z), max(max_z, z)
                        has_vertex = True
        if not has_vertex:
            return {
                "size": (1.0, 1.0, 1.0),
                "min": (0.0, 0.0, 0.0),
                "max": (1.0, 1.0, 1.0),
                "center": (0.5, 0.5, 0.5),
            }
        dx = max(0.001, max_x - min_x)
        dy = max(0.001, max_y - min_y)
        dz = max(0.001, max_z - min_z)
        return {
            "size": (dx, dy, dz),
            "min": (min_x, min_y, min_z),
            "max": (max_x, max_y, max_z),
            "center": ((min_x + max_x) * 0.5, (min_y + max_y) * 0.5, (min_z + max_z) * 0.5),
        }
    except OSError:
        return {
            "size": (1.0, 1.0, 1.0),
            "min": (0.0, 0.0, 0.0),
            "max": (1.0, 1.0, 1.0),
            "center": (0.5, 0.5, 0.5),
        }

# ── Load existing config (preserves assets, window settings, etc.) ─────────────
with open(CONFIG_PATH, 'r') as f:
    data = json.load(f)

available_online_meshes = register_online_meshes(data)
mesh_bounds = {name: compute_mesh_bounds(path) for name, path in available_online_meshes.items()}

# ── Ensure lit shader registered ──────────────────────────────────────────────
if 'lit' not in data['scene']['assets']['shaders']:
    data['scene']['assets']['shaders']['lit'] = {
        'vs': 'assets/shaders/lit.vert',
        'fs': 'assets/shaders/lit.frag'
    }

# ── Upgrade all materials to lit type ─────────────────────────────────────────
for mat_name, mat in data['scene']['assets']['materials'].items():
    if mat.get('type') in ('textured', 'tinted'):
        mat['type']   = 'lit'
        mat['shader'] = 'lit'

# ── Ensure crate material exists ──────────────────────────────────────────────
MATS = data['scene']['assets']['materials']
if 'crate' not in MATS:
    MATS['crate'] = {
        'type': 'lit',
        'shader': 'lit',
        'pipelineState': {
            'faceCulling':  {'enabled': False},
            'depthTesting': {'enabled': True}
        },
        'tint': [0.38, 0.27, 0.16, 1.0]   # dark wood brown
    }

# ── Helper: lit pipeline block ─────────────────────────────────────────────────
def pipeline(cull=False):
    return {'faceCulling': {'enabled': cull}, 'depthTesting': {'enabled': True}}

# ══════════════════════════════════════════════════════════════════════════════
# BUILD WORLD ARRAY
# ══════════════════════════════════════════════════════════════════════════════
world = []

# ── (1) Camera — elevated behind the player spawn ─────────────────────────────
world.append({
    'name': 'camera',
    'position': [0, 15, 22],
    'rotation': [-30, 0, 0],
    'components': [
        {'type': 'Camera'},
        {'type': 'Free Camera Controller'}
    ]
})

# ── (2) Ground floor ──────────────────────────────────────────────────────────
world.append({
    'name': 'street_floor',
    'position': [0, 0, 0],
    'rotation': [-90, 0, 0],
    'scale': [MAP_BOUND * 2, MAP_BOUND * 2, 1],
    'components': [
        {'type': 'Mesh Renderer', 'mesh': 'plane', 'material': 'dark_stone'},
        {'type': 'Collider',     'size': [MAP_BOUND * 2, 1.0, MAP_BOUND * 2]},
        {'type': 'Environment',  'environmentType': 'floor'}
    ]
})

# ── (3) Moonlight — cold directional ──────────────────────────────────────────
world.append({
    'name': 'moonlight',
    'position': [0, 60, 0],
    'rotation': [-50, 30, 0],
    'components': [{
        'type': 'Light', 'lightType': 'directional',
        'color': [0.18, 0.22, 0.45], 'intensity': 0.7
    }]
})

# ── (4) Centre spotlight — dramatic harsh cone pointing down ───────────────────
world.append({
    'name': 'centre_spotlight',
    'position': [0, 18, 0],
    'rotation': [-90, 0, 0],      # pointing straight down
    'components': [{
        'type': 'Light', 'lightType': 'spot',
        'color': [0.9, 0.85, 0.7], 'intensity': 4.0,
        'attenuation': [1.0, 0.04, 0.002],
        'cone_angles': [15, 28]    # tight inner / soft outer (degrees)
    }]
})

# ── (5) Four arena ring lamps (warm sodium glow) ──────────────────────────────
ring_offsets = [(10, 10), (-10, 10), (10, -10), (-10, -10)]
for idx, (rx, rz) in enumerate(ring_offsets):
    world.append({
        'name': f'arena_lamp_{idx}',
        'position': [rx, 5, rz],
        'components': [{
            'type': 'Light', 'lightType': 'point',
            'color': [1.0, 0.72, 0.28], 'intensity': 3.0,
            'attenuation': [1.0, 0.09, 0.008]
        }]
    })

# ── (6) Buildings — 3×3 grid, skip centre ─────────────────────────────────────
building_materials = ['red_brick', 'stone_wall', 'dark_stone', 'metal']
online_building_meshes = [
    name for name in ('env_house_a', 'env_house_b', 'env_tower_a')
    if name in available_online_meshes
]

for bx in range(-CITY_HALF, CITY_HALF + 1):
    for bz in range(-CITY_HALF, CITY_HALF + 1):
        if bx == 0 and bz == 0:
            continue    # open centre arena

        cx = bx * (BLOCK + STREET)
        cz = bz * (BLOCK + STREET)

        # Skip positions that fall inside the arena ring (inner 1×1 around centre)
        if abs(bx) <= 1 and abs(bz) <= 1:
            # Make these low barriers instead of tall buildings
            height = random.uniform(2, 4)
        else:
            height = random.uniform(10, 26)

        mat = random.choice(building_materials)

        chosen_mesh = random.choice(online_building_meshes) if online_building_meshes else 'cube'
        if chosen_mesh in mesh_bounds:
            stats = mesh_bounds[chosen_mesh]
            dx, dy, dz = stats["size"]
            min_x, min_y, min_z = stats["min"]
            model_scale = [BLOCK / dx, height / dy, BLOCK / dz]
            # Align mesh to block bounds and ground even if pivot is off-center.
            desired_min_x = cx - BLOCK * 0.5
            desired_min_y = 0.0
            desired_min_z = cz - BLOCK * 0.5
            model_position = [
                desired_min_x - model_scale[0] * min_x,
                desired_min_y - model_scale[1] * min_y,
                desired_min_z - model_scale[2] * min_z,
            ]
        else:
            model_scale = [BLOCK, height, BLOCK]
            model_position = [cx, height / 2.0, cz]
        world.append({
            'name': f'building_{bx}_{bz}',
            'position': model_position,
            'scale':    model_scale,
            'components': [
                {'type': 'Mesh Renderer', 'mesh': chosen_mesh, 'material': mat},
                {'type': 'Collider',     'size': [BLOCK, height, BLOCK]},
                {'type': 'Environment',  'environmentType': 'wall'}
            ]
        })

        # Street lamps on every other building, at building edge facing arena
        if (abs(bx) + abs(bz)) % 2 == 1:
            lamp_x = cx - (BLOCK / 2 + 1) * (1 if cx > 0 else -1)
            world.append({
                'name': f'lamp_{bx}_{bz}',
                'position': [lamp_x, 4.5, cz],
                'components': [{
                    'type': 'Light', 'lightType': 'point',
                    'color': [1.0, 0.68, 0.25], 'intensity': 2.2,
                    'attenuation': [1.0, 0.12, 0.012]
                }]
            })

# ── (7) Cover props — crates/barriers inside the arena ────────────────────────
# Fixed positions for tactical gameplay (deterministic, not random)
crate_positions = [
    ( 5, 0,  5, 1.2, 1.2, 1.2),
    (-5, 0,  5, 1.2, 1.2, 1.2),
    ( 5, 0, -5, 1.2, 1.2, 1.2),
    (-5, 0, -5, 1.2, 1.2, 1.2),
    ( 8, 0,  0, 2.5, 1.0, 1.0),   # horizontal barrier
    (-8, 0,  0, 2.5, 1.0, 1.0),
    ( 0, 0,  8, 1.0, 1.0, 2.5),
    ( 0, 0, -8, 1.0, 1.0, 2.5),
]
online_prop_meshes = [
    name for name in ('prop_barrel_a', 'prop_crate_a', 'prop_cart_a')
    if name in available_online_meshes
]
for idx, (cx, cy, cz, sx, sy, sz) in enumerate(crate_positions):
    chosen_prop_mesh = random.choice(online_prop_meshes) if online_prop_meshes else 'cube'
    if chosen_prop_mesh in mesh_bounds:
        stats = mesh_bounds[chosen_prop_mesh]
        dx, dy, dz = stats["size"]
        min_x, min_y, min_z = stats["min"]
        prop_scale = [sx / dx, sy / dy, sz / dz]
        desired_min_x = cx - sx * 0.5
        desired_min_y = 0.0
        desired_min_z = cz - sz * 0.5
        prop_position = [
            desired_min_x - prop_scale[0] * min_x,
            desired_min_y - prop_scale[1] * min_y,
            desired_min_z - prop_scale[2] * min_z,
        ]
    else:
        prop_scale = [sx, sy, sz]
        prop_position = [cx, sy / 2.0, cz]
    world.append({
        'name': f'crate_{idx}',
        'position': prop_position,
        'scale':    prop_scale,
        'components': [
            {'type': 'Mesh Renderer', 'mesh': chosen_prop_mesh, 'material': 'crate'},
            {'type': 'Collider',     'size': [sx, sy, sz]},
            {'type': 'Environment',  'environmentType': 'prop'}
        ]
    })

# ── (8) Perimeter invisible collision walls (keeps player + zombies in map) ────
# Four thin walls around the border — no mesh renderer, collider only
perim = MAP_BOUND
thick = 2.0
walls_def = [
    # name            pos-x   pos-y          pos-z   sx           sy            sz
    ('wall_north',     0,     WALL_HEIGHT/2,  perim,  perim*2,    WALL_HEIGHT,  thick),
    ('wall_south',     0,     WALL_HEIGHT/2, -perim,  perim*2,    WALL_HEIGHT,  thick),
    ('wall_east',      perim, WALL_HEIGHT/2,  0,      thick,      WALL_HEIGHT,  perim*2),
    ('wall_west',     -perim, WALL_HEIGHT/2,  0,      thick,      WALL_HEIGHT,  perim*2),
]
for wname, wx, wy, wz, wsx, wsy, wsz in walls_def:
    world.append({
        'name': wname,
        'position': [wx, wy, wz],
        'components': [
            {'type': 'Collider',    'size': [wsx, wsy, wsz]},
            {'type': 'Environment', 'environmentType': 'wall'}
        ]
    })

# ── (9) Write back ─────────────────────────────────────────────────────────────
data['scene']['world']                              = world
data['scene']['renderer']['postprocess']            = 'assets/shaders/blit.frag'
data['start-scene']                                 = 'play-phase2'

with open(CONFIG_PATH, 'w') as f:
    json.dump(data, f, indent=4)

# ── Summary ────────────────────────────────────────────────────────────────────
total        = len(world)
lights       = sum(1 for e in world if any(c.get('type') == 'Light'       for c in e.get('components', [])))
colliders    = sum(1 for e in world if any(c.get('type') == 'Collider'    for c in e.get('components', [])))
renderers    = sum(1 for e in world if any(c.get('type') == 'Mesh Renderer' for c in e.get('components', [])))

print("=" * 50)
print("  Procedural horror map generated successfully!")
print("=" * 50)
print(f"  Total entities   : {total}")
print(f"  Lights           : {lights}")
print(f"  Collidables      : {colliders}")
print(f"  Mesh renderers   : {renderers}")
print(f"  Online meshes    : {len(available_online_meshes)} detected")
if available_online_meshes:
    for mesh_name, mesh_path in sorted(available_online_meshes.items()):
        dx, dy, dz = mesh_bounds.get(mesh_name, {"size": (1.0, 1.0, 1.0)})["size"]
        print(f"    - {mesh_name}: {mesh_path} (bounds: {dx:.3f}, {dy:.3f}, {dz:.3f})")
else:
    print("    - none (using cube fallback)")
print(f"  Postprocess      : blit.frag (neutral visibility pass)")
print(f"  Config written   : {CONFIG_PATH}")
print("=" * 50)
