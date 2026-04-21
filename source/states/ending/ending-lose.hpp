#pragma once

#include <ecs/world.hpp>
#include <ecs/entity.hpp>
#include <components/mesh-renderer.hpp>
#include <components/zombie.hpp>
#include <animation/motion.hpp>
#include <mesh/mesh.hpp>

#include <algorithm>
#include <array>
#include <vector>
#include <glm/gtc/constants.hpp>

namespace ending
{
    glm::vec3 getEntityWorldPosition(our::Entity *entity);
    void applyClipToSkinnedEntity(our::Entity *entity, our::Mesh *mesh, our::Motion *motion, const our::MotionClip *clip, float localTime);

    inline void lookAtOnGround(our::Entity *entity, const glm::vec3 &target)
    {
        if (!entity)
            return;

        glm::vec3 position = getEntityWorldPosition(entity);
        glm::vec3 toTarget = target - position;
        toTarget.y = 0.0f;
        if (glm::dot(toTarget, toTarget) < 0.0001f)
            return;

        glm::vec3 direction = glm::normalize(toTarget);
        float yaw = std::atan2(-direction.x, -direction.z);
        entity->localTransform.rotation.y = yaw;
    }

    struct LoseRuntime
    {
        float playerDeathBlendIn = 0.30f;
        float playerDeathHoldFraction = 0.78f;
        float zombieBiteStart = 0.95f;
        float zombieBiteClipFraction = 0.72f;
        float zombieBiteDelayAfterDeath = 0.35f;
        float postActionHold = 1.80f;
        float skyTurnDuration = 4.50f;
        glm::vec3 zombieCircleCenterOffset = glm::vec3(0.0f, 0.0f, -0.28f);
        float zombieGroundOffset = -0.42f;
    };

    inline void applyLosePose(our::Entity *playerVisualEntity, float endingGroundY, const our::MotionClip *playerLoseClip)
    {
        if (!playerVisualEntity)
            return;

        playerVisualEntity->localTransform.position.y = endingGroundY;
        playerVisualEntity->localTransform.rotation.y = glm::radians(180.0f);
        if (!playerLoseClip)
            playerVisualEntity->localTransform.rotation.x = glm::radians(-88.0f);
    }

    inline float computeZombieBiteStart(const LoseRuntime &runtime, const our::MotionClip *playerLoseClip)
    {
        if (!playerLoseClip)
            return 1.0f;

        float deathDuration = std::max(0.01f, playerLoseClip->duration);
        float deathHoldTime = std::clamp(deathDuration * runtime.playerDeathHoldFraction, 0.0f, deathDuration);
        return runtime.playerDeathBlendIn + deathHoldTime + runtime.zombieBiteDelayAfterDeath;
    }

    inline our::Entity *spawnLoseZombieActor(
        our::World &world,
        our::Mesh *zombieMesh,
        our::Material *zombieMaterial,
        float endingZombieScale,
        const glm::vec3 &position,
        float yaw)
    {
        if (!(zombieMesh && zombieMaterial))
            return nullptr;

        our::Entity *entity = world.add();
        entity->name = "EndingZombie";
        entity->localTransform.position = position;
        entity->localTransform.rotation = glm::vec3(0.0f, yaw, 0.0f);
        entity->localTransform.scale = glm::vec3(endingZombieScale);

        auto *rendererComp = entity->addComponent<our::MeshRendererComponent>();
        rendererComp->mesh = zombieMesh;
        rendererComp->material = zombieMaterial;

        auto *zombieComp = entity->addComponent<our::ZombieComponent>();
        zombieComp->state = our::ZombieState::Crawling;
        zombieComp->shotsTaken = 1;
        if (zombieMesh->hasSkinning())
        {
            zombieComp->skinMatrices.assign(zombieMesh->getSkinJointNodes().size(), glm::mat4(1.0f));
        }

        return entity;
    }

    inline void setupLoseSceneActors(
        our::World &world,
        std::vector<our::Entity *> &loseZombies,
        std::vector<glm::vec3> &loseZombieFormationOffsets,
        our::Entity *playerVisualEntity,
        our::Mesh *zombieMesh,
        our::Material *zombieMaterial,
        float endingGroundY,
        float endingZombieScale,
        float zombieModelYawOffset,
        const LoseRuntime &runtime)
    {
        loseZombies.clear();
        loseZombieFormationOffsets.clear();
        if (!playerVisualEntity)
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;

        float ringR = 1.22f;
        std::array<glm::vec3, 3> offsets = {
            glm::vec3(ringR, 0.0f, 0.0f),
            glm::vec3(-ringR * 0.5f, 0.0f, ringR * 0.866f),
            glm::vec3(-ringR * 0.5f, 0.0f, -ringR * 0.866f)};

        for (const auto &offset : offsets)
        {
            glm::vec3 circleCenter = playerPos + runtime.zombieCircleCenterOffset;
            glm::vec3 spawnPos = circleCenter + offset;
            spawnPos.y = endingGroundY + runtime.zombieGroundOffset;
            glm::vec3 toPlayer = playerPos - spawnPos;
            float yaw = std::atan2(-toPlayer.x, -toPlayer.z) + zombieModelYawOffset;
            if (auto *zombieEntity = spawnLoseZombieActor(world, zombieMesh, zombieMaterial, endingZombieScale, spawnPos, yaw))
            {
                loseZombies.push_back(zombieEntity);
                loseZombieFormationOffsets.push_back(offset);
            }
        }
    }

    inline void orientZombieTowardPlayer(our::Entity *zombie, our::Entity *playerVisualEntity, float zombieModelYawOffset)
    {
        if (!(zombie && playerVisualEntity))
            return;

        glm::vec3 playerPos = getEntityWorldPosition(playerVisualEntity);
        glm::vec3 zombiePos = getEntityWorldPosition(zombie);
        glm::vec3 toPlayer = playerPos - zombiePos;
        toPlayer.y = 0.0f;
        if (glm::dot(toPlayer, toPlayer) < 0.0001f)
            return;

        float yaw = std::atan2(-toPlayer.x, -toPlayer.z) + zombieModelYawOffset;
        zombie->localTransform.rotation.y = yaw;
    }

    inline void updateLoseActors(
        our::Entity *playerVisualEntity,
        our::Mesh *playerMesh,
        our::Motion *playerMotion,
        const our::MotionClip *playerLoseClip,
        std::vector<our::Entity *> &loseZombies,
        const std::vector<glm::vec3> &loseZombieFormationOffsets,
        our::Mesh *zombieMesh,
        our::Motion *zombieMotion,
        const our::MotionClip *zombieAttackClip,
        const our::MotionClip *zombieCrawlClip,
        float loseSequenceTime,
        float endingGroundY,
        float zombieModelYawOffset,
        const LoseRuntime &runtime)
    {
        if (playerLoseClip)
        {
            float clipDuration = std::max(0.01f, playerLoseClip->duration);
            float holdTime = std::clamp(clipDuration * runtime.playerDeathHoldFraction, 0.0f, clipDuration);
            float playerTime = std::clamp(loseSequenceTime - runtime.playerDeathBlendIn, 0.0f, holdTime);
            applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerLoseClip, playerTime);
        }

        for (size_t i = 0; i < loseZombies.size(); ++i)
        {
            auto *zombieEntity = loseZombies[i];
            if (!zombieEntity)
                continue;

            if (i < loseZombieFormationOffsets.size() && playerVisualEntity)
            {
                glm::vec3 playerPos = playerVisualEntity->localTransform.position;
                glm::vec3 circleCenter = playerPos + runtime.zombieCircleCenterOffset;
                zombieEntity->localTransform.position = circleCenter + loseZombieFormationOffsets[i];
                zombieEntity->localTransform.position.y = endingGroundY + runtime.zombieGroundOffset;
            }

            orientZombieTowardPlayer(zombieEntity, playerVisualEntity, zombieModelYawOffset);

            const our::MotionClip *clip = zombieAttackClip ? zombieAttackClip : zombieCrawlClip;
            if (!clip)
                continue;

            float phaseOffset = 0.12f * static_cast<float>(i);
            float biteMaxTime = std::max(0.01f, clip->duration * runtime.zombieBiteClipFraction);
            float zombieTime = std::clamp(loseSequenceTime - runtime.zombieBiteStart + phaseOffset, 0.0f, biteMaxTime);
            applyClipToSkinnedEntity(zombieEntity, zombieMesh, zombieMotion, clip, zombieTime);
        }
    }

    inline void updateLoseCamera(
        our::Entity *cameraEntity,
        our::Entity *playerVisualEntity,
        const our::MotionClip *playerLoseClip,
        const our::MotionClip *zombieAttackClip,
        const our::MotionClip *zombieCrawlClip,
        float loseSequenceTime,
        const LoseRuntime &runtime)
    {
        if (!(cameraEntity && playerVisualEntity))
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;

        float playerClipDuration = playerLoseClip ? std::max(0.01f, playerLoseClip->duration) : 0.0f;
        float zombieClipDuration = zombieAttackClip ? std::max(0.01f, zombieAttackClip->duration)
                                                    : (zombieCrawlClip ? std::max(0.01f, zombieCrawlClip->duration) : 0.0f);
        float actionEndTime = std::max(runtime.playerDeathBlendIn + playerClipDuration, runtime.zombieBiteStart + zombieClipDuration) + runtime.postActionHold;

        float zoomT = std::clamp(loseSequenceTime / std::max(0.01f, actionEndTime), 0.0f, 1.0f);
        float smoothT = zoomT * zoomT * (3.0f - 2.0f * zoomT);

        float camDistance = glm::mix(6.1f, 4.8f, smoothT);
        float camHeight = glm::mix(3.05f, 2.65f, smoothT);
        glm::vec3 sideDir = glm::normalize(glm::vec3(0.9f, 0.0f, 0.45f));

        glm::vec3 baseCamPos = playerPos + sideDir * camDistance + glm::vec3(0.0f, camHeight, 0.0f);

        float skyT = std::clamp((loseSequenceTime - actionEndTime) / std::max(0.01f, runtime.skyTurnDuration), 0.0f, 1.0f);
        float skySmooth = skyT * skyT * (3.0f - 2.0f * skyT);

        cameraEntity->localTransform.position = glm::mix(baseCamPos, baseCamPos + glm::vec3(0.0f, 4.2f, -1.8f), skySmooth);
        lookAtOnGround(cameraEntity, playerPos);
        cameraEntity->localTransform.rotation.x = glm::mix(glm::radians(-19.0f), glm::radians(52.0f), skySmooth);
        cameraEntity->localTransform.rotation.z = 0.0f;
    }
}
