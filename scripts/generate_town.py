import json
import random

config_path = '/home/gehad/GFX-Project/config/game-phase2.jsonc'

def remove_comments(text):
    # a basic JSON strip comments if it was custom jsonc, but python json doesn't support comments.
    # We will read it purely. Actually wait, game-phase2.jsonc doesn't have comments in its source currently.
    return text

with open(config_path, 'r') as f:
    data = json.load(f)

# Update all materials to be "lit" instead of "tinted" or "textured" except for grass/wood/glass if needed.
for mat_name, mat in data["scene"]["assets"]["materials"].items():
    if mat["type"] in ["textured", "tinted"]:
        mat["type"] = "lit"
        mat["shader"] = "lit"
        
# Add a "lit" shader to assets if not present
if "lit" not in data["scene"]["assets"]["shaders"]:
    data["scene"]["assets"]["shaders"]["lit"] = {
        "vs": "assets/shaders/lit.vert",
        "fs": "assets/shaders/lit.frag"
    }

# Rebuild the world array to contain a procedural town
world = []

# 1. Camera (Elevated to see the town)
world.append({
    "name": "camera",
    "position": [0, 15, 20],
    "rotation": [-30, 0, 0],
    "components": [
        { "type": "Camera" },
        { "type": "Free Camera Controller" }
    ]
})

# 2. Floor
world.append({
    "name": "street_floor",
    "position": [0, 0, 0],
    "rotation": [-90, 0, 0],
    "scale": [100, 100, 1],
    "components": [
        { "type": "Mesh Renderer", "mesh": "plane", "material": "dark_stone" },
        { "type": "Collider", "size": [100, 100, 1] },
        { "type": "Environment", "environmentType": "floor" }
    ]
})

# 3. Ambient Moon Light (Directional)
world.append({
    "name": "moonlight",
    "position": [0, 50, 0],
    "rotation": [-45, 45, 0],
    "components": [
        { "type": "Light", "lightType": "directional", "color": [0.2, 0.25, 0.5], "intensity": 0.8 }
    ]
})

# 4. Generate City Blocks
block_size = 8
street_width = 6
city_size = 4  # 4x4 blocks

materials = ["red_brick", "stone_wall", "dark_stone", "metal"]

for x in range(-city_size, city_size):
    for z in range(-city_size, city_size):
        # Skip center block for an open plaza/spawn point
        if x == 0 and z == 0:
            continue

        cx = x * (block_size + street_width)
        cz = z * (block_size + street_width)
        
        # Build tall building
        height = random.uniform(8, 25)
        mat = random.choice(materials)
        
        world.append({
            "name": f"building_{x}_{z}",
            "position": [cx, height/2.0, cz],
            "scale": [block_size, height, block_size],
            "components": [
                { "type": "Mesh Renderer", "mesh": "cube", "material": mat },
                { "type": "Collider", "size": [block_size, height, block_size] },
                { "type": "Environment", "environmentType": "wall" }
            ]
        })
        
        # Place street lamps between buildings
        if random.random() > 0.3:
            world.append({
                "name": f"lamp_{x}_{z}",
                "position": [cx + block_size/2.0 + 1, 4, cz],
                "components": [
                    { "type": "Light", "lightType": "point", "color": [1.0, 0.7, 0.3], "intensity": 2.5, "attenuation": [1.0, 0.1, 0.01] }
                ]
            })

data["scene"]["world"] = world

with open(config_path, 'w') as f:
    json.dump(data, f, indent=4)

print("Procedural town generated successfully!")
