#pragma once

#include "../ecs/component.hpp"
#include <string>

namespace our {

    // Environment component - marks an entity as a static environment piece
    // Used to organize scene geometry (walls, floors, obstacles, props)
    class EnvironmentComponent : public Component {
    public:
        std::string environmentType = "prop"; // Type: "wall", "floor", "prop", "obstacle", etc.

        static std::string getID() { return "Environment"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
