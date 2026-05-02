#pragma once

#include <animation/motion.hpp>
#include <ecs/entity.hpp>
#include <mesh/mesh.hpp>

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace ending
{
    void applyClipToSkinnedEntity(our::Entity *entity, our::Mesh *mesh, our::Motion *motion, const our::MotionClip *clip, float localTime);

    inline float positiveModulo(float value, float period)
    {
        if (period <= 0.0f)
            return 0.0f;
        float wrapped = std::fmod(value, period);
        if (wrapped < 0.0f)
            wrapped += period;
        return wrapped;
    }

    struct WinRuntime
    {
        float playerGroundOffset = -1.10f;
        glm::vec3 playerPositionOffset = glm::vec3(0.0f);
        glm::vec3 playerRotationDegrees = glm::vec3(0.0f);
        glm::vec3 cameraTargetOffset = glm::vec3(0.0f);
        float cameraBlendInDuration = 2.2f;
        glm::vec3 cameraStartPosition = glm::vec3(0.0f);
        float cameraStartYaw = 0.0f;
        float cameraStartPitch = 0.0f;

        float orbitRadius = 8.8f;
        float orbitHeight = 3.8f;
        float orbitAngularSpeed = 0.16f;
        float targetPitchDegrees = -12.0f;
        float zoomTargetDistance = 3.2f;
        float zoomHeightOffset = 1.2f;
        float zoomDuration = 1.4f;
    };

    inline void applyWinPose(our::Entity *playerVisualEntity, float endingGroundY, const glm::vec3 &endingStageCenter, const WinRuntime &runtime)
    {
        if (!playerVisualEntity)
            return;
        glm::vec3 basePosition = endingStageCenter + runtime.playerPositionOffset;
        basePosition.y = endingGroundY + runtime.playerGroundOffset + runtime.playerPositionOffset.y;
        playerVisualEntity->localTransform.position = basePosition;
        playerVisualEntity->localTransform.rotation = glm::radians(runtime.playerRotationDegrees);
    }

    inline void cacheStartCameraPose(WinRuntime &runtime, our::Entity *cameraEntity)
    {
        if (!cameraEntity)
            return;
        runtime.cameraStartPosition = cameraEntity->localTransform.position;
        runtime.cameraStartYaw = cameraEntity->localTransform.rotation.y;
        runtime.cameraStartPitch = cameraEntity->localTransform.rotation.x;
    }

    inline void updateWinCamera(our::Entity *cameraEntity, our::Entity *playerVisualEntity, float elapsedTime, const glm::vec3 &endingStageCenter, const WinRuntime &runtime)
    {
        if (!(cameraEntity && playerVisualEntity))
            return;

        glm::vec3 targetPos = endingStageCenter + runtime.cameraTargetOffset;
        const float targetPitch = glm::radians(runtime.targetPitchDegrees);

        float angle = elapsedTime * runtime.orbitAngularSpeed;
        glm::vec3 orbitPosition = targetPos + glm::vec3(std::sin(angle) * runtime.orbitRadius, runtime.orbitHeight, std::cos(angle) * runtime.orbitRadius);

        float blendT = std::clamp(elapsedTime / std::max(0.01f, runtime.cameraBlendInDuration), 0.0f, 1.0f);
        float smoothBlendT = blendT * blendT * (3.0f - 2.0f * blendT);

        cameraEntity->localTransform.position = glm::mix(runtime.cameraStartPosition, orbitPosition, smoothBlendT);

        // glm::vec3 toTarget = playerPos - cameraEntity->localTransform.position;
        // toTarget.y = 0.0f;
        // if (glm::dot(toTarget, toTarget) > 0.0001f)
        // {
        //     float desiredYaw = std::atan2(-toTarget.x, -toTarget.z);
        //     cameraEntity->localTransform.rotation.y = glm::mix(runtime.cameraStartYaw, desiredYaw, smoothBlendT);
        // }

        cameraEntity->localTransform.rotation.x = glm::mix(runtime.cameraStartPitch, targetPitch, smoothBlendT);
        cameraEntity->localTransform.rotation.z = 0.0f;
        glm::vec3 basePosition = glm::mix(runtime.cameraStartPosition, orbitPosition, smoothBlendT);

        float zoomT = 0.0f;
        glm::vec3 finalPosition = basePosition;
        if (elapsedTime > runtime.cameraBlendInDuration && runtime.zoomDuration > 0.0f)
        {
            zoomT = std::clamp((elapsedTime - runtime.cameraBlendInDuration) / runtime.zoomDuration, 0.0f, 1.0f);
            glm::vec3 dir = basePosition - targetPos;
            float dirLen = glm::length(dir);
            if (dirLen > 1e-4f)
                dir /= dirLen;
            else
                dir = glm::vec3(0.0f, 0.0f, 1.0f);

            glm::vec3 zoomTargetPos = targetPos + dir * runtime.zoomTargetDistance + glm::vec3(0.0f, runtime.zoomHeightOffset, 0.0f);
            finalPosition = glm::mix(basePosition, zoomTargetPos, zoomT);
        }
        cameraEntity->localTransform.position = finalPosition;

        // Compute desired yaw/pitch to look at player from the final camera position
        glm::vec3 toTarget = targetPos - finalPosition;
        float dist2 = glm::dot(toTarget, toTarget);
        if (dist2 > 0.0001f)
        {
            float desiredYaw = std::atan2(-toTarget.x, -toTarget.z);
            float horizontalDist = glm::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
            float desiredPitch = std::atan2(toTarget.y, horizontalDist);

            float rotBlend = std::clamp(smoothBlendT + zoomT, 0.0f, 1.0f);
            cameraEntity->localTransform.rotation.y = glm::mix(runtime.cameraStartYaw, desiredYaw, rotBlend);
            cameraEntity->localTransform.rotation.x = glm::mix(runtime.cameraStartPitch, desiredPitch, rotBlend);
        }
        else
        {
            cameraEntity->localTransform.rotation.x = glm::mix(runtime.cameraStartPitch, targetPitch, smoothBlendT);
        }

        cameraEntity->localTransform.rotation.z = 0.0f;
    }

    inline void updateWinActors(
        our::Entity *playerVisualEntity,
        our::Mesh *playerMesh,
        our::Motion *playerMotion,
        const our::MotionClip *playerWinClip,
        float elapsedTime,
        float endingGroundY,
        const glm::vec3 &endingStageCenter,
        const WinRuntime &runtime)
    {
        applyWinPose(playerVisualEntity, endingGroundY, endingStageCenter, runtime);

        if (playerWinClip)
        {
            float playerTime = positiveModulo(elapsedTime, std::max(0.01f, playerWinClip->duration));
            applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerWinClip, playerTime);
        }
    }
}
