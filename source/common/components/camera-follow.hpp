#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    // A component that marks a camera as following a target entity.
    // Offset is the camera local offset relative to the target (in target space).
    class CameraFollowComponent : public Component {
    public:
        glm::vec3 offset = glm::vec3(0.0f, 2.0f, 4.0f); // default: behind and above the player
        float smoothTime = 10.0f; // higher means snappier; used as exponential smoothing factor

        static std::string getID() { return "CameraFollow"; }

        void deserialize(const nlohmann::json &data) override;
    };

}

