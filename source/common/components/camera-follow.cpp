#include "camera-follow.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void CameraFollowComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object()) return;
        if (data.contains("offset") && data["offset"].is_array())
            offset = data["offset"].get<glm::vec3>();
        smoothTime = data.value("smoothTime", smoothTime);
    }

}
