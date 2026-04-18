# Member 4 - Immediate Startup Checklist

## Step 1: Understand the Codebase ✓
- [x] Created ColliderComponent for AABB collision boundaries
- [x] Created EnvironmentComponent to label environment pieces
- [x] Created CollisionSystem for collision detection
- [x] Created SceneManager helper functions
- [x] Updated CMakeLists.txt to include new files
- [x] Updated component-deserializer.hpp

## Step 2: Build the Project
```bash
cd /home/gehad/GFX-Project
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

If you get errors, they're likely:
- Missing includes (check file paths)
- Compilation errors (check syntax)
- Link errors (check CMakeLists.txt)

## Step 3: Create a Simple Test Scene
Create `config/test-collision.jsonc`:

```json
{
    "start-scene": "play",
    "window": {
        "title": "Collision Test",
        "size": { "width": 800, "height": 600 },
        "fullscreen": false
    },
    "scene": {
        "assets": {
            "shaders": {
                "tinted": {
                    "vs": "assets/shaders/tinted.vert",
                    "fs": "assets/shaders/tinted.frag"
                }
            },
            "meshes": {
                "cube": "assets/models/cube.obj",
                "plane": "assets/models/plane.obj"
            },
            "samplers": { "default": {} },
            "materials": {
                "red": {
                    "type": "tinted",
                    "shader": "tinted",
                    "pipelineState": { "faceCulling": { "enabled": false } },
                    "tint": [1, 0, 0, 1]
                },
                "green": {
                    "type": "tinted",
                    "shader": "tinted",
                    "pipelineState": { "faceCulling": { "enabled": false } },
                    "tint": [0, 1, 0, 1]
                }
            }
        },
        "world": [
            {
                "position": [0, 2, 0],
                "components": [
                    { "type": "Camera" }
                ]
            },
            {
                "position": [0, 0, 0],
                "rotation": [-90, 0, 0],
                "scale": [10, 10, 1],
                "components": [
                    {
                        "type": "Mesh Renderer",
                        "mesh": "plane",
                        "material": "green"
                    },
                    {
                        "type": "Collider",
                        "size": [10, 10, 1]
                    }
                ]
            },
            {
                "position": [0, 1, 0],
                "scale": [1, 1, 1],
                "components": [
                    {
                        "type": "Mesh Renderer",
                        "mesh": "cube",
                        "material": "red"
                    },
                    {
                        "type": "Collider",
                        "size": [1, 1, 1]
                    }
                ]
            }
        ]
    }
}
```

## Step 4: Integrate with PlayState
Edit `source/states/play-state.hpp` and integrate CollisionSystem:

```cpp
#include <systems/collision-system.hpp>

class Playstate: public our::State {
    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::CollisionSystem collisionSystem;  // ADD THIS
    
    void onDraw(double deltaTime) override {
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        collisionSystem.update(&world);  // ADD THIS
        renderer.render(&world);
    }
};
```

## Step 5: Test Collisions
```bash
cd /home/gehad/GFX-Project/bin
./GAME_APPLICATION -c ../config/test-collision.jsonc
```

You should see a floor (green plane) and a cube in the middle. Both should have colliders.

## Step 6: Design the Full Game Arena
Update `config/game-phase2.jsonc` with the full game environment:

**Key Areas to Design:**
- [ ] Open arena floor (50m x 50m approx)
- [ ] 4 perimeter walls (north, south, east, west)
- [ ] 2-3 large obstacles in the middle (cover for players)
- [ ] Spawn points for players and zombies
- [ ] High points/sniper positions (optional)
- [ ] Narrow corridors (optional)

**Example layout:**
```
    +===============+
    |               |
    | O     O     O |  O = Obstacle
    |               |
    | P           Z |  P = Player spawn
    |               |  Z = Zombie spawn
    +===============+
```

## Step 7: Communicate with Team Members

### For Member 1 (Player):
- Ask: "Where is the player entity defined? I need to add a collider to it."
- Provide: Player AABB size/half-size for designing obstacles correctly

### For Member 2 (Shooting):
- Provide: Raycasting method signature for hit detection
- Explain: How to query collisions after shooting

### For Member 3 (Zombies):
- Provide: SceneManager guide on how to create zombie entities with colliders
- Explain: How to query wall collisions for AI pathfinding

## Step 8: Optimize & Polish
- [ ] Add more obstacles and details to arena
- [ ] Test collision performance (100+ entities?)
- [ ] Add visual debug visualization (optional - draw collider bounds)
- [ ] Fine-tune collision boundaries

## Files to Edit Now
1. ✅ `source/common/components/collider.hpp` - CREATED
2. ✅ `source/common/components/collider.cpp` - CREATED
3. ✅ `source/common/components/environment.hpp` - CREATED
4. ✅ `source/common/components/environment.cpp` - CREATED
5. ✅ `source/common/systems/collision-system.hpp` - CREATED
6. ✅ `source/common/systems/collision-system.cpp` - CREATED
7. ✅ `source/common/systems/scene-manager.hpp` - CREATED
8. ✅ `source/common/systems/scene-manager.cpp` - CREATED
9. ✅ `source/common/components/component-deserializer.hpp` - UPDATED
10. ✅ `CMakeLists.txt` - UPDATED
11. ❓ `source/states/play-state.hpp` - INTEGRATE (see Step 4)
12. ❌ `config/game-phase2.jsonc` - CREATE (see Step 3)

## Debugging Tips

**Compile errors?**
- Check #include paths are correct
- Make sure all classes are forward declared
- Verify CMakeLists.txt includes all new files

**Runtime crashes?**
- Use nullptr checks everywhere
- Print debug info: `std::cout << "Colliding: " << ...`
- Check entity component existence: `if (entity->getComponent<ColliderComponent>()) {...}`

**Collisions not detected?**
- Verify both entities have ColliderComponent
- Check that halfSize values are not zero
- Verify entity transforms are being updated

**Performance issues?**
- Collision detection is O(n²) - optimize with spatial partitioning if n > 1000
- Consider baking static colliders (terrain) separately

## Next Phase Goals
After completing the checklist above, prioritize:
1. Player-wall collision
2. Player-zombie collision
3. Weapon raycasting
4. Game over condition (all players dead or all zombies dead)
