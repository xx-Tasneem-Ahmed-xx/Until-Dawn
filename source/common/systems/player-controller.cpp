#include "player-controller.hpp"
#include "../components/player.hpp"
#include "../components/camera.hpp"
#include "../ecs/entity.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cmath>
#include <algorithm>

namespace our
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Wrap an angle (radians) into [-π, +π]
static float wrapAngle(float a)
{
    while (a >  glm::pi<float>()) a -= glm::two_pi<float>();
    while (a < -glm::pi<float>()) a += glm::two_pi<float>();
    return a;
}

float PlayerControllerSystem::smoothYaw(float current, float target,
                                        float turnSpeedDeg, float dt)
{
    float diff  = wrapAngle(target - current);
    float maxDt = glm::radians(turnSpeedDeg) * dt;

    if (std::abs(diff) <= maxDt)
        return target;

    return current + (diff > 0.0f ? maxDt : -maxDt);
}

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------
void PlayerControllerSystem::update(World      *world,
                                    float       deltaTime,
                                    double      mouseDeltaX,
                                    double      mouseDeltaY,
                                    int         windowW,
                                    int         windowH,
                                    GLFWwindow *window)
{
    // ── 1. Find the player entity & component ───────────────────────────────
    Entity          *playerEntity     = nullptr;
    PlayerComponent *player           = nullptr;
    Entity          *cameraEntity     = nullptr;

    for (auto *e : world->getEntities())
    {
        if (auto *p = e->getComponent<PlayerComponent>())
        {
            if (p->isMainPlayer) { playerEntity = e; player = p; }
        }
    }

    if (!playerEntity || !player) return;

    // Prefer the camera that is parented to the main player, then fallback to any camera.
    for (auto *e : world->getEntities())
    {
        if (e->parent == playerEntity && e->getComponent<CameraComponent>())
        {
            cameraEntity = e;
            break;
        }
    }
    if (!cameraEntity)
    {
        for (auto *e : world->getEntities())
        {
            if (e->getComponent<CameraComponent>())
            {
                cameraEntity = e;
                break;
            }
        }
    }

    // ── 2. Move the crosshair in NDC space based on raw mouse delta ──────────
    //
    //  crosshairSensitivity maps one pixel of mouse movement to a fraction of
    //  screen-width in NDC units.  Keeping it ≤ 0.001 feels sluggish-but-
    //  precise, which is what the user asked for.
    //
    //  NDC: X right = +1, Y up = +1  (opposite to typical screen-Y)
    float safeWindowW = std::max(1, windowW);
    float safeWindowH = std::max(1, windowH);
    float ndcPerPixelX = player->crosshairSensitivity;
    float ndcPerPixelY = player->crosshairSensitivity
                         * (safeWindowW / safeWindowH);

    player->crosshairX += static_cast<float>(mouseDeltaX) * ndcPerPixelX;
    player->crosshairY += static_cast<float>(mouseDeltaY) * ndcPerPixelY;

    // Clamp to a circular region so the crosshair stays on screen
    float r = player->crosshairMaxRadius;
    float len = std::sqrt(player->crosshairX * player->crosshairX +
                          player->crosshairY * player->crosshairY);
    if (len > r)
    {
        player->crosshairX = player->crosshairX / len * r;
        player->crosshairY = player->crosshairY / len * r;
    }

    // ── 3. Convert crosshair NDC → aim angles relative to camera forward ─────
    //
    //  We treat the crosshair as a point on a virtual "aim plane" 1 unit ahead
    //  of the camera.  atan2 gives us the horizontal and vertical deflections.
    //
    //  crosshairX in NDC ∈ [-r, r]  maps to a half-FOV-weighted angle.
    //  We use a simple linear approximation: tan(angle) ≈ ndc * tan(halfFov).
    //  For simplicity (no camera FOV reference here) we use a fixed half-FOV
    //  of 45° which gives a comfortable aiming feel.  If you have the camera
    //  FOV available, replace 1.0f with tan(fov/2).
    const float kHalfFovTan = 1.0f; // tan(45°) = 1

    player->aimYaw   = std::atan2(player->crosshairX * kHalfFovTan, 1.0f);
    player->aimPitch = std::atan2(player->crosshairY * kHalfFovTan, 1.0f);

    // ── 4. Compute the camera's current world-space yaw ─────────────────────
    float cameraWorldYaw = 0.0f;
    if (cameraEntity)
    {
        // Camera entity rotation is stored as Euler angles in the Transform.
        // We only care about the Y (yaw) component.
        glm::vec3 camRot = cameraEntity->localTransform.rotation; // (pitch, yaw, roll) in radians
        cameraWorldYaw   = camRot.y;
    }

    // ── 5. WASD drives player facing (not camera), then movement follows ─────
    //
    //  Input is interpreted relative to camera yaw so:
    //  W=forward, S=back, D=right, A=left (including diagonals).
    glm::mat4 camMatrix = cameraEntity->getLocalToWorldMatrix();

    glm::vec3 cameraForward = -glm::vec3(camMatrix[2]);
    glm::vec3 cameraRight   =  glm::vec3(camMatrix[0]);

    cameraForward.y = 0;
    cameraRight.y   = 0;

    cameraForward = glm::normalize(cameraForward);
    cameraRight   = glm::normalize(cameraRight);

    float inputX = 0.0f;
    float inputZ = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputX += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputX -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputZ += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputZ -= 1.0f;

    glm::vec3 desiredDir = cameraRight * inputX + cameraForward * inputZ;
    bool isMoving = glm::length(desiredDir) > 0.001f;
    if (isMoving)
        desiredDir = glm::normalize(desiredDir);

    // Player body always faces the crosshair (camera yaw + horizontal aim offset).
    float targetYaw = cameraWorldYaw + player->aimYaw;

    playerEntity->localTransform.rotation.y =
        smoothYaw(playerEntity->localTransform.rotation.y,
                  targetYaw,
                  player->turnSpeed,
                  deltaTime);

    // Move toward intended direction.
    playerEntity->localTransform.position += desiredDir * player->walkSpeed * deltaTime;

    // Animation state hint (used by play-state animation logic)
    player->animationState = isMoving
                             ? PlayerAnimationState::Running
                             : PlayerAnimationState::Idle;
}

// ---------------------------------------------------------------------------
// getCrosshairScreenPos()
// ---------------------------------------------------------------------------
glm::vec2 PlayerControllerSystem::getCrosshairScreenPos(int windowW, int windowH,
                                                         const PlayerComponent *player) const
{
    // NDC [-1,+1] → screen pixels (Y flipped: NDC +Y = up, screen +Y = down)
    float sx = ( player->crosshairX + 1.0f) * 0.5f * static_cast<float>(windowW);
    float sy = (-player->crosshairY + 1.0f) * 0.5f * static_cast<float>(windowH);
    return { sx, sy };
}

} // namespace our