#include "player-controller.hpp"
#include "../components/player.hpp"
#include "../components/camera.hpp"
#include "../ecs/entity.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace our
{

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
    if (std::abs(diff) <= maxDt) return target;
    return current + (diff > 0.0f ? maxDt : -maxDt);
}

void PlayerControllerSystem::update(World      *world,
                                    float       deltaTime,
                                    double      mouseDeltaX,
                                    double      mouseDeltaY,
                                    int         windowW,
                                    int         windowH,
                                    GLFWwindow *window)
{
    // ── 1. Find player & camera ──────────────────────────────────────────────
    Entity          *playerEntity = nullptr;
    PlayerComponent *player       = nullptr;
    Entity          *cameraEntity = nullptr;

    for (auto *e : world->getEntities())
        if (auto *p = e->getComponent<PlayerComponent>())
            if (p->isMainPlayer) { playerEntity = e; player = p; }

    if (!playerEntity || !player) return;

    for (auto *e : world->getEntities())
        if (e->parent == playerEntity && e->getComponent<CameraComponent>())
            { cameraEntity = e; break; }

    float dx = static_cast<float>(mouseDeltaX);
    float dy = static_cast<float>(mouseDeltaY);

    // ── 2. Mouse X → rotate player yaw (left/right) ─────────────────────────
    //  Player body, camera, and weapon all rotate together (children inherit).
    playerEntity->localTransform.rotation.y -= dx * player->rotationSensitivity;
    playerEntity->localTransform.rotation.y  = wrapAngle(playerEntity->localTransform.rotation.y);

    // ── 3. Mouse Y → rotate camera pitch (up/down) ───────────────────────────
    //  Camera is a child of the player.  Pitching the camera local X axis gives
    //  standard FPS up/down look without affecting the player body yaw.
    //  Pitch is clamped so the player can't look fully upside-down.
    if (cameraEntity)
    {
        cameraEntity->localTransform.rotation.x -= dy * player->pitchSensitivity;

        // Clamp pitch: ~80° up, ~80° down (in radians)
        const float maxPitch = glm::radians(80.0f);
        cameraEntity->localTransform.rotation.x =
            std::max(-maxPitch, std::min(maxPitch, cameraEntity->localTransform.rotation.x));
    }

    // ── 4. Crosshair — fixed at screen centre (no drift) ─────────────────────
    //  Player rotates with mouse X so the crosshair is always weapon-aligned.
    player->crosshairX = 0.0f;
    player->crosshairY = 0.0f;
    player->aimYaw     = 0.0f;
    player->aimPitch   = cameraEntity ? cameraEntity->localTransform.rotation.x : 0.0f;

    // ── 5. WASD — relative to player facing only ──────────────────────────────
    // Forward = direction the player model faces.
    // The camera is initialised at playerYaw + π (behind the player),
    // so the player's true forward is the OPPOSITE of the camera forward.
    // With yaw=0 the player faces -Z in world space, matching the scene setup.
    // Read forward and right directly from the camera world matrix.
    // This is always correct regardless of engine rotation convention,
    // euler order, or how the parent-child hierarchy composes transforms.
    // Camera -Z column = what the player sees as forward.
    // Camera +X column = what the player sees as right.
    // Project both onto XZ plane so movement stays horizontal.
    glm::vec3 fwd(0.0f, 0.0f, -1.0f);
    glm::vec3 rgt(1.0f, 0.0f,  0.0f);
    if (cameraEntity)
    {
        glm::mat4 camWorld = cameraEntity->getLocalToWorldMatrix();
        // -Z column of the world matrix = camera forward in world space
        fwd = glm::normalize(glm::vec3(-camWorld[2][0], 0.0f, -camWorld[2][2]));
        // +X column of the world matrix = camera right in world space
        rgt = glm::normalize(glm::vec3( camWorld[0][0], 0.0f,  camWorld[0][2]));
    }

    glm::vec3 move(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += fwd;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= fwd;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += rgt;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= rgt;

    bool isMovingXZ = glm::length(move) > 0.001f;
    if (isMovingXZ)
        playerEntity->localTransform.position += glm::normalize(move) * player->walkSpeed * deltaTime;

    // ── 6. Jump — SPACE ───────────────────────────────────────────────────────
    bool jPressed = (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS);
    static bool jKeyWasPressedLastFrame = false;
   if (!player->isJumping && jPressed && jKeyWasPressedLastFrame){
        player->isJumping        = true;
        player->verticalVelocity = player->jumpSpeed;
    }
    jKeyWasPressedLastFrame = jPressed;
float distanceToFloor = playerEntity->localTransform.position.y - player->groundY;
if (std::abs(distanceToFloor) > 0.01f) {
    std::cout << "[DEBUG] Player Y: " << playerEntity->localTransform.position.y 
              << " | GroundY: " << player->groundY 
              << " | Gap: " << distanceToFloor << std::endl;
}
    if (player->isJumping)
    {
        player->verticalVelocity                += player->gravity * deltaTime;
        playerEntity->localTransform.position.y += player->verticalVelocity * deltaTime;

        if (playerEntity->localTransform.position.y <= player->groundY)
        {
            playerEntity->localTransform.position.y = player->groundY;
            player->isJumping        = false;
            player->verticalVelocity = 0.0f;
        }
    }

    // ── 7. Animation state ────────────────────────────────────────────────────
    if (player->isJumping)
        player->animationState = PlayerAnimationState::Jumping;
    else if (isMovingXZ)
        player->animationState = PlayerAnimationState::Running;
    else
        player->animationState = PlayerAnimationState::Idle;
}

glm::vec2 PlayerControllerSystem::getCrosshairScreenPos(int windowW, int windowH,
                                                         const PlayerComponent *player) const
{
    // Crosshair X is always 0 (screen centre horizontally).
    // Y drifts slightly for vertical aim feel.
    float sx = ( player->crosshairX + 1.0f) * 0.5f * static_cast<float>(windowW);
    float sy = (-player->crosshairY + 1.0f) * 0.5f * static_cast<float>(windowH);
    return { sx, sy };
}

} // namespace our