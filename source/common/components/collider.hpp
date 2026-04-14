#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    // Collider component - defines collision boundaries for entities
    // Supports AABB (Axis-Aligned Bounding Box) collisions
    class ColliderComponent : public Component {
    public:
        glm::vec3 center = glm::vec3(0, 0, 0);   // Center offset from entity position
        glm::vec3 halfSize = glm::vec3(1, 1, 1); // Half extents of the bounding box
        bool isTrigger = false;                   // If true, collision will be reported but no physics response

        static std::string getID() { return "Collider"; }

        void deserialize(const nlohmann::json& data) override;

        // Get the world AABB bounds
        void getWorldBounds(glm::vec3& min, glm::vec3& max) const;

        // Check if this collider intersects with another
        bool intersects(const ColliderComponent* other) const;

        // Check if a point is inside this collider
        bool contains(const glm::vec3& point) const;
    };

}
