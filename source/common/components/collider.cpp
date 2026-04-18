#include "collider.hpp"
#include "../ecs/entity.hpp"
#include "../ecs/transform.hpp"
#include <glm/glm.hpp>

namespace our {

    void ColliderComponent::deserialize(const nlohmann::json& data) {
        // Deserialize center (optional)
        if (data.contains("center") && data["center"].is_array()) {
            center[0] = data["center"][0].get<float>();
            center[1] = data["center"][1].get<float>();
            center[2] = data["center"][2].get<float>();
        }
        
        // Deserialize halfSize (optional)
        if (data.contains("halfSize") && data["halfSize"].is_array()) {
            halfSize[0] = data["halfSize"][0].get<float>();
            halfSize[1] = data["halfSize"][1].get<float>();
            halfSize[2] = data["halfSize"][2].get<float>();
        }
        
        // Alternative: Allow specifying full size instead of half size
        if (data.contains("size") && data["size"].is_array()) {
            glm::vec3 size;
            size[0] = data["size"][0].get<float>();
            size[1] = data["size"][1].get<float>();
            size[2] = data["size"][2].get<float>();
            halfSize = size * 0.5f;
        }
        
        // Deserialize isTrigger (optional)
        if (data.contains("isTrigger"))
            isTrigger = data["isTrigger"].get<bool>();
    }

    void ColliderComponent::getWorldBounds(glm::vec3& min, glm::vec3& max) const {
        Entity* entity = getOwner();
        glm::vec3 worldPos = entity->getLocalToWorldMatrix() * glm::vec4(center, 1.0f);
        
        // Scale the half size by entity scale
        glm::vec3 scale = entity->localTransform.scale;
        glm::vec3 scaledHalfSize = halfSize * scale;

        min = worldPos - scaledHalfSize;
        max = worldPos + scaledHalfSize;
    }

    bool ColliderComponent::intersects(const ColliderComponent* other) const {
        glm::vec3 thisMin, thisMax;
        this->getWorldBounds(thisMin, thisMax);

        glm::vec3 otherMin, otherMax;
        other->getWorldBounds(otherMin, otherMax);

        // AABB collision check
        return (thisMin.x <= otherMax.x && thisMax.x >= otherMin.x) &&
               (thisMin.y <= otherMax.y && thisMax.y >= otherMin.y) &&
               (thisMin.z <= otherMax.z && thisMax.z >= otherMin.z);
    }

    bool ColliderComponent::contains(const glm::vec3& point) const {
        glm::vec3 min, max;
        getWorldBounds(min, max);
        
        return (point.x >= min.x && point.x <= max.x) &&
               (point.y >= min.y && point.y <= max.y) &&
               (point.z >= min.z && point.z <= max.z);
    }
}
