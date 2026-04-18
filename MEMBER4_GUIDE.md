# Member 4 - World, Collision & Scene Setup Guide

## Overview
As Member 4, you're responsible for all environment, collision detection, and scene management. This foundational work enables the other team members to test their components safely.

## Components Created

### 1. **ColliderComponent** (`source/common/components/collider.hpp/.cpp`)
```cpp
// Defines collision boundaries for entities
class ColliderComponent : public Component {
    glm::vec3 center;        // Offset from entity position
    glm::vec3 halfSize;      // Half extents of AABB
    bool isTrigger;          // Non-physical collision
};
```

**Key Methods:**
- `intersects(other)` - AABB collision check with another collider
- `contains(point)` - Point-in-AABB test
- `getWorldBounds(min, max)` - Get world-space AABB bounds

**Usage in JSON:**
```json
{
    "type": "Collider",
    "size": [2, 2, 2],      // Full size (gets halved internally)
    "center": [0, 0, 0],    // Optional offset
    "isTrigger": false      // Optional
}
```

### 2. **EnvironmentComponent** (`source/common/components/environment.hpp/.cpp`)
```cpp
// Labels entities as environment pieces
class EnvironmentComponent : public Component {
    std::string environmentType;  // "wall", "floor", "prop", "obstacle"
};
```

**Usage in JSON:**
```json
{
    "type": "Environment",
    "type": "wall"    // Type of environment piece
}
```

### 3. **CollisionSystem** (`source/common/systems/collision-system.hpp/.cpp`)
Handles all collision detection and queries.

**Key Methods:**
- `update(world)` - Run every frame to detect collisions
- `areColliding(entityA, entityB)` - Check if two entities collide
- `getCollisionsForEntity(entity)` - Get all collisions involving an entity
- `getNewCollisions()` - Collisions that just started this frame
- `getEndedCollisions()` - Collisions that just ended this frame
- `raycastPoint(position)` - Find entity at a position

## Scene Setup

### Creating Scene Entities

Use **SceneManager** for easy entity creation:

```cpp
// Create floor
Entity* floor = SceneManager::createFloor(world, position, scale, "plane", "grass");

// Create wall
Entity* wall = SceneManager::createWall(world, position, scale, "cube", "wood");

// Create prop (static object)
Entity* prop = SceneManager::createProp(world, position, scale, "cube", "metal");

// Create invisible collision box
Entity* collBox = SceneManager::createCollisionBox(world, position, halfSize);

// Add components to existing entity
SceneManager::addCollider(entity, halfSize);
SceneManager::addEnvironmentComponent(entity, "wall");
```

### Loading Scenes from JSON

The game already supports loading complete scenes from JSON config files (like `config/game-phase2.jsonc`).

Each entity has:
- **position** - World position
- **rotation** - Euler angles (x: pitch, y: yaw, z: roll)
- **scale** - Scale factors
- **components** - Array of components to add
- **children** - Optional array of child entities (hierarchical)

**Example:**
```json
{
    "position": [0, 1, 0],
    "scale": [5, 5, 5],
    "components": [
        {
            "type": "Mesh Renderer",
            "mesh": "cube",
            "material": "wood"
        },
        {
            "type": "Collider",
            "size": [5, 5, 5]
        },
        {
            "type": "Environment",
            "type": "wall"
        }
    ]
}
```

## TODO: Remaining Tasks for Phase 2

### High Priority
1. **[ ] Integrate CollisionSystem into PlayState**
   - Add collision system to PlayState
   - Call `collisionSystem.update(world)` in `onDraw()`
   - Use collisions for game logic

2. **[ ] Test collision detection**
   - Create simple test scene
   - Verify AABB collisions are detected correctly
   - Check collision callbacks work

3. **[ ] Create full game arena**
   - Design playable area boundaries
   - Place obstacles and cover points
   - Ensure all walls have colliders

4. **[ ] Player collision integration**
   - Add collider to player entity (work with Member 1)
   - Prevent player from walking through walls
   - Handle wall collision responses

5. **[ ] Zombie collision integration**
   - Zombies need colliders (coordinate with Member 3)
   - Zombies block each other
   - Player can collide with zombies

### Medium Priority
6. **[ ] Implement raycasting**
   - Complete `raycast()` method in CollisionSystem
   - Used for weapon hit detection (Member 2)
   - Ray-AABB intersection

7. **[ ] Advanced collision responses**
   - Simple separation/pushback to prevent overlap
   - Friction/sliding along walls
   - Damage on collision for spikes/hazards

8. **[ ] Lighting setup**
   - Position lights in the arena
   - Place light sources (torches, overhead)
   - Ensure visibility of all game elements

9. **[ ] Post-processing effects**
   - Configure vignette or other postprocess effects
   - Optional: blood splatter effects on hit

### Low Priority (Polish)
10. **[ ] Audio collision events** (future)
11. **[ ] Particle effect spawning at collision points** (future)
12. **[ ] Environmental hazards** (spikes, lava, etc.)

## Integration Checklist

### 1. Update PlayState
```cpp
// In play-state.hpp
#include <systems/collision-system.hpp>

class Playstate: public our::State {
    our::CollisionSystem collisionSystem;  // ADD THIS
    
    void onDraw(double deltaTime) override {
        // ... existing code ...
        
        // UPDATE COLLISIONS
        collisionSystem.update(&world);
        
        // Handle collisions
        auto newCollisions = collisionSystem.getNewCollisions();
        for (const auto& collision : newCollisions) {
            // TODO: Handle new collision
        }
    }
};
```

### 2. Register Components
✅ **Already Done** - Updated component-deserializer.hpp to include:
- ColliderComponent
- EnvironmentComponent

### 3. Compile and Test
```bash
# Build the project
cd /home/gehad/GFX-Project
mkdir -p build && cd build
cmake ..
make
```

## Communication with Team Members

### Member 1 (Player Controller)
- Provide player entity with collider
- Receive player position (used for camera)
- Tell them wall collision values/behavior

### Member 2 (Shooting & Combat)
- Provide raycasting functionality for hit detection
- Receive hit entity information
- Report damage to health system

### Member 3 (Zombie AI)
- Provide zombie entity collider
- Get zombie positions for rendering
- Handle zombie-wall collisions

## File Structure
```
source/common/
├── components/
│   ├── collider.hpp          ← NEW
│   ├── collider.cpp          ← NEW
│   ├── environment.hpp       ← NEW
│   ├── environment.cpp       ← NEW
│   └── component-deserializer.hpp (UPDATED)
│
└── systems/
    ├── collision-system.hpp   ← NEW
    ├── collision-system.cpp   ← NEW
    ├── scene-manager.hpp      ← NEW
    └── scene-manager.cpp      ← NEW

config/
└── game-phase2.jsonc         ← NEW (example game scene)
```

## Testing Workflow

1. **Create a simple test scene** with a few collision boxes
2. **Run with member 1** to test player-wall collisions
3. **Add zombies** and test entity-entity collisions
4. **Implement raycasting** and test with member 2's weapons
5. **Scale up** to full arena with all obstacles

## Notes
- AABB collisions are good for boxes but may not work perfectly for complex shapes. Consider improving with sphere colliders or convex hulls for Phase 3.
- Current implementation detects collisions every frame; optimize with spatial partitioning if needed.
- Store collision results for inter-system communication between members.

## Quick Reference Commands

```bash
# Edit game config
nano config/game-phase2.jsonc

# Run game with custom config
./bin/GAME_APPLICATION -c config/game-phase2.jsonc

# Run for fixed frames (testing)
./bin/GAME_APPLICATION -c config/game-phase2.jsonc -f 60
```
