#include "environment.hpp"

namespace our {

    void EnvironmentComponent::deserialize(const nlohmann::json& data) {
        if (data.contains("type"))
            environmentType = data["type"].get<std::string>();
    }
}
