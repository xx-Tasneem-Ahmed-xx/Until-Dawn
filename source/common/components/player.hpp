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
        Shooting
    };

    // PlayerComponent used by the PlayerControllerSystem and the Playstate animation logic.
    // Stores tuning parameters and runtime state for the main player.
    class PlayerComponent : public Component
    {
    public:
        bool isMainPlayer = true;    // Marks the entity as the main controllable player
        float walkSpeed = 1.8f;      // units per second

        // Mouse sensitivity for crosshair movement (kept very low for 3rd-person feel)
    float crosshairSensitivity = 0.0004f; // pixels of crosshair movement per pixel of mouse movement (screen-space)

        // Crosshair position in Normalised Device Coordinates [-1, +1]
        // (0,0) = screen centre.  Updated every frame by PlayerControllerSystem.
        float crosshairX = 0.0f;
        float crosshairY = 0.0f;

        // Maximum crosshair travel from centre, in NDC units [0..1].
        // 0.6 keeps it well inside a typical 16:9 frame.
        float crosshairMaxRadius = 0.6f;

        // How fast the player body yaws to face the crosshair direction (deg/s).
        // Lower = lazy / cinematic; higher = snappy / responsive.
        float bodyTurnSpeed = 120.0f; // degrees per second

        // Derived aim angles computed by PlayerControllerSystem each frame.
        // aimYaw   – horizontal angle from camera-forward to the crosshair ray (radians)
        // aimPitch – vertical   angle from camera-forward to the crosshair ray (radians)
        float aimYaw   = 0.0f;
        float aimPitch = 0.0f;

        // ----- Legacy fields kept for compatibility with animation/shoot logic -----
        float rotationSensitivity = 0.0020f; // (no longer drives body rotation directly)
        float pitchSensitivity    = 0.0020f; // (no longer drives camera pitch directly)
        bool  shootRequested      = false;

        // Runtime animation state used by play-state.hpp
        std::vector<glm::mat4>  skinMatrices;
        PlayerAnimationState    animationState  = PlayerAnimationState::Idle;
        std::string             activeMotionClip;
        float                   motionClipTime  = 0.0f;
        float                   shootClipTime   = 0.0f;
        float turnSpeed = 360.0f; 

        static std::string getID() { return "Player"; }

        void deserialize(const nlohmann::json &data) override;
    };

}