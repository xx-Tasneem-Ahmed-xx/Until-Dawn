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
#include <components/free-camera-controller.hpp>

// PHASE 2 PLAY STATE - With collision detection, scene management, and animated postprocessing
class PlaystatePhase2: public our::State {

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::CollisionSystem collisionSystem;

    // Elapsed time — fed to animated postprocess shaders (film grain)
    float totalTime = 0.0f;

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

        // Validate the scene — prints entity/collider/environment counts to stdout
        our::SceneManager::validateWorld(&world);
        totalTime = 0.0f;
    }

    void onDraw(double deltaTime) override {
        totalTime += (float)deltaTime;

        // Feed elapsed time to the renderer so animated postprocess shaders work
        renderer.setTime(totalTime);

        // Run physics and input systems
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);

        // Update collision detection
        collisionSystem.update(&world);

        // Handle collisions with proper AABB push-back
        handleCollisions((float)deltaTime);

        // Render the scene
        renderer.render(&world);

        // Escape → back to menu
        auto& keyboard = getApp()->getKeyboard();
        if(keyboard.justPressed(GLFW_KEY_ESCAPE)){
            getApp()->changeState("menu");
        }
    }

    // ──────────────────────────────────────────────────────────────────────────
    // handleCollisions — robust AABB push-back for player ↔ wall collisions
    //
    // Strategy:
    //   • For each collision pair, identify which entity is STATIC (wall/floor)
    //     and which is DYNAMIC (player or zombie).
    //   • Use collisionSystem.resolveAABB() to compute the minimum-penetration
    //     separation vector and apply it directly to the dynamic entity's position.
    //   • This works for camera-controlled entities (no MovementComponent) AND
    //     for velocity-driven entities.
    //   • Floor collisions are skipped on the Y axis (prevent sinking but
    //     don't block X/Z movement from the floor collider).
    // ──────────────────────────────────────────────────────────────────────────
    void handleCollisions(float /*deltaTime*/) {
        auto& allCollisions = collisionSystem.getCurrentCollisions();

        for (const auto& collision : allCollisions) {
            our::Entity* entityA = collision.entityA;
            our::Entity* entityB = collision.entityB;

            auto envA = entityA->template getComponent<our::EnvironmentComponent>();
            auto envB = entityB->template getComponent<our::EnvironmentComponent>();

            bool isStaticA = envA && (envA->environmentType == "wall" || envA->environmentType == "floor");
            bool isStaticB = envB && (envB->environmentType == "wall" || envB->environmentType == "floor");
            bool isFloorA  = envA && envA->environmentType == "floor";
            bool isFloorB  = envB && envB->environmentType == "floor";

            // Both static → nothing to do
            if (isStaticA && isStaticB) continue;
            // Neither is static → skip (zombie-zombie handled by zombie system)
            if (!isStaticA && !isStaticB) continue;

            // Determine which entity is dynamic and which is static
            our::Entity* dynamicEntity = isStaticA ? entityB : entityA;
            bool         collidingWithFloor = isStaticA ? isFloorA : isFloorB;

            // Build a corrected CollisionInfo so resolveAABB always pushes
            // the dynamic body (A) out of the static body (B)
            our::CollisionInfo oriented;
            if (isStaticB) {
                // entityA is dynamic, entityB is static — standard order
                oriented = collision;
            } else {
                // Swap so A = dynamic, B = static (resolveAABB pushes A out of B)
                oriented.entityA    = entityB;
                oriented.entityB    = entityA;
                oriented.colliderA  = collision.colliderB;
                oriented.colliderB  = collision.colliderA;
            }

            glm::vec3 pushBack = collisionSystem.resolveAABB(oriented);

            // For floor collisions, only allow upward push (prevent sinking);
            // zero out X/Z to avoid the floor blocking lateral movement.
            if (collidingWithFloor) {
                pushBack.x = 0.0f;
                pushBack.z = 0.0f;
                if (pushBack.y < 0.0f) pushBack.y = 0.0f; // only push up
            }

            // Apply push-back — a small epsilon keeps the entity touching the surface
            // instead of overlapping, which avoids flicker from re-detection
            const float epsilon = 0.001f;
            if (glm::length(pushBack) > epsilon) {
                dynamicEntity->localTransform.position += pushBack;
            }
        }
    }

    void onDestroy() override {
        renderer.destroy();
        cameraController.exit();
        world.clear();
        collisionSystem.clear();
        our::clearAllAssets();
    }
};
