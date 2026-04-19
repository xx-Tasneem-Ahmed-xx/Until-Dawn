#include "environment.hpp"

namespace our {

    void EnvironmentComponent::deserialize(const nlohmann::json& data) {
        // Preferred field used by scene JSON files.
        if (data.contains("environmentType") && data["environmentType"].is_string()) {
            environmentType = data["environmentType"].get<std::string>();
            return;
        }

        // Backward compatibility: older scenes may have used "type" to store
        // the environment tag itself (e.g. "wall", "floor", "prop").
        if (data.contains("type") && data["type"].is_string()) {
            std::string legacyType = data["type"].get<std::string>();
            if (legacyType != "Environment") {
                environmentType = legacyType;
            }
        }
    }
}
