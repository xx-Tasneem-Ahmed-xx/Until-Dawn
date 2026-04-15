#pragma once

#include "../ecs/component.hpp"

namespace our
{

    class PlayerComponent : public Component
    {
    public:
        bool isMainPlayer = true;

        static std::string getID() { return "Player"; }

        void deserialize(const nlohmann::json &data) override;
    };

}
