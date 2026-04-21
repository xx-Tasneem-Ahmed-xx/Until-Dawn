#pragma once

#include "../asset-loader.hpp"
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

        Transform zombiePrototypeTransform{};
        Mesh *zombieMesh = nullptr;
        Material *zombieMaterial = nullptr;
    };

    class ZombieSpawningSystem
    {
    public:
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
            float &zombieModelYawOffsetDegrees,
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
            zombieModelYawOffsetDegrees = zombiesConfig.value("modelYawOffsetDegrees", zombieModelYawOffsetDegrees);
            zombieModelYawOffset = glm::radians(zombieModelYawOffsetDegrees);
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

            zombieGroundY = zombiePrototypeTransform.position.y + zombieSpawnHeightOffset;

            if (zombieSpawnPoints.empty())
            {
                zombieSpawnPoints.push_back(glm::vec3(0.0f, -0.5f, 2.0f));
                zombieSpawnPoints.push_back(glm::vec3(5.0f, -0.5f, 4.0f));
                zombieSpawnPoints.push_back(glm::vec3(-5.0f, -0.5f, 4.0f));
                zombieSpawnPoints.push_back(glm::vec3(0.0f, -0.5f, -2.0f));
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

        glm::vec3 pickSpawnPoint(const ZombieSpawnerConfig &config, const ZombieWaveRuntime &runtime, const glm::vec3 &playerPos, const glm::vec3 &forward, int spawnedIndex) const
        {
            int hash = spawnedIndex * 73 + static_cast<int>(runtime.currentWaveIndex) * 131 + 17;
            float tAngle = static_cast<float>(hash % 1000) / 999.0f;
            float tDist = static_cast<float>((hash * 37) % 1000) / 999.0f;

            float halfAngleRad = glm::radians(config.zombieSpawnViewHalfAngleDegrees);
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

            float maxSpawnDistance = std::max(config.minSpawnPlayerDistance + 0.1f, config.zombieSpawnMaxDistance);
            float distance = config.minSpawnPlayerDistance + (maxSpawnDistance - config.minSpawnPlayerDistance) * tDist;

            glm::vec3 spawn = playerPos + dir * distance;
            spawn.y = config.zombieGroundY;
            return spawn;
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
            zombieEntity->localTransform.position.y = config.zombieGroundY;
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
            zombie->shotsTaken = 0;
            zombie->radius = config.zombieRadius;
            zombie->speed = config.zombieWalkSpeed;
            zombie->crawlSpeed = config.zombieCrawlSpeed;
            zombie->damage = config.zombieDamage;
            zombie->attackRange = config.zombieAttackRange;
            zombie->attackCooldown = config.zombieAttackCooldown;
            zombie->corpseLifetime = config.zombieCorpseLifetime;
            zombie->baseY = config.zombieGroundY;

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
                spawnZombie(world, config, runtime, pickSpawnPoint(config, runtime, playerPos, forward, runtime.zombiesSpawnedThisWave), playerPos);
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
    };

}
