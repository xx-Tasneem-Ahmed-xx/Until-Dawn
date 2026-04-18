#pragma once

#include "../ecs/entity.hpp"
#include "camera.hpp"
#include "mesh-renderer.hpp"
#include "free-camera-controller.hpp"
#include "movement.hpp"
#include "weapon.hpp"
#include "health.hpp"
#include "zombie.hpp"
#include "player.hpp"
#include "collider.hpp"
#include "environment.hpp"
#include "light.hpp"

namespace our
{

    // Given a json object, this function picks and creates a component in the given entity
    // based on the "type" specified in the json object which is later deserialized from the rest of the json object
    inline void deserializeComponent(const nlohmann::json &data, Entity *entity)
    {
        std::string type = data.value("type", "");
        Component *component = nullptr;
        // TODO: (Req 8) Add an option to deserialize a "MeshRendererComponent" to the following if-else statement
        if (type == CameraComponent::getID())
        {
            component = entity->addComponent<CameraComponent>();
        }
        else if (type == FreeCameraControllerComponent::getID())
        {
            component = entity->addComponent<FreeCameraControllerComponent>();
        }
        else if (type == MeshRendererComponent::getID())
        {
            component = entity->addComponent<MeshRendererComponent>();
        }
        else if (type == MovementComponent::getID())
        {
            component = entity->addComponent<MovementComponent>();
        }
        else if (type == WeaponComponent::getID() || type == "weapon")
        {
            component = entity->addComponent<WeaponComponent>();
        }
        else if (type == Pistol::getID() || type == "pistol")
        {
            component = entity->addComponent<Pistol>();
        }
        else if (type == Rifle::getID() || type == "rifle")
        {
            component = entity->addComponent<Rifle>();
        }
        else if (type == HealthComponent::getID())
        {
            component = entity->addComponent<HealthComponent>();
        }
        else if (type == ZombieComponent::getID())
        {
            component = entity->addComponent<ZombieComponent>();
        }
        else if (type == PlayerComponent::getID())
        {
            component = entity->addComponent<PlayerComponent>();
else if (type == ColliderComponent::getID())
        {
            component = entity->addComponent<ColliderComponent>();
        }
        else if (type == EnvironmentComponent::getID())
        {
            component = entity->addComponent<EnvironmentComponent>();
        }
        else if (type == LightComponent::getID())
        {
            component = entity->addComponent<LightComponent>();
        }

        if (component)
            component->deserialize(data);
    }

}