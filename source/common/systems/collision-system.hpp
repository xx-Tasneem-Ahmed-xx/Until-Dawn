#pragma once

#include "../ecs/world.hpp"
#include "../components/collider.hpp"
#include <functional>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    // Structure to hold collision information
    struct CollisionInfo {
        Entity* entityA;
        Entity* entityB;
        ColliderComponent* colliderA;
        ColliderComponent* colliderB;
    };

    // Collision system - handles collision detection between entities
    class CollisionSystem {
    private:
        std::vector<CollisionInfo> currentCollisions;
        std::vector<CollisionInfo> previousCollisions;

    public:
        // Callback type for collision events
        using CollisionCallback = std::function<void(CollisionInfo)>;

        // Update collision detection
        void update(World* world);

        // Get all current collisions
        const std::vector<CollisionInfo>& getCurrentCollisions() const {
            return currentCollisions;
        }

        // Check if two entities are colliding
        bool areColliding(Entity* entityA, Entity* entityB) const;

        // Get collisions involving a specific entity
        std::vector<CollisionInfo> getCollisionsForEntity(Entity* entity) const;

        // Check collision between a point and world colliders
        Entity* raycastPoint(World* world, const glm::vec3& point) const;

        // Cast a ray and find all entities it hits
        std::vector<Entity*> raycast(World* world, const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 1000.0f) const;

        // Get collisions that just started this frame
        std::vector<CollisionInfo> getNewCollisions() const;

        // Get collisions that just ended this frame
        std::vector<CollisionInfo> getEndedCollisions() const;

        // Clear all tracked collisions
        void clear() {
            currentCollisions.clear();
            previousCollisions.clear();
        }
    };

}
