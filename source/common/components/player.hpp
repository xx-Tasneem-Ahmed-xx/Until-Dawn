#pragma once

#include "../ecs/component.hpp"
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

    class PlayerComponent : public Component
    {
    public:
        bool isMainPlayer = true;
        PlayerAnimationState animationState = PlayerAnimationState::Idle;
        std::string activeMotionClip;
        float motionClipTime = 0.0f;
        float shootClipTime = 0.0f;
        bool shootRequested = false;
        std::vector<glm::mat4> skinMatrices;

        static std::string getID() { return "Player"; }

        void deserialize(const nlohmann::json &data) override;
    };

}
