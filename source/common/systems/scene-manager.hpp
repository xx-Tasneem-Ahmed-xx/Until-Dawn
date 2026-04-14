#pragma once

#include "../ecs/world.hpp"
#include "../components/collider.hpp"
#include "../components/environment.hpp"
#include <glm/glm.hpp>

namespace our {

    // Scene manager to help with common scene setup tasks
    class SceneManager {
    public:
        // Create a floor entity
        static Entity* createFloor(World* world, const glm::vec3& position, const glm::vec3& scale, 
                                   const std::string& meshName = "plane", const std::string& materialName = "grass");

        // Create a wall entity
        static Entity* createWall(World* world, const glm::vec3& position, const glm::vec3& scale, 
                                  const std::string& meshName = "cube", const std::string& materialName = "metal");

        // Create a prop entity (static object like a box or obstacle)
        static Entity* createProp(World* world, const glm::vec3& position, const glm::vec3& scale, 
                                  const std::string& meshName = "cube", const std::string& materialName = "metal");

        // Create a simple collider-only entity (invisible collision shape)
        static Entity* createCollisionBox(World* world, const glm::vec3& position, const glm::vec3& halfSize);

        // Add collider to existing entity
        static void addCollider(Entity* entity, const glm::vec3& halfSize, const glm::vec3& center = glm::vec3(0));

        // Add environment component to existing entity
        static void addEnvironmentComponent(Entity* entity, const std::string& type = "prop");

        // Load all entities in a world and validate their components
        static void validateWorld(World* world);
    };

}
