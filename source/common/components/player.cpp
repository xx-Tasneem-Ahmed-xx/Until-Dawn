#include "player.hpp"

namespace our
{

    void PlayerComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        // Movement
        isMainPlayer          = data.value("isMainPlayer",          isMainPlayer);
        walkSpeed             = data.value("walkSpeed",             walkSpeed);

        // Mouse sensitivity
        rotationSensitivity   = data.value("rotationSensitivity",   rotationSensitivity);
        pitchSensitivity      = data.value("pitchSensitivity",      pitchSensitivity);

        // Crosshair
        crosshairSensitivity  = data.value("crosshairSensitivity",  crosshairSensitivity);
        crosshairMaxRadius    = data.value("crosshairMaxRadius",    crosshairMaxRadius);

        // Jump / gravity
        jumpSpeed             = data.value("jumpSpeed",             jumpSpeed);
        gravity               = data.value("gravity",               gravity);
        groundY               = data.value("groundY",               groundY);

        // Reset all runtime state
        crosshairX            = 0.0f;
        crosshairY            = 0.0f;
        aimYaw                = 0.0f;
        aimPitch              = 0.0f;
        isJumping             = false;
        verticalVelocity      = 0.0f;
        shootRequested        = false;
        skinMatrices.clear();
        animationState        = PlayerAnimationState::Idle;
        activeMotionClip.clear();
        motionClipTime        = 0.0f;
        shootClipTime         = 0.0f;
    }

}