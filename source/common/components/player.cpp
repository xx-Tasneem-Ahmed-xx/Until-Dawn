#include "player.hpp"

namespace our
{

    void PlayerComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        isMainPlayer = data.value("isMainPlayer", isMainPlayer);
        animationState = PlayerAnimationState::Idle;
        activeMotionClip.clear();
        motionClipTime = 0.0f;
        shootClipTime = 0.0f;
        shootRequested = false;
        skinMatrices.clear();
    }

}
