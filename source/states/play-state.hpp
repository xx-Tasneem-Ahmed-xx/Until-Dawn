#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/shooting-system.hpp>
#include <components/camera.hpp>
#include <components/free-camera-controller.hpp>
#include <components/health.hpp>
#include <components/mesh-renderer.hpp>
#include <components/player.hpp>
#include <components/weapon.hpp>
#include <audio-manager.hpp>
#include <asset-loader.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State
{

    our::World world;
    our::ForwardRenderer renderer;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::ShootingSystem shootingSystem;
    float muzzleFlashTimeLeft = 0.0f;
    const float muzzleFlashDuration = 0.06f;
    our::Entity *mainCameraEntity = nullptr;
    our::Entity *mainPlayerEntity = nullptr;

    our::Entity *findMainPlayerEntity()
    {
        for (auto entity : world.getEntities())
        {
            if (auto player = entity->getComponent<our::PlayerComponent>(); player && player->isMainPlayer)
            {
                return entity;
            }
        }

        for (auto entity : world.getEntities())
        {
            if (entity->name == "MainPlayer")
            {
                return entity;
            }
        }

        return nullptr;
    }

    our::Entity *findMainCameraEntity(our::Entity *playerEntity)
    {
        if (playerEntity)
        {
            for (auto entity : world.getEntities())
            {
                if (entity->parent == playerEntity && entity->getComponent<our::CameraComponent>())
                {
                    return entity;
                }
            }
        }

        for (auto entity : world.getEntities())
        {
            if (entity->getComponent<our::CameraComponent>() && entity->getComponent<our::FreeCameraControllerComponent>())
            {
                return entity;
            }
        }

        for (auto entity : world.getEntities())
        {
            if (entity->getComponent<our::CameraComponent>())
            {
                return entity;
            }
        }

        return nullptr;
    }

    our::HealthComponent *getMainPlayerHealth()
    {
        our::Entity *playerEntity = mainPlayerEntity;
        if (!playerEntity)
            return nullptr;
        return playerEntity->getComponent<our::HealthComponent>();
    }

    our::WeaponComponent *getMainPlayerWeapon()
    {
        our::Entity *playerEntity = mainPlayerEntity;
        if (!playerEntity)
            return nullptr;
        return playerEntity->getComponent<our::WeaponComponent>();
    }

    void onInitialize() override
    {
        // First of all, we get the scene configuration from the app config
        auto &config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if (config.contains("assets"))
        {
            our::deserializeAllAssets(config["assets"]);
        }
        // If we have a world in the scene config, we use it to populate our world
        if (config.contains("world"))
        {
            world.deserialize(config["world"]);
        }

        mainPlayerEntity = findMainPlayerEntity();
        mainCameraEntity = findMainCameraEntity(mainPlayerEntity);

        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
    }

    void onDraw(double deltaTime) override
    {
        glm::vec2 muzzleFlashCenter = glm::vec2(0.66f, 0.28f);
        our::Entity *cameraEntity = mainCameraEntity;
        our::CameraComponent *camera = nullptr;
        our::Entity *pistolEntity = nullptr;
        our::Mesh *pistolMesh = our::AssetLoader<our::Mesh>::get("pistol");

        if (!cameraEntity)
        {
            cameraEntity = findMainCameraEntity(mainPlayerEntity);
            mainCameraEntity = cameraEntity;
        }

        if (!mainPlayerEntity)
        {
            mainPlayerEntity = findMainPlayerEntity();
        }

        if (cameraEntity)
        {
            camera = cameraEntity->getComponent<our::CameraComponent>();
        }

        auto mainPlayerWeapon = getMainPlayerWeapon();

        for (auto entity : world.getEntities())
        {
            if (!pistolEntity && pistolMesh)
            {
                if (auto meshRenderer = entity->getComponent<our::MeshRendererComponent>();
                    meshRenderer && meshRenderer->mesh == pistolMesh)
                {
                    pistolEntity = entity;
                }
            }

            if (pistolEntity)
                break;
        }

        if (camera && cameraEntity && pistolEntity)
        {
            auto frameBufferSize = getApp()->getFrameBufferSize();
            glm::mat4 VP = camera->getProjectionMatrix(frameBufferSize) * camera->getViewMatrix();

            glm::vec3 muzzleLocalOffset = glm::vec3(-0.3f, -0.1f, 0.3f);
            glm::vec4 muzzleWorld = pistolEntity->getLocalToWorldMatrix() * glm::vec4(muzzleLocalOffset, 1.0f);
            glm::vec4 clip = VP * muzzleWorld;
            if (clip.w > 0.0001f)
            {
                glm::vec2 ndc = glm::vec2(clip) / clip.w;
                muzzleFlashCenter = glm::clamp(ndc * 0.5f + 0.5f, glm::vec2(0.0f), glm::vec2(1.0f));
            }
        }

        renderer.setMuzzleFlashCenter(muzzleFlashCenter);
        muzzleFlashTimeLeft = std::max(0.0f, muzzleFlashTimeLeft - static_cast<float>(deltaTime));
        float muzzleFlashStrength = muzzleFlashDuration > 0.0f ? (muzzleFlashTimeLeft / muzzleFlashDuration) : 0.0f;
        renderer.setMuzzleFlashStrength(muzzleFlashStrength);

        // Get a reference to the keyboard object
        auto &keyboard = getApp()->getKeyboard();

        // Debug: decrease main player health on K press
        if (keyboard.justPressed(GLFW_KEY_K))
        {
            if (auto health = getMainPlayerHealth(); health)
            {
                health->takeDamage(10.0f);
            }
        }

        float currentHealth = 100.0f;
        float maxHealth = 100.0f;
        if (auto health = getMainPlayerHealth(); health)
        {
            currentHealth = health->currentHealth;
            maxHealth = health->maxHealth;
        }
        renderer.setHealth(currentHealth, maxHealth, (float)deltaTime);

        // Here, we just run a bunch of systems to control the world logic
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);

        // Update main player weapon (handles cooldown and reload)
        if (mainPlayerWeapon)
        {
            mainPlayerWeapon->update((float)deltaTime);
        }

        // Clean up finished audio sources
        our::AudioManager::getInstance().cleanupFinishedSources();

        // And finally we use the renderer system to draw the scene
        renderer.render(&world);

        // Handle reload key (R)
        if (keyboard.justPressed(GLFW_KEY_R))
        {
            if (mainPlayerWeapon)
            {
                mainPlayerWeapon->reload();
            }
        }

        if (keyboard.justPressed(GLFW_KEY_ESCAPE))
        {
            // If the escape  key is pressed in this frame, go to the play state
            getApp()->changeState("menu");
        }
    }

    void onMouseButtonEvent(int button, int action, int mods) override
    {
        // Handle mouse button clicks for shooting
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            if (!mainCameraEntity)
            {
                mainCameraEntity = findMainCameraEntity(mainPlayerEntity);
            }

            if (!mainPlayerEntity)
            {
                mainPlayerEntity = findMainPlayerEntity();
            }

            our::CameraComponent *camera = mainCameraEntity ? mainCameraEntity->getComponent<our::CameraComponent>() : nullptr;
            our::WeaponComponent *weapon = getMainPlayerWeapon();

            if (camera && weapon)
            {
                // Attempt to shoot
                if (weapon->shoot())
                {
                    muzzleFlashTimeLeft = muzzleFlashDuration;
                    // If shot was successful, build a ray and fire it
                    our::Ray ray = shootingSystem.buildRayFromCamera(&world);
                    shootingSystem.fireRay(ray, &world, weapon);
                }
            }
        }
    }

    void onDestroy() override
    {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};