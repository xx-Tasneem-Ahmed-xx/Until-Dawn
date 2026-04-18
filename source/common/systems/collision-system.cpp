#include "collision-system.hpp"
#include "../components/collider.hpp"
#include "../ecs/entity.hpp"
#include <glm/glm.hpp>
#include <algorithm>

namespace our {

    void CollisionSystem::update(World* world) {
        // Store previous collisions
        previousCollisions = currentCollisions;
        currentCollisions.clear();

        const auto& entities = world->getEntities();
        std::vector<Entity*> collidables(entities.begin(), entities.end());

        // Check all pairs of entities for collisions
        for (size_t i = 0; i < collidables.size(); ++i) {
            Entity* entityA = collidables[i];
            auto colliderA = entityA->getComponent<ColliderComponent>();
            
            if (!colliderA) continue;

            for (size_t j = i + 1; j < collidables.size(); ++j) {
                Entity* entityB = collidables[j];
                auto colliderB = entityB->getComponent<ColliderComponent>();
                
                if (!colliderB) continue;

                // Check for collision
                if (colliderA->intersects(colliderB)) {
                    CollisionInfo info = {entityA, entityB, colliderA, colliderB};
                    currentCollisions.push_back(info);
                }
            }
        }
    }

    bool CollisionSystem::areColliding(Entity* entityA, Entity* entityB) const {
        for (const auto& collision : currentCollisions) {
            if ((collision.entityA == entityA && collision.entityB == entityB) ||
                (collision.entityA == entityB && collision.entityB == entityA)) {
                return true;
            }
        }
        return false;
    }

    std::vector<CollisionInfo> CollisionSystem::getCollisionsForEntity(Entity* entity) const {
        std::vector<CollisionInfo> result;
        for (const auto& collision : currentCollisions) {
            if (collision.entityA == entity || collision.entityB == entity) {
                result.push_back(collision);
            }
        }
        return result;
    }

    Entity* CollisionSystem::raycastPoint(World* world, const glm::vec3& point) const {
        if (!world) return nullptr;
        
        const auto& entities = world->getEntities();
        for (auto entity : entities) {
            auto collider = entity->getComponent<ColliderComponent>();
            if (collider && collider->contains(point)) {
                return entity;
            }
        }
        return nullptr;
    }

    std::vector<Entity*> CollisionSystem::raycast(World* world, const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const {
        std::vector<Entity*> result;
        // TODO: Implement proper raycasting with AABB
        // For now, return empty
        return result;
    }

    std::vector<CollisionInfo> CollisionSystem::getNewCollisions() const {
        std::vector<CollisionInfo> newCollisions;
        
        for (const auto& current : currentCollisions) {
            bool wasColliding = false;
            for (const auto& previous : previousCollisions) {
                if ((previous.entityA == current.entityA && previous.entityB == current.entityB) ||
                    (previous.entityA == current.entityB && previous.entityB == current.entityA)) {
                    wasColliding = true;
                    break;
                }
            }
            if (!wasColliding) {
                newCollisions.push_back(current);
            }
        }
        
        return newCollisions;
    }

    std::vector<CollisionInfo> CollisionSystem::getEndedCollisions() const {
        std::vector<CollisionInfo> endedCollisions;
        
        for (const auto& previous : previousCollisions) {
            bool stillColliding = false;
            for (const auto& current : currentCollisions) {
                if ((current.entityA == previous.entityA && current.entityB == previous.entityB) ||
                    (current.entityA == previous.entityB && current.entityB == previous.entityA)) {
                    stillColliding = true;
                    break;
                }
            }
            if (!stillColliding) {
                endedCollisions.push_back(previous);
            }
        }
        
        return endedCollisions;
    }

    // resolveAABB — returns the push-back vector to move entityA OUT of entityB.
    // Uses the minimum-penetration-depth axis so the separation is minimal and artefact-free.
    glm::vec3 CollisionSystem::resolveAABB(const CollisionInfo& info) const {
        glm::vec3 minA, maxA, minB, maxB;
        info.colliderA->getWorldBounds(minA, maxA);
        info.colliderB->getWorldBounds(minB, maxB);

        // Overlap on each axis
        float overlapX_pos = maxB.x - minA.x;  // push A in +X
        float overlapX_neg = maxA.x - minB.x;  // push A in -X
        float overlapY_pos = maxB.y - minA.y;
        float overlapY_neg = maxA.y - minB.y;
        float overlapZ_pos = maxB.z - minA.z;
        float overlapZ_neg = maxA.z - minB.z;

        // Minimum penetration per axis
        float px = (overlapX_pos < overlapX_neg) ?  overlapX_pos : -overlapX_neg;
        float py = (overlapY_pos < overlapY_neg) ?  overlapY_pos : -overlapY_neg;
        float pz = (overlapZ_pos < overlapZ_neg) ?  overlapZ_pos : -overlapZ_neg;

        float absPx = glm::abs(px);
        float absPy = glm::abs(py);
        float absPz = glm::abs(pz);

        // Resolve along axis with smallest penetration
        if (absPx <= absPy && absPx <= absPz) {
            return glm::vec3(px, 0.0f, 0.0f);
        } else if (absPy <= absPx && absPy <= absPz) {
            return glm::vec3(0.0f, py, 0.0f);
        } else {
            return glm::vec3(0.0f, 0.0f, pz);
        }
    }

}
