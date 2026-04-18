#pragma once
#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    // An enum to define light types
    enum class LightType {
        DIRECTIONAL,
        POINT,
        SPOT
    };

    class LightComponent : public Component {
    public:
        LightType lightType = LightType::DIRECTIONAL;
        glm::vec3 diffuse = glm::vec3(1.0f);
        glm::vec3 specular = glm::vec3(1.0f);
        glm::vec3 attenuation = glm::vec3(1.0f, 0.0f, 0.0f); // x=constant, y=linear, z=quadratic
        glm::vec2 cone_angles = glm::vec2(glm::radians(20.0f), glm::radians(30.0f)); // For spot lights: x=inner, y=outer

        // The ID of this component type is "Light"
        static std::string getID() { return "Light"; }

        // Deserialize light data from JSON
        void deserialize(const nlohmann::json& data) override;
    };

}
