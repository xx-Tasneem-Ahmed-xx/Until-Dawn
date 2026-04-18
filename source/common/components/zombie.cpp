#include "zombie.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"

namespace our {
    
    void ZombieComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        radius       = data.value("radius",         radius);
        speed        = data.value("speed",           speed);
        damage       = data.value("damage",          damage);
        attackRange  = data.value("attackRange",     attackRange);
        attackCooldown = data.value("attackCooldown", attackCooldown);
    }
    
}
