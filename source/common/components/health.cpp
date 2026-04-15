#include "health.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"
#include <algorithm>

namespace our
{

    void HealthComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        maxHealth = data.value("maxHealth", maxHealth);
        currentHealth = data.value("currentHealth", maxHealth);

        // Clamp currentHealth to valid range
        currentHealth = std::clamp(currentHealth, 0.0f, maxHealth);

        isAlive = currentHealth > 0.0f;
    }

    void HealthComponent::takeDamage(float damageAmount)
    {
        if (!isAlive)
            return;

        currentHealth -= damageAmount;

        if (currentHealth <= 0.0f)
        {
            currentHealth = 0.0f;
            isAlive = false;
        }
    }

    void HealthComponent::heal(float healAmount)
    {
        if (!isAlive)
            return;

        currentHealth += healAmount;

        // Clamp to maxHealth
        if (currentHealth > maxHealth)
        {
            currentHealth = maxHealth;
        }
    }

}
