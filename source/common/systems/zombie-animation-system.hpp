#pragma once

#include "../animation/motion.hpp"
#include "../asset-loader.hpp"
#include "../components/health.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/zombie.hpp"
#include "../ecs/entity.hpp"
#include "../ecs/transform.hpp"
#include "../ecs/world.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <json/json.hpp>
#include <string>
#include <vector>

namespace our
{

    struct ZombieClipOverrides
    {
        std::string walk = "Armature|Walk";
        std::string attack = "Armature|Attack";
        std::string crawl = "Armature|Crawl";
        std::string die = "Armature|Bite_ground";
        std::string crawlDie = "Armature|Bite_ground";
    };

    struct ZombiePoseConfig
    {
        float walkBobAmplitude = 0.055f;
        float crawlBobAmplitude = 0.025f;
        float attackBobAmplitude = 0.085f;
        float walkBobFrequency = 7.0f;
        float crawlBobFrequency = 4.2f;
        float attackBobFrequency = 11.0f;
        float walkRollDegrees = 7.0f;
        float attackPitchDegrees = 13.0f;
        float attackPitchFrequency = 8.0f;
        float crawlPitchDegrees = 58.0f;
        float crawlHeightDrop = 0.22f;
        float modelScaleMultiplier = 1.0f;
    };

    struct ZombieAnimationConfig
    {
        float modelYawOffset = 0.0f;
        float deathFallDegrees = 82.0f;
        float deathSink = 0.30f;
        Transform prototypeTransform{};
        Material *bloodFallbackMaterial = nullptr;
        ZombiePoseConfig pose{};
    };

    struct BloodSplashFx
    {
        Entity *entity = nullptr;
        float timeLeft = 0.0f;
    };

    class ZombieAnimationSystem
    {
        Motion *zombieMotion = nullptr;
        const MotionClip *walkClip = nullptr;
        const MotionClip *attackClip = nullptr;
        const MotionClip *crawlClip = nullptr;
        const MotionClip *dieClip = nullptr;
        const MotionClip *crawlDieClip = nullptr;
        Mesh *bloodSplashMesh = nullptr;
        Material *bloodSplashMaterial = nullptr;
        float bloodSplashLifetimeSeconds = 1.1f;
        float bloodSplashScaleMultiplier = 8.0f;
        float bloodSplashHeightOffset = 0.9f;
        std::vector<BloodSplashFx> activeBloodSplashes;

        const MotionClip *firstAvailableClip() const
        {
            if (!(zombieMotion && !zombieMotion->clips.empty()))
                return nullptr;
            return &zombieMotion->clips[0];
        }

        const MotionClip *findClipByExactNameInsensitive(const std::string &clipName) const
        {
            if (!(zombieMotion && !clipName.empty()))
                return nullptr;

            auto toLower = [](std::string s)
            {
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                               { return static_cast<char>(std::tolower(c)); });
                return s;
            };

            std::string wanted = toLower(clipName);
            for (const auto &clip : zombieMotion->clips)
            {
                if (toLower(clip.name) == wanted)
                    return &clip;
            }
            return nullptr;
        }

    public:
        void initializeAssets(const std::string &motionAssetName = "zombie-motion", const ZombieClipOverrides &overrides = {})
        {
            bindMotionClips(motionAssetName, overrides);
            cacheBloodSplashAssets();
        }

        void loadGameplayConfig(
            const nlohmann::json &zombiesConfig,
            ZombiePoseConfig &poseConfig,
            float &deathFallDegrees,
            float &deathSink)
        {
            if (!zombiesConfig.is_object())
                return;

            poseConfig.walkBobAmplitude = std::max(0.0f, zombiesConfig.value("walkBobAmplitude", poseConfig.walkBobAmplitude));
            poseConfig.crawlBobAmplitude = std::max(0.0f, zombiesConfig.value("crawlBobAmplitude", poseConfig.crawlBobAmplitude));
            poseConfig.attackBobAmplitude = std::max(0.0f, zombiesConfig.value("attackBobAmplitude", poseConfig.attackBobAmplitude));
            poseConfig.walkBobFrequency = std::max(0.0f, zombiesConfig.value("walkBobFrequency", poseConfig.walkBobFrequency));
            poseConfig.crawlBobFrequency = std::max(0.0f, zombiesConfig.value("crawlBobFrequency", poseConfig.crawlBobFrequency));
            poseConfig.attackBobFrequency = std::max(0.0f, zombiesConfig.value("attackBobFrequency", poseConfig.attackBobFrequency));
            poseConfig.walkRollDegrees = std::max(0.0f, zombiesConfig.value("walkRollDegrees", poseConfig.walkRollDegrees));
            poseConfig.attackPitchDegrees = std::max(0.0f, zombiesConfig.value("attackPitchDegrees", poseConfig.attackPitchDegrees));
            poseConfig.attackPitchFrequency = std::max(0.0f, zombiesConfig.value("attackPitchFrequency", poseConfig.attackPitchFrequency));
            poseConfig.crawlPitchDegrees = std::max(0.0f, zombiesConfig.value("crawlPitchDegrees", poseConfig.crawlPitchDegrees));
            poseConfig.crawlHeightDrop = std::max(0.0f, zombiesConfig.value("crawlHeightDrop", poseConfig.crawlHeightDrop));

            deathFallDegrees = std::max(0.0f, zombiesConfig.value("deathFallDegrees", deathFallDegrees));
            deathSink = std::max(0.0f, zombiesConfig.value("deathSink", deathSink));

            bloodSplashLifetimeSeconds = std::max(0.05f, zombiesConfig.value("bloodSplashLifetimeSeconds", bloodSplashLifetimeSeconds));
            bloodSplashScaleMultiplier = std::max(0.05f, zombiesConfig.value("bloodSplashScaleMultiplier", bloodSplashScaleMultiplier));
            bloodSplashHeightOffset = zombiesConfig.value("bloodSplashHeightOffset", bloodSplashHeightOffset);
        }

        void loadGameplayConfig(
            const nlohmann::json &zombiesConfig,
            ZombieAnimationConfig &animationConfig)
        {
            loadGameplayConfig(
                zombiesConfig,
                animationConfig.pose,
                animationConfig.deathFallDegrees,
                animationConfig.deathSink);
        }

        void configureRuntime(
            ZombieAnimationConfig &animationConfig,
            float modelYawOffset,
            float modelScaleMultiplier,
            const Transform &prototypeTransform,
            Material *bloodFallbackMaterial) const
        {
            animationConfig.modelYawOffset = modelYawOffset;
            animationConfig.pose.modelScaleMultiplier = modelScaleMultiplier;
            animationConfig.prototypeTransform = prototypeTransform;
            animationConfig.bloodFallbackMaterial = bloodFallbackMaterial;
        }

        void resetEffects()
        {
            activeBloodSplashes.clear();
        }

        void bindMotionClips(const std::string &motionAssetName = "zombie-motion", const ZombieClipOverrides &overrides = {})
        {
            zombieMotion = AssetLoader<Motion>::get(motionAssetName);
            if (!zombieMotion)
            {
                std::cout << "[Motion] " << motionAssetName << " asset not found.\n";
                return;
            }

            walkClip = findClipByExactNameInsensitive(overrides.walk);
            attackClip = findClipByExactNameInsensitive(overrides.attack);
            crawlClip = findClipByExactNameInsensitive(overrides.crawl);
            dieClip = findClipByExactNameInsensitive(overrides.die);
            crawlDieClip = findClipByExactNameInsensitive(overrides.crawlDie);

            if (!walkClip)
                walkClip = firstAvailableClip();
            if (!attackClip)
                attackClip = walkClip;
            if (!crawlClip)
                crawlClip = walkClip;
            if (!dieClip)
                dieClip = walkClip;
            if (!crawlDieClip)
                crawlDieClip = dieClip;

            std::cout << "[Motion] Loaded " << zombieMotion->clips.size() << " clip(s) from " << zombieMotion->sourcePath << "\n";
            for (const auto &clip : zombieMotion->clips)
            {
                std::cout << "  - " << clip.name << " | duration=" << clip.duration << " | channels=" << clip.channelCount << "\n";
            }
            std::cout << "[Motion] Bound clips -> walk: " << (walkClip ? walkClip->name : "none")
                      << ", attack: " << (attackClip ? attackClip->name : "none")
                      << ", crawl: " << (crawlClip ? crawlClip->name : "none")
                      << ", die: " << (dieClip ? dieClip->name : "none")
                      << ", crawl-die: " << (crawlDieClip ? crawlDieClip->name : "none") << "\n";
        }

        void cacheBloodSplashAssets()
        {
            bloodSplashMesh = AssetLoader<Mesh>::get("blood-splash");
            if (!bloodSplashMesh)
                bloodSplashMesh = AssetLoader<Mesh>::get("blood");
            if (!bloodSplashMesh)
                bloodSplashMesh = AssetLoader<Mesh>::get("Blood");

            bloodSplashMaterial = AssetLoader<Material>::get("blood-fx");
            if (!bloodSplashMaterial)
                bloodSplashMaterial = AssetLoader<Material>::get("auto");

            if (bloodSplashMesh)
            {
                bloodSplashMesh->setGLTFBaseColorTexture(0);
                bloodSplashMesh->setGLTFBaseColorFactor(glm::vec4(1.0f));
            }

            std::cout << "[BloodFX] mesh=" << (bloodSplashMesh ? "loaded" : "missing")
                      << ", material=" << (bloodSplashMaterial ? "loaded" : "missing")
                      << ", lifetime=" << bloodSplashLifetimeSeconds
                      << ", scale=" << bloodSplashScaleMultiplier
                      << ", yOffset=" << bloodSplashHeightOffset << "\n";
        }

        bool spawnBloodSplashAt(
            World *world,
            const Transform &prototypeTransform,
            Material *fallbackMaterial,
            const glm::vec3 &position,
            float yaw,
            float sourceScale)
        {
            if (!bloodSplashMesh)
            {
                std::cout << "[BloodFX] spawn skipped: blood mesh not loaded\n";
                return false;
            }

            auto makeSplashEntity = [&](const glm::vec3 &rotation)
            {
                Entity *splash = world->add();
                splash->name = "BloodSplash";
                splash->parent = nullptr;
                splash->localTransform = prototypeTransform;
                splash->localTransform.position = position;
                splash->localTransform.position.y += bloodSplashHeightOffset;
                splash->localTransform.rotation = rotation;
                float finalScale = std::max(0.05f, sourceScale * bloodSplashScaleMultiplier);
                splash->localTransform.scale = glm::vec3(finalScale);

                auto *renderer = splash->addComponent<MeshRendererComponent>();
                renderer->mesh = bloodSplashMesh;
                renderer->material = bloodSplashMaterial ? bloodSplashMaterial : fallbackMaterial;

                activeBloodSplashes.push_back({splash, bloodSplashLifetimeSeconds});
            };

            makeSplashEntity(glm::vec3(0.0f, yaw, 0.0f));
            makeSplashEntity(glm::vec3(glm::half_pi<float>(), yaw, 0.0f));

            std::cout << "[BloodFX] spawned at ("
                      << position.x << ", " << position.y << ", " << position.z
                      << ") with scale=" << (sourceScale * bloodSplashScaleMultiplier) << "\n";
            return true;
        }

        void updateBloodSplashEffects(World *world, float deltaTime)
        {
            for (auto &fx : activeBloodSplashes)
            {
                fx.timeLeft -= deltaTime;
                if (fx.timeLeft <= 0.0f && fx.entity)
                {
                    world->markForRemoval(fx.entity);
                    fx.entity = nullptr;
                }
            }

            activeBloodSplashes.erase(
                std::remove_if(activeBloodSplashes.begin(), activeBloodSplashes.end(), [](const BloodSplashFx &fx)
                               { return fx.timeLeft <= 0.0f || fx.entity == nullptr; }),
                activeBloodSplashes.end());
        }

        const MotionClip *getMotionClipForState(const ZombieComponent *zombie) const
        {
            if (!zombie)
                return walkClip;

            ZombieState state = zombie->state;
            if (state == ZombieState::Attacking)
                return attackClip;
            if (state == ZombieState::Crawling)
                return crawlClip;
            if (state == ZombieState::Dead)
            {
                if (zombie->health <= 0.5 * zombie->maxHealth && zombie->health > 0 && crawlDieClip)
                    return crawlDieClip;
                return dieClip;
            }
            return walkClip;
        }

        void updateZombieAnimation(Entity *entity, ZombieComponent *zombie, float deltaTime) const
        {
            if (!zombie)
                return;

            const MotionClip *clip = getMotionClipForState(zombie);
            if (!clip)
                return;

            if (zombie->activeMotionClip != clip->name)
            {
                zombie->activeMotionClip = clip->name;
                zombie->motionClipTime = 0.0f;
            }
            else
            {
                zombie->motionClipTime += deltaTime;
            }

            if (clip->duration > 0.0001f)
            {
                if (zombie->state == ZombieState::Dead)
                    zombie->motionClipTime = std::min(zombie->motionClipTime, clip->duration);
                else
                    zombie->motionClipTime = std::fmod(zombie->motionClipTime, clip->duration);
            }

            if (!(entity && zombieMotion))
                return;

            auto *renderer = entity->getComponent<MeshRendererComponent>();
            if (!(renderer && renderer->mesh && renderer->mesh->hasSkinning()))
            {
                zombie->skinMatrices.clear();
                return;
            }

            if (!zombieMotion->computeSkinMatrices(
                    clip,
                    zombie->motionClipTime,
                    renderer->mesh->getSkinJointNodes(),
                    renderer->mesh->getInverseBindMatrices(),
                    zombie->skinMatrices))
            {
                zombie->skinMatrices.clear();
            }
        }

        void applyZombiePose(
            Entity *entity,
            ZombieComponent *zombie,
            const Transform &prototypeTransform,
            const ZombiePoseConfig &poseConfig) const
        {
            if (!(entity && zombie))
                return;

            const bool usingSkinnedAnimation = !zombie->skinMatrices.empty();
            if (usingSkinnedAnimation)
            {
                entity->localTransform.position.y = zombie->baseY;
                entity->localTransform.rotation.x = 0.0f;
                entity->localTransform.rotation.z = 0.0f;
                entity->localTransform.scale = prototypeTransform.scale * poseConfig.modelScaleMultiplier;
                return;
            }

            float bobAmplitude = poseConfig.walkBobAmplitude;
            float bobFrequency = poseConfig.walkBobFrequency;
            float posePitch = 0.0f;
            float poseRoll = 0.0f;
            float poseBaseY = zombie->baseY;
            float attackPitchFrequencyLocal = poseConfig.attackPitchFrequency;

            if (const MotionClip *active = getMotionClipForState(zombie); active && active->duration > 0.0001f)
            {
                float baseFrequency = glm::two_pi<float>() / active->duration;
                if (zombie->state == ZombieState::Attacking)
                {
                    bobFrequency = baseFrequency;
                    attackPitchFrequencyLocal = baseFrequency;
                }
                else if (zombie->state == ZombieState::Crawling || zombie->state == ZombieState::Walking)
                {
                    bobFrequency = baseFrequency;
                }
            }

            if (zombie->state == ZombieState::Crawling)
            {
                bobAmplitude = poseConfig.crawlBobAmplitude;
                bobFrequency = poseConfig.crawlBobFrequency;
                posePitch = glm::radians(poseConfig.crawlPitchDegrees);
                poseBaseY -= poseConfig.crawlHeightDrop;
            }
            else if (zombie->state == ZombieState::Attacking)
            {
                bobAmplitude = poseConfig.attackBobAmplitude;
                float attackPoseOsc = std::abs(std::sin(zombie->motionTime * attackPitchFrequencyLocal));
                posePitch = glm::radians(poseConfig.attackPitchDegrees) * attackPoseOsc;
            }
            else
            {
                poseRoll = glm::radians(poseConfig.walkRollDegrees) * std::sin(zombie->motionTime * 0.5f * poseConfig.walkBobFrequency);
            }

            float bob = (bobAmplitude > 0.0f && bobFrequency > 0.0f)
                            ? std::sin(zombie->motionTime * bobFrequency) * bobAmplitude
                            : 0.0f;

            entity->localTransform.position.y = poseBaseY + bob;
            entity->localTransform.rotation.x = -posePitch;
            entity->localTransform.rotation.z = poseRoll;
            entity->localTransform.scale = prototypeTransform.scale * poseConfig.modelScaleMultiplier;
        }

        void updateAllZombies(
            World *world,
            float deltaTime,
            const glm::vec3 &playerTarget,
            HealthComponent *playerHealth,
            float zombieModelYawOffset,
            float deathFallDegrees,
            float deathSink,
            const Transform &prototypeTransform,
            const ZombiePoseConfig &poseConfig) const
        {
            if (!world)
                return;

            for (auto entity : world->getEntities())
            {
                auto *zombie = entity->getComponent<ZombieComponent>();
                auto *health = entity->getComponent<HealthComponent>();
                if (!(zombie && health))
                    continue;

                zombie->update(deltaTime);
                updateZombieAnimation(entity, zombie, deltaTime);

                zombie->applyHealthState(health->isAlive);
                if (zombie->updateDeathTransform(entity->localTransform, deathFallDegrees, deathSink))
                {
                    world->markForRemoval(entity);
                    continue;
                }

                glm::vec3 zombiePosition = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                zombie->updateMovementAndCombat(
                    entity->localTransform,
                    zombiePosition,
                    playerTarget,
                    zombieModelYawOffset,
                    deltaTime,
                    playerHealth);

                applyZombiePose(entity, zombie, prototypeTransform, poseConfig);
            }
        }

        void updateAllZombies(
            World *world,
            float deltaTime,
            const glm::vec3 &playerTarget,
            HealthComponent *playerHealth,
            const ZombieAnimationConfig &animationConfig) const
        {
            updateAllZombies(
                world,
                deltaTime,
                playerTarget,
                playerHealth,
                animationConfig.modelYawOffset,
                animationConfig.deathFallDegrees,
                animationConfig.deathSink,
                animationConfig.prototypeTransform,
                animationConfig.pose);
        }

        bool handleZombieKill(
            World *world,
            Entity *hitEntity,
            bool killedZombie,
            int &zombiesKilledCount,
            const Transform &prototypeTransform,
            Material *fallbackMaterial)
        {
            if (!(world && killedZombie && hitEntity))
                return false;

            zombiesKilledCount++;
            glm::vec3 worldPos = glm::vec3(hitEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
            float maxAxisScale = std::max({std::abs(hitEntity->localTransform.scale.x),
                                           std::abs(hitEntity->localTransform.scale.y),
                                           std::abs(hitEntity->localTransform.scale.z),
                                           1.0f});

            spawnBloodSplashAt(
                world,
                prototypeTransform,
                fallbackMaterial,
                worldPos,
                hitEntity->localTransform.rotation.y,
                maxAxisScale);

            world->markForRemoval(hitEntity);
            return true;
        }

        bool handleZombieKill(
            World *world,
            Entity *hitEntity,
            bool killedZombie,
            int &zombiesKilledCount,
            const ZombieAnimationConfig &animationConfig)
        {
            return handleZombieKill(
                world,
                hitEntity,
                killedZombie,
                zombiesKilledCount,
                animationConfig.prototypeTransform,
                animationConfig.bloodFallbackMaterial);
        }
    };

}
