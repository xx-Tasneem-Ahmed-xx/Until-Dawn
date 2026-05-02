#include "player.hpp"

namespace our
{

    void PlayerComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        isMainPlayer          = data.value("isMainPlayer",          isMainPlayer);
        walkSpeed             = data.value("walkSpeed",             walkSpeed);
        crosshairSensitivity  = data.value("crosshairSensitivity",  crosshairSensitivity);
        crosshairMaxRadius    = data.value("crosshairMaxRadius",    crosshairMaxRadius);
        bodyTurnSpeed         = data.value("bodyTurnSpeed",         bodyTurnSpeed);

        // Legacy fields – still read so existing JSON configs don't break
        rotationSensitivity   = data.value("rotationSensitivity",   rotationSensitivity);
        pitchSensitivity      = data.value("pitchSensitivity",       pitchSensitivity);

        // Reset all runtime state
        shootRequested   = false;
        crosshairX       = 0.0f;
        crosshairY       = 0.0f;
        aimYaw           = 0.0f;
        aimPitch         = 0.0f;
        skinMatrices.clear();

        animationState   = PlayerAnimationState::Idle;
        activeMotionClip.clear();
        motionClipTime   = 0.0f;
        shootClipTime    = 0.0f;
    }

}