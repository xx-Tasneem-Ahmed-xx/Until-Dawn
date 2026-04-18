#include "light.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void LightComponent::deserialize(const nlohmann::json& data) {
        if(!data.is_object()) return;

        std::string typeStr = data.value("lightType", "directional");
        if (typeStr == "point") lightType = LightType::POINT;
        else if (typeStr == "spot") lightType = LightType::SPOT;
        else lightType = LightType::DIRECTIONAL;

        diffuse = data.value("color", glm::vec3(1.0f));
        // Scale diffuse by intensity if provided
        float intensity = data.value("intensity", 1.0f);
        diffuse *= intensity;
        
        specular = data.value("specular", diffuse);
        attenuation = data.value("attenuation", glm::vec3(1.0f, 0.0f, 0.0f));
        
        if (data.contains("cone_angles")) {
            cone_angles.x = glm::radians(data["cone_angles"][0].get<float>());
            cone_angles.y = glm::radians(data["cone_angles"][1].get<float>());
        }
    }

}
