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
        float playerGroundOffset = -0.58f;
        float cameraBlendInDuration = 2.2f;
        glm::vec3 cameraStartPosition = glm::vec3(0.0f);
        float cameraStartYaw = 0.0f;
        float cameraStartPitch = 0.0f;

        float orbitRadius = 8.8f;
        float orbitHeight = 3.8f;
        float orbitAngularSpeed = 0.16f;
        float targetPitchDegrees = -12.0f;
    };

    inline void applyWinPose(our::Entity *playerVisualEntity, float endingGroundY, const WinRuntime &runtime)
    {
        if (!playerVisualEntity)
            return;
        playerVisualEntity->localTransform.position.y = endingGroundY + runtime.playerGroundOffset;
    }

    inline void cacheStartCameraPose(WinRuntime &runtime, our::Entity *cameraEntity)
    {
        if (!cameraEntity)
            return;
        runtime.cameraStartPosition = cameraEntity->localTransform.position;
        runtime.cameraStartYaw = cameraEntity->localTransform.rotation.y;
        runtime.cameraStartPitch = cameraEntity->localTransform.rotation.x;
    }

    inline void updateWinCamera(our::Entity *cameraEntity, our::Entity *playerVisualEntity, float elapsedTime, const WinRuntime &runtime)
    {
        if (!(cameraEntity && playerVisualEntity))
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;
        const float targetPitch = glm::radians(runtime.targetPitchDegrees);

        float angle = elapsedTime * runtime.orbitAngularSpeed;
        glm::vec3 orbitPosition = playerPos + glm::vec3(std::sin(angle) * runtime.orbitRadius, runtime.orbitHeight, std::cos(angle) * runtime.orbitRadius);

        float blendT = std::clamp(elapsedTime / std::max(0.01f, runtime.cameraBlendInDuration), 0.0f, 1.0f);
        float smoothBlendT = blendT * blendT * (3.0f - 2.0f * blendT);

        cameraEntity->localTransform.position = glm::mix(runtime.cameraStartPosition, orbitPosition, smoothBlendT);

        glm::vec3 toTarget = playerPos - cameraEntity->localTransform.position;
        toTarget.y = 0.0f;
        if (glm::dot(toTarget, toTarget) > 0.0001f)
        {
            float desiredYaw = std::atan2(-toTarget.x, -toTarget.z);
            cameraEntity->localTransform.rotation.y = glm::mix(runtime.cameraStartYaw, desiredYaw, smoothBlendT);
        }

        cameraEntity->localTransform.rotation.x = glm::mix(runtime.cameraStartPitch, targetPitch, smoothBlendT);
        cameraEntity->localTransform.rotation.z = 0.0f;
    }

    inline void updateWinActors(
        our::Entity *playerVisualEntity,
        our::Mesh *playerMesh,
        our::Motion *playerMotion,
        const our::MotionClip *playerWinClip,
        float elapsedTime,
        float endingGroundY,
        const WinRuntime &runtime)
    {
        applyWinPose(playerVisualEntity, endingGroundY, runtime);

        if (playerWinClip)
        {
            float playerTime = positiveModulo(elapsedTime, std::max(0.01f, playerWinClip->duration));
            applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerWinClip, playerTime);
        }
        else if (playerVisualEntity)
        {
            playerVisualEntity->localTransform.rotation.x = 0.0f;
        }
    }
}
