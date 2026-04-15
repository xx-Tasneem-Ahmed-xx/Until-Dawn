#pragma once

#include "../ecs/component.hpp"

namespace our
{

    // Entities with this component can take damage and be healed
    class HealthComponent : public Component
    {
    public:
        float maxHealth = 100.0f;
        float currentHealth = 100.0f;
        bool isAlive = true;

        static std::string getID() { return "Health"; }

        // Reads health data from the given json object
        void deserialize(const nlohmann::json &data) override;

        void takeDamage(float damageAmount);

        void heal(float healAmount);
    };

}
