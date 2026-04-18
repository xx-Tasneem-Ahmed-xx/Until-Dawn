#pragma once

#include "../ecs/component.hpp"

namespace our {

    // This component marks an entity as a zombie and provides zombie-related data
    // Member 3 will expand this with movement and behavior logic
    class ZombieComponent : public Component {
    public:
        float radius = 1.0f;        // Bounding sphere radius for collision detection
        float speed = 5.0f;         // Movement speed (placeholder for Member 3)
        float damage = 10.0f;       // Damage dealt to player per attack
        float attackRange = 1.5f;   // Distance at which zombie starts attacking
        float attackCooldown = 1.0f;// Seconds between attacks


        // The ID of this component type is "Zombie"
        static std::string getID() { return "Zombie"; }

        // Reads zombie data from the given json object
        void deserialize(const nlohmann::json& data) override;
    };

}
