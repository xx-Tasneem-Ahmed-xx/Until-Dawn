#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>

namespace our
{

    enum class PlayerAnimationState
    {
        Idle,
        Running,
        Shooting,
        Jumping
    };

    // PlayerComponent used by the PlayerControllerSystem and the Playstate animation logic.
    class PlayerComponent : public Component
    {
    public:
        // ── Basic movement ────────────────────────────────────────────────────
        bool  isMainPlayer         = true;
        float walkSpeed            = 1.8f;    // units/second

        // ── Mouse rotation ────────────────────────────────────────────────────
        // Mouse X delta  → player body yaws instantly by (delta * rotationSensitivity).
        // Mouse Y delta  → crosshair moves vertically (no camera pitch).
        float rotationSensitivity  = 0.0020f; // radians / pixel  (horizontal)
        float pitchSensitivity     = 0.0015f; // radians / pixel  (vertical, crosshair only)

        // ── Crosshair (screen-space HUD dot) ─────────────────────────────────
        // The crosshair position in NDC [-1, +1].  (0,0) = screen centre.
        // Mouse X moves it horizontally AND rotates the player simultaneously.
        // Mouse Y moves it vertically only (no camera pitch).
        float crosshairSensitivity = 0.0008f; // NDC drift per pixel (small = slow)
        float crosshairX           = 0.0f;
        float crosshairY           = 0.0f;
        float crosshairMaxRadius   = 0.55f;   // clamp radius in NDC

        // ── Derived aim angles (set every frame by PlayerControllerSystem) ────
        float aimYaw               = 0.0f;    // radians, horizontal offset from camera fwd
        float aimPitch             = 0.0f;    // radians, vertical   offset from camera fwd

        // ── Jump / gravity ────────────────────────────────────────────────────
        float jumpSpeed            = 6.5f;    // initial upward velocity (units/s)
        float gravity              = -18.0f;  // downward acceleration  (units/s²) — negative
        float groundY              = 0.0f;    // Y at which the player is considered grounded

        // Runtime jump state – NOT serialised, reset on deserialize
        bool  isJumping            = false;
        float verticalVelocity     = 0.0f;

        // ── Legacy / animation ────────────────────────────────────────────────
        bool shootRequested = false;

        std::vector<glm::mat4>  skinMatrices;
        PlayerAnimationState    animationState   = PlayerAnimationState::Idle;
        std::string             activeMotionClip;
        float                   motionClipTime   = 0.0f;
        float                   shootClipTime    = 0.0f;

        static std::string getID() { return "Player"; }

        void deserialize(const nlohmann::json &data) override;
    };

}