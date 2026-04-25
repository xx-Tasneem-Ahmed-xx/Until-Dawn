#pragma once

#include "../asset-loader.hpp"
#include "../components/collider.hpp"
#include "../components/environment.hpp"
#include "../components/health.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/zombie.hpp"
#include "../ecs/world.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <json/json.hpp>
#include <limits>
#include <string>
#include <vector>

namespace our
{

    struct ZombieWaveRuntime
    {
        size_t currentWaveIndex = 0;
        int zombiesSpawnedThisWave = 0;
        float zombieSpawnTimer = 0.0f;
        bool waitingForNextWave = true;
        float betweenWaveTimer = 0.0f;
        bool allWavesCompleted = false;
    };

    struct ZombieSpawnerConfig
    {
        std::vector<int> waveZombieCounts;

        float zombieSpawnIntervalSeconds = 0.8f;
        float betweenWavesDelaySeconds = 4.0f;
        float minSpawnPlayerDistance = 7.0f;
        float zombieSpawnMaxDistance = 22.0f;
        float zombieSpawnViewHalfAngleDegrees = 24.0f;

        float zombieWalkSpeed = 1.8f;
        float zombieCrawlSpeed = 0.8f;
        float zombieDamage = 8.0f;
        float zombieAttackRange = 1.8f;
        float zombieAttackCooldown = 1.0f;
        float zombieCorpseLifetime = 1.25f;
        float zombieRadius = 1.0f;

        float zombieGroundY = -0.5f;
        float zombieModelYawOffset = 0.0f;
        float zombieModelScaleMultiplier = 1.0f;
        std::vector<glm::vec3> zombieSpawnPoints;

        Transform zombiePrototypeTransform{};
        Mesh *zombieMesh = nullptr;
        Material *zombieMaterial = nullptr;
    };

    class ZombieSpawningSystem
    {
        float sampleFloorYAt(World *world, const glm::vec3 &positionXZ, float fallbackY) const
        {
            if (!world)
                return fallbackY;

            bool found = false;
            float bestY = -std::numeric_limits<float>::infinity();

            for (auto entity : world->getEntities())
            {
                auto *env = entity->getComponent<EnvironmentComponent>();
                if (!(env && env->environmentType == "floor"))
                    continue;

                if (auto *collider = entity->getComponent<ColliderComponent>())
                {
                    glm::vec3 minB, maxB;
                    collider->getWorldBounds(minB, maxB);
                    if (positionXZ.x >= minB.x && positionXZ.x <= maxB.x &&
                        positionXZ.z >= minB.z && positionXZ.z <= maxB.z)
                    {
                        found = true;
                        bestY = std::max(bestY, maxB.y);
                    }
                }
                else
                {
                    glm::vec3 p = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                    found = true;
                    bestY = std::max(bestY, p.y);
                }
            }

            return found ? bestY : fallbackY;
        }

        float sampleGlobalFloorY(World *world, float fallbackY) const
        {
            if (!world)
                return fallbackY;

            bool found = false;
            float bestY = -std::numeric_limits<float>::infinity();
            for (auto entity : world->getEntities())
            {
                auto *env = entity->getComponent<EnvironmentComponent>();
                if (!(env && env->environmentType == "floor"))
                    continue;

                if (auto *collider = entity->getComponent<ColliderComponent>())
                {
                    glm::vec3 minB, maxB;
                    collider->getWorldBounds(minB, maxB);
                    found = true;
                    bestY = std::max(bestY, maxB.y);
                }
                else
                {
                    glm::vec3 p = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                    found = true;
                    bestY = std::max(bestY, p.y);
                }
            }

            return found ? bestY : fallbackY;
        }

        std::vector<glm::vec3> buildFallbackSpawnPoints(World *world, float fallbackY) const
        {
            std::vector<glm::vec3> points;

            for (auto entity : world->getEntities())
            {
                auto *env = entity->getComponent<EnvironmentComponent>();
                if (!(env && env->environmentType == "floor"))
                    continue;

                auto *collider = entity->getComponent<ColliderComponent>();
                if (!collider)
                    continue;

                glm::vec3 minB, maxB;
                collider->getWorldBounds(minB, maxB);
                float y = maxB.y;

                float marginX = std::max(4.0f, (maxB.x - minB.x) * 0.15f);
                float marginZ = std::max(4.0f, (maxB.z - minB.z) * 0.15f);

                points.push_back(glm::vec3(minB.x + marginX, y, minB.z + marginZ));
                points.push_back(glm::vec3(minB.x + marginX, y, maxB.z - marginZ));
                points.push_back(glm::vec3(maxB.x - marginX, y, minB.z + marginZ));
                points.push_back(glm::vec3(maxB.x - marginX, y, maxB.z - marginZ));
                points.push_back(glm::vec3((minB.x + maxB.x) * 0.5f, y, minB.z + marginZ));
                points.push_back(glm::vec3((minB.x + maxB.x) * 0.5f, y, maxB.z - marginZ));
                points.push_back(glm::vec3(minB.x + marginX, y, (minB.z + maxB.z) * 0.5f));
                points.push_back(glm::vec3(maxB.x - marginX, y, (minB.z + maxB.z) * 0.5f));
            }

            if (points.empty())
            {
                points.push_back(glm::vec3(0.0f, fallbackY, 8.0f));
                points.push_back(glm::vec3(8.0f, fallbackY, 0.0f));
                points.push_back(glm::vec3(-8.0f, fallbackY, 0.0f));
                points.push_back(glm::vec3(0.0f, fallbackY, -8.0f));
            }

            return points;
        }

        bool isCandidateInViewCone(const ZombieSpawnerConfig &config, const glm::vec3 &playerPos, const glm::vec3 &forward, const glm::vec3 &candidate) const
        {
            glm::vec3 toCandidate = candidate - playerPos;
            toCandidate.y = 0.0f;
            float lenSq = glm::dot(toCandidate, toCandidate);
            if (lenSq <= 0.0001f)
                return false;

            glm::vec3 dir = glm::normalize(toCandidate);
            float cosine = glm::dot(dir, forward);
            float minCosine = std::cos(glm::radians(config.zombieSpawnViewHalfAngleDegrees));
            return cosine >= minCosine;
        }

        bool isCandidateInDistanceBand(const ZombieSpawnerConfig &config, const glm::vec3 &playerPos, const glm::vec3 &candidate) const
        {
            glm::vec3 d = candidate - playerPos;
            d.y = 0.0f;
            float distance = glm::length(d);
            float maxSpawnDistance = std::max(config.minSpawnPlayerDistance + 0.1f, config.zombieSpawnMaxDistance);
            return distance >= config.minSpawnPlayerDistance && distance <= maxSpawnDistance;
        }

        bool isBlockedByStaticGeometry(World *world, const ZombieSpawnerConfig &config, const glm::vec3 &candidate) const
        {
            if (!world)
                return false;

            const float radius = std::max(0.1f, config.zombieRadius);
            for (auto entity : world->getEntities())
            {
                auto *env = entity->getComponent<EnvironmentComponent>();
                if (!env)
                    continue;

                const std::string &type = env->environmentType;
                bool isBlocking = (type == "wall" || type == "prop" || type == "obstacle" || type == "building");
                if (!isBlocking)
                    continue;

                if (auto *collider = entity->getComponent<ColliderComponent>())
                {
                    glm::vec3 minB, maxB;
                    collider->getWorldBounds(minB, maxB);
                    if (candidate.x >= (minB.x - radius) && candidate.x <= (maxB.x + radius) &&
                        candidate.z >= (minB.z - radius) && candidate.z <= (maxB.z + radius))
                    {
                        return true;
                    }
                    continue;
                }

                glm::vec3 center = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                glm::vec2 deltaXZ(candidate.x - center.x, candidate.z - center.z);
                float approxHalfExtent = std::max(std::abs(entity->localTransform.scale.x), std::abs(entity->localTransform.scale.z));
                approxHalfExtent = std::max(0.5f, approxHalfExtent);
                float safeRadius = approxHalfExtent + radius;
                if (glm::dot(deltaXZ, deltaXZ) <= safeRadius * safeRadius)
                {
                    return true;
                }
            }

            return false;
        }

        bool isValidSpawnCandidate(
            World *world,
            const ZombieSpawnerConfig &config,
            const glm::vec3 &playerPos,
            const glm::vec3 &forward,
            const glm::vec3 &candidate) const
        {
            if (!isCandidateInDistanceBand(config, playerPos, candidate))
                return false;
            if (!isCandidateInViewCone(config, playerPos, forward, candidate))
                return false;
            if (isBlockedByStaticGeometry(world, config, candidate))
                return false;
            return true;
        }

    public:
        void resetRuntime(ZombieWaveRuntime &runtime, float initialWaveDelaySeconds) const
        {
            runtime = ZombieWaveRuntime{};
            runtime.waitingForNextWave = true;
            runtime.betweenWaveTimer = std::max(0.0f, initialWaveDelaySeconds);
        }

        int getTotalPlannedZombieCount(const ZombieSpawnerConfig &config) const
        {
            int total = 0;
            for (int waveCount : config.waveZombieCounts)
            {
                if (waveCount > 0)
                    total += waveCount;
            }
            return total;
        }

        void initializeFromWorld(
            World *world,
            Mesh *&zombieMesh,
            Material *&zombieMaterial,
            Transform &zombiePrototypeTransform,
            std::vector<glm::vec3> &zombieSpawnPoints,
            float zombieSpawnHeightOffset,
            float &zombieGroundY) const
        {
            cachePrototypeAndSpawnPoints(
                world,
                zombieMesh,
                zombieMaterial,
                zombiePrototypeTransform,
                zombieSpawnPoints,
                zombieSpawnHeightOffset,
                zombieGroundY);
        }

        void initializeFromWorld(
            World *world,
            ZombieSpawnerConfig &config,
            float zombieSpawnHeightOffset) const
        {
            initializeFromWorld(
                world,
                config.zombieMesh,
                config.zombieMaterial,
                config.zombiePrototypeTransform,
                config.zombieSpawnPoints,
                zombieSpawnHeightOffset,
                config.zombieGroundY);
        }

        void loadGameplayConfig(
            const nlohmann::json &zombiesConfig,
            std::vector<int> &waveZombieCounts,
            float &zombieSpawnIntervalSeconds,
            float &initialWaveDelaySeconds,
            float &betweenWavesDelaySeconds,
            float &minSpawnPlayerDistance,
            float &zombieWalkSpeed,
            float &zombieCrawlSpeed,
            float &zombieDamage,
            float &zombieAttackRange,
            float &zombieAttackCooldown,
            float &zombieCorpseLifetime,
            float &zombieRadius,
            float &zombieSpawnHeightOffset,
            float &zombieModelScaleMultiplier,
            float &zombieSpawnMaxDistance,
            float &zombieSpawnViewHalfAngleDegrees,
            float &zombieModelYawOffset) const
        {
            if (!zombiesConfig.is_object())
                return;

            if (zombiesConfig.contains("waveZombieCounts") && zombiesConfig["waveZombieCounts"].is_array())
            {
                std::vector<int> parsedCounts;
                for (const auto &v : zombiesConfig["waveZombieCounts"])
                {
                    if (!v.is_number_integer())
                        continue;
                    int c = v.get<int>();
                    if (c > 0)
                        parsedCounts.push_back(c);
                }
                if (!parsedCounts.empty())
                {
                    waveZombieCounts = parsedCounts;
                }
            }

            zombieSpawnIntervalSeconds = std::max(0.01f, zombiesConfig.value("spawnIntervalSeconds", zombieSpawnIntervalSeconds));
            initialWaveDelaySeconds = std::max(0.0f, zombiesConfig.value("initialWaveDelaySeconds", initialWaveDelaySeconds));
            betweenWavesDelaySeconds = std::max(0.0f, zombiesConfig.value("betweenWavesDelaySeconds", betweenWavesDelaySeconds));
            minSpawnPlayerDistance = std::max(0.0f, zombiesConfig.value("minSpawnPlayerDistance", minSpawnPlayerDistance));

            zombieWalkSpeed = std::max(0.0f, zombiesConfig.value("walkSpeed", zombieWalkSpeed));
            zombieCrawlSpeed = std::max(0.0f, zombiesConfig.value("crawlSpeed", zombieCrawlSpeed));
            zombieDamage = std::max(0.0f, zombiesConfig.value("damage", zombieDamage));
            zombieAttackRange = std::max(0.05f, zombiesConfig.value("attackRange", zombieAttackRange));
            zombieAttackCooldown = std::max(0.01f, zombiesConfig.value("attackCooldown", zombieAttackCooldown));
            zombieCorpseLifetime = std::max(0.0f, zombiesConfig.value("corpseLifetime", zombieCorpseLifetime));
            zombieRadius = std::max(0.05f, zombiesConfig.value("radius", zombieRadius));
            zombieSpawnHeightOffset = zombiesConfig.value("spawnHeightOffset", zombieSpawnHeightOffset);
            zombieModelScaleMultiplier = std::max(0.05f, zombiesConfig.value("modelScaleMultiplier", zombieModelScaleMultiplier));
            zombieSpawnMaxDistance = std::max(minSpawnPlayerDistance + 0.1f, zombiesConfig.value("spawnMaxDistance", zombieSpawnMaxDistance));
            zombieSpawnViewHalfAngleDegrees = std::clamp(zombiesConfig.value("spawnViewHalfAngleDegrees", zombieSpawnViewHalfAngleDegrees), 1.0f, 85.0f);
            float zombieModelYawOffsetDegrees = glm::degrees(zombieModelYawOffset);
            zombieModelYawOffsetDegrees = zombiesConfig.value("modelYawOffsetDegrees", zombieModelYawOffsetDegrees);
            zombieModelYawOffset = glm::radians(zombieModelYawOffsetDegrees);
        }

        void loadGameplayConfig(
            const nlohmann::json &zombiesConfig,
            ZombieSpawnerConfig &config,
            float &initialWaveDelaySeconds,
            float &zombieSpawnHeightOffset) const
        {
            loadGameplayConfig(
                zombiesConfig,
                config.waveZombieCounts,
                config.zombieSpawnIntervalSeconds,
                initialWaveDelaySeconds,
                config.betweenWavesDelaySeconds,
                config.minSpawnPlayerDistance,
                config.zombieWalkSpeed,
                config.zombieCrawlSpeed,
                config.zombieDamage,
                config.zombieAttackRange,
                config.zombieAttackCooldown,
                config.zombieCorpseLifetime,
                config.zombieRadius,
                zombieSpawnHeightOffset,
                config.zombieModelScaleMultiplier,
                config.zombieSpawnMaxDistance,
                config.zombieSpawnViewHalfAngleDegrees,
                config.zombieModelYawOffset);
        }

        void cachePrototypeAndSpawnPoints(
            World *world,
            Mesh *&zombieMesh,
            Material *&zombieMaterial,
            Transform &zombiePrototypeTransform,
            std::vector<glm::vec3> &zombieSpawnPoints,
            float zombieSpawnHeightOffset,
            float &zombieGroundY) const
        {
            zombieMesh = AssetLoader<Mesh>::get("zombie");
            zombieMaterial = AssetLoader<Material>::get("zombie_theme");
            if (!zombieMaterial)
                zombieMaterial = AssetLoader<Material>::get("auto");
            zombiePrototypeTransform = Transform{};
            zombiePrototypeTransform.position = glm::vec3(0.0f, -0.5f, 0.0f);

            std::vector<Entity *> startupZombieEntities;
            zombieSpawnPoints.clear();
            for (auto entity : world->getEntities())
            {
                auto *renderer = entity->getComponent<MeshRendererComponent>();
                if (!(renderer && renderer->mesh == zombieMesh))
                    continue;

                startupZombieEntities.push_back(entity);
                zombieSpawnPoints.push_back(entity->localTransform.position);
                zombiePrototypeTransform = entity->localTransform;
                if (renderer->material)
                    zombieMaterial = renderer->material;
            }

            for (auto entity : startupZombieEntities)
            {
                world->markForRemoval(entity);
            }
            world->deleteMarkedEntities();

            float sceneFloorY = sampleGlobalFloorY(world, zombiePrototypeTransform.position.y);
            zombieGroundY = sceneFloorY + zombieSpawnHeightOffset;

            if (!zombieSpawnPoints.empty())
            {
                for (auto &p : zombieSpawnPoints)
                {
                    p.y = sampleFloorYAt(world, p, sceneFloorY) + zombieSpawnHeightOffset;
                }
            }

            if (zombieSpawnPoints.empty())
            {
                zombieSpawnPoints = buildFallbackSpawnPoints(world, zombieGroundY);
            }

            std::cout << "[Zombies] mesh=" << (zombieMesh ? "loaded" : "missing")
                      << ", gltfBaseColorTexture=" << ((zombieMesh && zombieMesh->hasGLTFBaseColorTexture()) ? "yes" : "no")
                      << ", material=" << (zombieMaterial ? "loaded" : "missing") << "\n";
        }

        ZombieSpawnerConfig makeConfig(
            const std::vector<int> &waveZombieCounts,
            float zombieSpawnIntervalSeconds,
            float betweenWavesDelaySeconds,
            float minSpawnPlayerDistance,
            float zombieSpawnMaxDistance,
            float zombieSpawnViewHalfAngleDegrees,
            float zombieWalkSpeed,
            float zombieCrawlSpeed,
            float zombieDamage,
            float zombieAttackRange,
            float zombieAttackCooldown,
            float zombieCorpseLifetime,
            float zombieRadius,
            float zombieGroundY,
            float zombieModelYawOffset,
            float zombieModelScaleMultiplier,
            const Transform &zombiePrototypeTransform,
            const std::vector<glm::vec3> &zombieSpawnPoints,
            Mesh *zombieMesh,
            Material *zombieMaterial) const
        {
            ZombieSpawnerConfig config;
            config.waveZombieCounts = waveZombieCounts;
            config.zombieSpawnIntervalSeconds = zombieSpawnIntervalSeconds;
            config.betweenWavesDelaySeconds = betweenWavesDelaySeconds;
            config.minSpawnPlayerDistance = minSpawnPlayerDistance;
            config.zombieSpawnMaxDistance = zombieSpawnMaxDistance;
            config.zombieSpawnViewHalfAngleDegrees = zombieSpawnViewHalfAngleDegrees;

            config.zombieWalkSpeed = zombieWalkSpeed;
            config.zombieCrawlSpeed = zombieCrawlSpeed;
            config.zombieDamage = zombieDamage;
            config.zombieAttackRange = zombieAttackRange;
            config.zombieAttackCooldown = zombieAttackCooldown;
            config.zombieCorpseLifetime = zombieCorpseLifetime;
            config.zombieRadius = zombieRadius;

            config.zombieGroundY = zombieGroundY;
            config.zombieModelYawOffset = zombieModelYawOffset;
            config.zombieModelScaleMultiplier = zombieModelScaleMultiplier;
            config.zombiePrototypeTransform = zombiePrototypeTransform;
            config.zombieSpawnPoints = zombieSpawnPoints;
            config.zombieMesh = zombieMesh;
            config.zombieMaterial = zombieMaterial;

            return config;
        }

        int getAliveZombieCount(World *world) const
        {
            int alive = 0;
            for (auto entity : world->getEntities())
            {
                auto *zombie = entity->getComponent<ZombieComponent>();
                auto *health = entity->getComponent<HealthComponent>();
                if (zombie && health && health->isAlive)
                    alive++;
            }
            return alive;
        }

        void beginWave(ZombieWaveRuntime &runtime, size_t waveIndex) const
        {
            runtime.currentWaveIndex = waveIndex;
            runtime.zombiesSpawnedThisWave = 0;
            runtime.zombieSpawnTimer = 0.0f;
            runtime.waitingForNextWave = false;
        }

        glm::vec3 pickSpawnPoint(World *world, const ZombieSpawnerConfig &config, const ZombieWaveRuntime &runtime, const glm::vec3 &playerPos, const glm::vec3 &forward, int spawnedIndex) const
        {
            if (!config.zombieSpawnPoints.empty())
            {
                int start = (spawnedIndex * 17 + static_cast<int>(runtime.currentWaveIndex) * 31) % static_cast<int>(config.zombieSpawnPoints.size());
                for (size_t i = 0; i < config.zombieSpawnPoints.size(); ++i)
                {
                    const glm::vec3 &base = config.zombieSpawnPoints[(start + static_cast<int>(i)) % config.zombieSpawnPoints.size()];
                    glm::vec3 candidate = base;
                    candidate.y = sampleFloorYAt(world, candidate, config.zombieGroundY);
                    if (isValidSpawnCandidate(world, config, playerPos, forward, candidate))
                        return candidate;
                }
            }

            int hash = spawnedIndex * 73 + static_cast<int>(runtime.currentWaveIndex) * 131 + 17;
            float halfAngleRad = glm::radians(config.zombieSpawnViewHalfAngleDegrees);
            float maxSpawnDistance = std::max(config.minSpawnPlayerDistance + 0.1f, config.zombieSpawnMaxDistance);

            for (int attempt = 0; attempt < 24; ++attempt)
            {
                int seed = hash + attempt * 97;
                float tAngle = static_cast<float>((seed * 53) % 1000) / 999.0f;
                float tDist = static_cast<float>((seed * 37) % 1000) / 999.0f;

                float angle = (tAngle * 2.0f - 1.0f) * halfAngleRad;
                float c = std::cos(angle);
                float s = std::sin(angle);
                glm::vec3 dir;
                dir.x = forward.x * c - forward.z * s;
                dir.y = 0.0f;
                dir.z = forward.x * s + forward.z * c;
                if (glm::dot(dir, dir) < 0.0001f)
                    dir = forward;
                else
                    dir = glm::normalize(dir);

                float distance = config.minSpawnPlayerDistance + (maxSpawnDistance - config.minSpawnPlayerDistance) * tDist;
                glm::vec3 candidate = playerPos + dir * distance;
                candidate.y = sampleFloorYAt(world, candidate, config.zombieGroundY);
                if (isValidSpawnCandidate(world, config, playerPos, forward, candidate))
                    return candidate;
            }

            glm::vec3 fallback = playerPos + forward * maxSpawnDistance;
            fallback.y = sampleFloorYAt(world, fallback, config.zombieGroundY);
            return fallback;
        }

        void spawnZombie(World *world, const ZombieSpawnerConfig &config, const ZombieWaveRuntime &runtime, const glm::vec3 &spawnPosition, const glm::vec3 &playerPos) const
        {
            if (!config.zombieMesh)
                return;

            Entity *zombieEntity = world->add();
            zombieEntity->name = "WaveZombie_" + std::to_string(runtime.currentWaveIndex + 1) + "_" + std::to_string(runtime.zombiesSpawnedThisWave + 1);
            zombieEntity->parent = nullptr;
            zombieEntity->localTransform = config.zombiePrototypeTransform;
            zombieEntity->localTransform.position = spawnPosition;
            zombieEntity->localTransform.scale *= config.zombieModelScaleMultiplier;

            auto *renderer = zombieEntity->addComponent<MeshRendererComponent>();
            renderer->mesh = config.zombieMesh;
            renderer->material = config.zombieMaterial;

            auto *health = zombieEntity->addComponent<HealthComponent>();
            health->maxHealth = 2.0f;
            health->currentHealth = 2.0f;
            health->isAlive = true;

            auto *zombie = zombieEntity->addComponent<ZombieComponent>();
            zombie->state = ZombieState::Walking;
            zombie->health = zombie->maxHealth;
            zombie->radius = config.zombieRadius;
            zombie->speed = config.zombieWalkSpeed;
            zombie->crawlSpeed = config.zombieCrawlSpeed;
            zombie->damage = config.zombieDamage;
            zombie->attackRange = config.zombieAttackRange;
            zombie->attackCooldown = config.zombieAttackCooldown;
            zombie->corpseLifetime = config.zombieCorpseLifetime;
            zombie->baseY = spawnPosition.y;

            // Ensure zombies physically collide with environment colliders (walls/props/floor).
            // Use gameplay radius on XZ and a stable upright Y extent.
            auto *collider = zombieEntity->addComponent<ColliderComponent>();
            float modelScale = std::max(0.05f, config.zombieModelScaleMultiplier);
            float horizontalHalfExtent = std::max(0.15f, config.zombieRadius * modelScale);
            float verticalHalfExtent = std::max(0.5f, config.zombieRadius * modelScale * 1.2f);
            collider->halfSize = glm::vec3(horizontalHalfExtent, verticalHalfExtent, horizontalHalfExtent);
            collider->center = glm::vec3(0.0f, verticalHalfExtent, 0.0f);
            collider->isTrigger = false;

            if (config.zombieMesh->hasSkinning())
            {
                zombie->skinMatrices.assign(config.zombieMesh->getSkinJointNodes().size(), glm::mat4(1.0f));
            }

            glm::vec3 toPlayer = playerPos - zombieEntity->localTransform.position;
            toPlayer.y = 0.0f;
            if (glm::dot(toPlayer, toPlayer) > 0.0001f)
            {
                glm::vec3 direction = glm::normalize(toPlayer);
                float yaw = std::atan2(-direction.x, -direction.z);
                zombieEntity->localTransform.rotation.y = yaw + config.zombieModelYawOffset;
            }
        }

        void update(World *world, ZombieWaveRuntime &runtime, const ZombieSpawnerConfig &config, float deltaTime, const glm::vec3 &playerPos, const glm::vec3 &forward) const
        {
            if (runtime.allWavesCompleted)
                return;

            if (config.waveZombieCounts.empty())
            {
                runtime.allWavesCompleted = true;
                runtime.waitingForNextWave = false;
                return;
            }

            if (runtime.currentWaveIndex >= config.waveZombieCounts.size())
                runtime.currentWaveIndex = config.waveZombieCounts.size() - 1;

            if (runtime.waitingForNextWave)
            {
                runtime.betweenWaveTimer -= deltaTime;
                if (runtime.betweenWaveTimer <= 0.0f)
                {
                    beginWave(runtime, runtime.currentWaveIndex);
                }
                return;
            }

            int targetForWave = config.waveZombieCounts[runtime.currentWaveIndex];
            runtime.zombieSpawnTimer -= deltaTime;
            while (runtime.zombiesSpawnedThisWave < targetForWave && runtime.zombieSpawnTimer <= 0.0f)
            {
                spawnZombie(world, config, runtime, pickSpawnPoint(world, config, runtime, playerPos, forward, runtime.zombiesSpawnedThisWave), playerPos);
                runtime.zombiesSpawnedThisWave++;
                runtime.zombieSpawnTimer += config.zombieSpawnIntervalSeconds;
            }

            if (runtime.zombiesSpawnedThisWave >= targetForWave && getAliveZombieCount(world) == 0)
            {
                if (runtime.currentWaveIndex + 1 >= config.waveZombieCounts.size())
                {
                    runtime.allWavesCompleted = true;
                    runtime.waitingForNextWave = false;
                }
                else
                {
                    runtime.waitingForNextWave = true;
                    runtime.currentWaveIndex++;
                    runtime.betweenWaveTimer = config.betweenWavesDelaySeconds;
                }
            }
        }

        void updateInPlace(
            World *world,
            size_t &currentWaveIndex,
            int &zombiesSpawnedThisWave,
            float &zombieSpawnTimer,
            bool &waitingForNextWave,
            float &betweenWaveTimer,
            bool &allWavesCompleted,
            const ZombieSpawnerConfig &config,
            float deltaTime,
            const glm::vec3 &playerPos,
            const glm::vec3 &forward) const
        {
            ZombieWaveRuntime runtime;
            runtime.currentWaveIndex = currentWaveIndex;
            runtime.zombiesSpawnedThisWave = zombiesSpawnedThisWave;
            runtime.zombieSpawnTimer = zombieSpawnTimer;
            runtime.waitingForNextWave = waitingForNextWave;
            runtime.betweenWaveTimer = betweenWaveTimer;
            runtime.allWavesCompleted = allWavesCompleted;

            update(world, runtime, config, deltaTime, playerPos, forward);

            currentWaveIndex = runtime.currentWaveIndex;
            zombiesSpawnedThisWave = runtime.zombiesSpawnedThisWave;
            zombieSpawnTimer = runtime.zombieSpawnTimer;
            waitingForNextWave = runtime.waitingForNextWave;
            betweenWaveTimer = runtime.betweenWaveTimer;
            allWavesCompleted = runtime.allWavesCompleted;
        }
    };

}
