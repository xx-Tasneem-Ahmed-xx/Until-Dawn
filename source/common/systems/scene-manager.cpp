#include "scene-manager.hpp"
#include "../ecs/entity.hpp"
#include "../components/mesh-renderer.hpp"
#include "../asset-loader.hpp"
#include <iostream>

namespace our {

    Entity* SceneManager::createFloor(World* world, const glm::vec3& position, const glm::vec3& scale, 
                                      const std::string& meshName, const std::string& materialName) {
        Entity* floor = world->add();
        floor->localTransform.position = position;
        floor->localTransform.scale = scale;

        // Add mesh renderer if mesh and material names are provided
        if (!meshName.empty() && !materialName.empty()) {
            auto meshRenderer = floor->addComponent<MeshRendererComponent>();
            meshRenderer->mesh = AssetLoader<Mesh>::get(meshName);
            meshRenderer->material = AssetLoader<Material>::get(materialName);
        }

        // Add collider
        addCollider(floor, scale * 0.5f);

        // Add environment component
        addEnvironmentComponent(floor, "floor");

        return floor;
    }

    Entity* SceneManager::createWall(World* world, const glm::vec3& position, const glm::vec3& scale, 
                                     const std::string& meshName, const std::string& materialName) {
        Entity* wall = world->add();
        wall->localTransform.position = position;
        wall->localTransform.scale = scale;

        // Add mesh renderer
        if (!meshName.empty() && !materialName.empty()) {
            auto meshRenderer = wall->addComponent<MeshRendererComponent>();
            meshRenderer->mesh = AssetLoader<Mesh>::get(meshName);
            meshRenderer->material = AssetLoader<Material>::get(materialName);
        }

        // Add collider
        addCollider(wall, scale * 0.5f);

        // Add environment component
        addEnvironmentComponent(wall, "wall");

        return wall;
    }

    Entity* SceneManager::createProp(World* world, const glm::vec3& position, const glm::vec3& scale, 
                                     const std::string& meshName, const std::string& materialName) {
        Entity* prop = world->add();
        prop->localTransform.position = position;
        prop->localTransform.scale = scale;

        // Add mesh renderer
        if (!meshName.empty() && !materialName.empty()) {
            auto meshRenderer = prop->addComponent<MeshRendererComponent>();
            meshRenderer->mesh = AssetLoader<Mesh>::get(meshName);
            meshRenderer->material = AssetLoader<Material>::get(materialName);
        }

        // Add collider
        addCollider(prop, scale * 0.5f);

        // Add environment component
        addEnvironmentComponent(prop, "prop");

        return prop;
    }

    Entity* SceneManager::createCollisionBox(World* world, const glm::vec3& position, const glm::vec3& halfSize) {
        Entity* collisionBox = world->add();
        collisionBox->localTransform.position = position;

        // Only add a collider, no renderer
        auto collider = collisionBox->addComponent<ColliderComponent>();
        collider->halfSize = halfSize;
        collider->center = glm::vec3(0);

        return collisionBox;
    }

    void SceneManager::addCollider(Entity* entity, const glm::vec3& halfSize, const glm::vec3& center) {
        auto collider = entity->addComponent<ColliderComponent>();
        collider->halfSize = halfSize;
        collider->center = center;
    }

    void SceneManager::addEnvironmentComponent(Entity* entity, const std::string& type) {
        auto env = entity->addComponent<EnvironmentComponent>();
        env->environmentType = type;
    }

    void SceneManager::validateWorld(World* world) {
        const auto& entities = world->getEntities();
        int entityCount = 0;
        int collidableCount = 0;
        int environmentCount = 0;

        for (auto entity : entities) {
            entityCount++;
            if (entity->getComponent<ColliderComponent>()) {
                collidableCount++;
            }
            if (entity->getComponent<EnvironmentComponent>()) {
                environmentCount++;
            }
        }

        std::cout << "=== Scene Validation ===" << std::endl;
        std::cout << "Total entities: " << entityCount << std::endl;
        std::cout << "Collidable entities: " << collidableCount << std::endl;
        std::cout << "Environment entities: " << environmentCount << std::endl;
    }

}
