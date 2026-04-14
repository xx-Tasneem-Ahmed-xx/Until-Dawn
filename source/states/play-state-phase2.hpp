#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/collision-system.hpp>
#include <systems/scene-manager.hpp>
#include <asset-loader.hpp>
#include <components/environment.hpp>
#include <components/movement.hpp>

// PHASE 2 PLAY STATE - With collision detection and scene management
class PlaystatePhase2: public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::CollisionSystem collisionSystem;  // NEW: Collision detection

    void onInitialize() override {
        // First of all, we get the scene configuration from the app config
        auto& config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if(config.contains("assets")){
            our::deserializeAllAssets(config["assets"]);
        }
        // If we have a world in the scene config, we use it to populate our world
        if(config.contains("world")){
            world.deserialize(config["world"]);
        }
        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        // NEW: Validate the scene
        our::SceneManager::validateWorld(&world);
    }

    void onDraw(double deltaTime) override {
        // Run physics and input systems
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);

        // NEW: Update collision detection
        collisionSystem.update(&world);

        // NEW: Handle collisions
        handleCollisions((float)deltaTime);

        // Render the scene
        renderer.render(&world);

        // Get a reference to the keyboard object
        auto& keyboard = getApp()->getKeyboard();

        if(keyboard.justPressed(GLFW_KEY_ESCAPE)){
            // If the escape key is pressed, go back to menu
            getApp()->changeState("menu");
        }
    }

    // NEW: Handle collision events
    void handleCollisions(float deltaTime) {
        // Get collisions that just started this frame
        auto newCollisions = collisionSystem.getNewCollisions();
        for (const auto& collision : newCollisions) {
            // std::cout << "New collision detected: " << collision.entityA->name << " and " << collision.entityB->name << std::endl;
        }

        // Get all current collisions
        auto allCollisions = collisionSystem.getCurrentCollisions();
        for (const auto& collision : allCollisions) {
            our::Entity* entityA = collision.entityA;
            our::Entity* entityB = collision.entityB;

            auto envA = entityA->template getComponent<our::EnvironmentComponent>();
            auto envB = entityB->template getComponent<our::EnvironmentComponent>();

            bool isWallA = envA && envA->environmentType == "wall";
            bool isWallB = envB && envB->environmentType == "wall";

            // If one is a wall and the other is not (like player/zombie), push the non-wall back out
            our::Entity* dynamicEntity = nullptr;
            if (isWallA && !isWallB) dynamicEntity = entityB;
            else if (isWallB && !isWallA) dynamicEntity = entityA;

            if (dynamicEntity) {
                // If it has a movement component, we reverse its velocity for this frame (basic separation)
                auto movement = dynamicEntity->template getComponent<our::MovementComponent>();
                if (movement) {
                    dynamicEntity->localTransform.position -= movement->linearVelocity * deltaTime;
                } else {
                    // For camera controller (Member 1 usually updates directly from camera)
                    // Pushing it back slightly
                    glm::vec3 separationDir = glm::normalize(dynamicEntity->localTransform.position - (isWallA ? entityA->localTransform.position : entityB->localTransform.position));
                    if(glm::length(separationDir) < 0.001f) separationDir = glm::vec3(0,0,1);
                    dynamicEntity->localTransform.position += separationDir * 0.1f;
                }
            }
        }

        // Get collisions that ended this frame
        auto endedCollisions = collisionSystem.getEndedCollisions();
        for (const auto& collision : endedCollisions) {
            std::cout << "Collision ended: " << collision.entityA->name << " and " << collision.entityB->name << std::endl;
        }
    }

    void onDestroy() override {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // Clear collision system
        collisionSystem.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};

// Note: To use this state, register it in main.cpp:
// app.registerState<PlaystatePhase2>("play-phase2");
// Then set "start-scene": "play-phase2" in your config file
