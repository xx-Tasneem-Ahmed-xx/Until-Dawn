#pragma once

#include "../ecs/world.hpp"
#include "../components/camera.hpp"
#include "../components/zombie.hpp"
#include "../components/health.hpp"
#include "../components/weapon.hpp"
#include <glm/glm.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>

namespace our
{

    // A simple struct to hold ray data
    struct Ray
    {
        glm::vec3 origin;    // Starting point of the ray (camera position)
        glm::vec3 direction; // Direction of the ray (normalized camera forward)
    };

    // A struct to hold hit information
    struct RayHit
    {
        Entity *entity = nullptr; // The entity that was hit (nullptr if no hit)
        float distance = -1.0f;   // Distance from ray origin to hit point (-1 if no hit)
    };

    struct FireResult
    {
        bool hitZombie = false;
        bool killedZombie = false;
        Entity *hitEntity = nullptr;
        float hitDistance = -1.0f;
    };

    // The shooting system is responsible for building rays from the camera
    // and handling hit detection and damage calculations
    class ShootingSystem
    {
    public:
        // Build a ray from the camera's position and direction
        // Returns a Ray struct with origin and direction in world space
        Ray buildRayFromCamera(World *world)
        {
            Ray ray;
            ray.origin = glm::vec3(0.0f);
            ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);

            // Find the entity with a camera component
            for (auto entity : world->getEntities())
            {
                CameraComponent *camera = entity->getComponent<CameraComponent>();
                if (camera)
                {
                    // Get the local-to-world transformation matrix of the camera entity
                    glm::mat4 worldMatrix = entity->getLocalToWorldMatrix();

                    // Ray origin is the camera position in world space
                    // Transform the local origin (0,0,0) by the world matrix
                    ray.origin = glm::vec3(worldMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

                    // Ray direction is the camera forward direction (negative Z axis in camera space)
                    // Extract the forward direction from the world matrix
                    // In camera space, forward is -Z, so we extract column 2 and negate it
                    glm::vec3 forward = glm::vec3(worldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
                    ray.direction = glm::normalize(forward);

                    // Only use the first camera found
                    break;
                }
            }

            return ray;
        }

        // Test if a ray intersects with a sphere using analytic ray-sphere intersection
        // Returns the distance from ray origin to the hit point, or -1 if no hit
        float testRaySphereIntersection(const Ray &ray, glm::vec3 sphereCenter, float sphereRadius, const WeaponComponent *weapon)
        {
            // Vector from ray origin to sphere center
            glm::vec3 L = sphereCenter - ray.origin;

            // Quadratic formula coefficients: at^2 + bt + c = 0
            float a = glm::dot(ray.direction, ray.direction); // Should be 1 if direction is normalized
            float b = -2.0f * glm::dot(L, ray.direction);
            float c = glm::dot(L, L) - (sphereRadius * sphereRadius);

            // Compute discriminant
            float discriminant = b * b - 4.0f * a * c;

            // No intersection if discriminant is negative
            if (discriminant < 0.0f)
            {
                return -1.0f;
            }

            // Compute the two intersection points
            float sqrtDiscriminant = glm::sqrt(discriminant);
            float t1 = (-b - sqrtDiscriminant) / (2.0f * a);
            float t2 = (-b + sqrtDiscriminant) / (2.0f * a);

            // Return the closest positive intersection distance within weapon's fire range
            if (t1 >= 0.0f && t1 <= weapon->maxRange)
            {
                return t1;
            }
            else if (t2 >= 0.0f)
            {
                return t2;
            }

            // Both intersections are behind the ray origin
            return -1.0f;
        }

        // Test the ray against all entities with ZombieComponent + HealthComponent
        // Returns the closest hit
        RayHit castRayAgainstZombies(const Ray &ray, World *world, WeaponComponent *weapon)
        {
            RayHit closestHit;
            closestHit.distance = std::numeric_limits<float>::max();

            // Test each entity in the world
            for (auto entity : world->getEntities())
            {
                // Check if entity has both Zombie and Health components
                ZombieComponent *zombie = entity->getComponent<ZombieComponent>();
                HealthComponent *health = entity->getComponent<HealthComponent>();

                if (zombie && health && health->isAlive)
                {
                    float hitDistance = computeZombieHitDistance(ray, entity, zombie, weapon);

                    // If hit and closer than previous closest, update closest hit
                    if (hitDistance >= 0.0f && hitDistance < closestHit.distance)
                    {
                        closestHit.entity = entity;
                        closestHit.distance = hitDistance;
                    }
                }
            }

            // If no hit was found, reset distance to -1
            if (closestHit.distance == std::numeric_limits<float>::max())
            {
                closestHit.distance = -1.0f;
            }

            return closestHit;
        }

        // Fire a ray and damage the closest zombie it hits
        // Collects all intersections with t > 0, sorts by distance, and applies damage to the closest
        FireResult fireRay(const Ray &ray, World *world, WeaponComponent *weapon)
        {
            FireResult result;
            if (!weapon)
                return result;

            // Print ray information: Camera position and direction
            std::cout << "Ray fired: (" << ray.origin.x << ", " << ray.origin.y << ", " << ray.origin.z << ")"
                      << " Direction: (" << ray.direction.x << ", " << ray.direction.y << ", " << ray.direction.z << ")" << std::endl;

            // Collect all hits with t > 0
            std::vector<RayHit> hits;

            for (auto entity : world->getEntities())
            {
                ZombieComponent *zombie = entity->getComponent<ZombieComponent>();
                HealthComponent *health = entity->getComponent<HealthComponent>();

                if (zombie && health && health->isAlive)
                {
                    float hitDistance = computeZombieHitDistance(ray, entity, zombie, weapon);

                    // Collect only positive hits (in front of camera)
                    if (hitDistance > 0.0f)
                    {
                        RayHit hit;
                        hit.entity = entity;
                        hit.distance = hitDistance;
                        hits.push_back(hit);
                    }
                }
            }

            // Report hit detection results
            if (hits.empty())
            {
                std::cout << "No zombies hit" << std::endl;
                return result;
            }

            // Sort hits by distance (ascending order)
            std::sort(hits.begin(), hits.end(), [](const RayHit &a, const RayHit &b)
                      { return a.distance < b.distance; });

            // Apply damage to the closest hit only
            RayHit closestHit = hits[0];
            HealthComponent *targetHealth = closestHit.entity->getComponent<HealthComponent>();
            ZombieComponent *targetZombie = closestHit.entity->getComponent<ZombieComponent>();

            if (targetHealth)
            {
                if (targetZombie)
                {
                    result.hitZombie = true;
                    result.hitEntity = closestHit.entity;
                    result.hitDistance = closestHit.distance;

                    bool killed = targetZombie->registerShot();
                    if (killed)
                    {
                        result.killedZombie = true;
                        std::cout << "Zombie second shot: dead at distance " << closestHit.distance << std::endl;
                        targetHealth->takeDamage(targetHealth->currentHealth);
                    }
                    else
                    {
                        std::cout << "Zombie first shot: now crawling at distance " << closestHit.distance << std::endl;
                    }
                }
                else
                {
                    std::cout << "Hit detected at distance: " << closestHit.distance << " | Damage applied: " << weapon->damage << std::endl;
                    targetHealth->takeDamage(weapon->damage);
                }
            }

            return result;
        }

    private:
        float computeZombieHitDistance(const Ray &ray, Entity *entity, const ZombieComponent *zombie, const WeaponComponent *weapon)
        {
            glm::vec3 zombiePosition = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

            glm::vec3 scaleVec = entity->localTransform.scale;
            float maxScale = std::max({std::abs(scaleVec.x), std::abs(scaleVec.y), std::abs(scaleVec.z), 1.0f});
            float baseRadius = std::max(0.1f, zombie->radius * maxScale);

            float centerHeight = baseRadius * 0.9f;
            if (zombie->state == ZombieState::Crawling)
            {
                centerHeight = baseRadius * 0.40f;
            }
            else if (zombie->state == ZombieState::Dead)
            {
                centerHeight = baseRadius * 0.25f;
            }

            const std::array<glm::vec3, 3> hitCenters = {
                zombiePosition + glm::vec3(0.0f, centerHeight, 0.0f),
                zombiePosition + glm::vec3(0.0f, centerHeight + baseRadius * 0.70f, 0.0f),
                zombiePosition + glm::vec3(0.0f, centerHeight - baseRadius * 0.55f, 0.0f)};

            float hitDistance = -1.0f;
            const float hitRadius = baseRadius * 1.15f;
            for (const auto &center : hitCenters)
            {
                float candidate = testRaySphereIntersection(ray, center, hitRadius, weapon);
                if (candidate >= 0.0f && (hitDistance < 0.0f || candidate < hitDistance))
                {
                    hitDistance = candidate;
                }
            }

            return hitDistance;
        }
    };

}
