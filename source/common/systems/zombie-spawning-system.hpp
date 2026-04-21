#pragma once

#include "../components/health.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/zombie.hpp"
#include "../ecs/world.hpp"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
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
