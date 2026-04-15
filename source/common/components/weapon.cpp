#include "weapon.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"

namespace our
{

    void WeaponComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        // Deserialize ammo capacity and current ammo
        maxAmmo = data.value("maxAmmo", maxAmmo);
        ammo = data.value("ammo", ammo);

        // Deserialize shooting parameters
        maxRange = data.value("maxRange", maxRange);
        fireRate = data.value("fireRate", fireRate);
        damage = data.value("damage", damage);

        // Deserialize reload parameters
        reloadTime = data.value("reloadTime", reloadTime);

        // Deserialize sound paths
        shootSound = data.value("shootSound", shootSound);
        reloadSound = data.value("reloadSound", reloadSound);

        // isReloading, reloadTimer, and fireCooldown are runtime state and should not be set during deserialization (they default to false/0)
    }

}