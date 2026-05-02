#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/scene-manager.hpp>
#include <components/camera.hpp>
#include <components/mesh-renderer.hpp>
#include <components/zombie.hpp>
#include <animation/motion.hpp>
#include <asset-loader.hpp>
#include <mesh/mesh-utils.hpp>
#include <game-session.hpp>
#include <ui/ui-theme.hpp>

#include "ending/ending-win.hpp"
#include "ending/ending-lose.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace ending
{
    inline std::string lowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    inline glm::vec3 getEntityWorldPosition(our::Entity *entity)
    {
        if (!entity)
            return glm::vec3(0.0f);
        return glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
    }

    inline void applyClipToSkinnedEntity(our::Entity *entity, our::Mesh *mesh, our::Motion *motion, const our::MotionClip *clip, float localTime)
    {
        if (!(entity && mesh && motion && clip && mesh->hasSkinning()))
            return;

        auto *skinCarrier = entity->getComponent<our::ZombieComponent>();
        if (!skinCarrier)
            return;

        motion->computeSkinMatrices(
            clip,
            localTime,
            mesh->getSkinJointNodes(),
            mesh->getInverseBindMatrices(),
            skinCarrier->skinMatrices);
    }

    inline our::Entity *findFirstCameraEntity(our::World &world)
    {
        for (auto entity : world.getEntities())
        {
            if (entity->getComponent<our::CameraComponent>())
                return entity;
        }
        return nullptr;
    }

    inline our::Entity *findEntityByMesh(our::World &world, our::Mesh *mesh)
    {
        if (!mesh)
            return nullptr;
        for (auto entity : world.getEntities())
        {
            auto *rendererComp = entity->getComponent<our::MeshRendererComponent>();
            if (rendererComp && rendererComp->mesh == mesh)
                return entity;
        }
        return nullptr;
    }

    inline void removeEntitiesByMesh(our::World &world, our::Mesh *mesh)
    {
        if (!mesh)
            return;
        for (auto entity : world.getEntities())
        {
            auto *rendererComp = entity->getComponent<our::MeshRendererComponent>();
            if (rendererComp && rendererComp->mesh == mesh)
            {
                world.markForRemoval(entity);
            }
        }
    }

    inline bool isGroundLikeEntityName(const std::string &lowerName)
    {
        return lowerName.find("floor") != std::string::npos ||
               lowerName.find("street") != std::string::npos ||
               lowerName.find("road") != std::string::npos ||
               lowerName.find("ground") != std::string::npos;
    }

    inline void clearLocalOccludersAroundStage(
        our::World &world,
        const glm::vec3 &center,
        our::Mesh *playerMesh,
        our::Mesh *scenePlayerMesh,
        our::Mesh *zombieMesh)
    {
        int removedCount = 0;
        const int maxRemoved = 6;

        for (auto entity : world.getEntities())
        {
            auto *rendererComp = entity->getComponent<our::MeshRendererComponent>();
            if (!rendererComp)
                continue;

            our::Mesh *mesh = rendererComp->mesh;
            if (mesh == playerMesh || mesh == scenePlayerMesh || mesh == zombieMesh || mesh == our::AssetLoader<our::Mesh>::get("pistol"))
                continue;

            std::string lowerName = ending::lowerCopy(entity->name);
            if (isGroundLikeEntityName(lowerName))
                continue;

            glm::vec3 worldPos = ending::getEntityWorldPosition(entity);
            glm::vec2 delta = glm::vec2(worldPos.x - center.x, worldPos.z - center.z);
            float distXZ = glm::length(delta);

            bool likelyOccluder = distXZ < 10.0f && worldPos.y > center.y + 0.8f;
            if (likelyOccluder)
            {
                world.markForRemoval(entity);
                removedCount++;
                if (removedCount >= maxRemoved)
                    break;
            }
        }
    }

}

class EndingState : public our::State
{
    our::World world;
    our::ForwardRenderer renderer;

    struct SceneAssets
    {
        our::Entity *cameraEntity = nullptr;
        our::Entity *playerVisualEntity = nullptr;

        our::Mesh *scenePlayerMesh = nullptr;
        our::Mesh *playerMesh = nullptr;
        our::Material *playerMaterial = nullptr;
        our::Mesh *ownedEndingPlayerMesh = nullptr;

        our::Mesh *zombieMesh = nullptr;
        our::Material *zombieMaterial = nullptr;

        our::Motion *zombieMotion = nullptr;
        our::Motion *playerMotion = nullptr;

        const our::MotionClip *zombieCrawlClip = nullptr;
        const our::MotionClip *zombieAttackClip = nullptr;
        const our::MotionClip *playerLoseClip = nullptr;
        const our::MotionClip *playerWinClip = nullptr;
    } scene;

    struct RuntimeState
    {
        std::vector<our::Entity *> loseZombies;
        std::vector<glm::vec3> loseZombieFormationOffsets;
        float elapsedTime = 0.0f;
        float loseSequenceTime = 0.0f;
        float endingZombieScale = 0.42f;
        float zombieModelYawOffset = glm::pi<float>();
        float endingGroundY = -0.75f;
        glm::vec3 endingStageCenter = glm::vec3(0.0f);
        our::ui::ending::Assets uiAssets{};
        ending::WinRuntime winRuntime{};
        ending::LoseRuntime loseRuntime{};
    } runtime;

    void setupCamera()
    {
        scene.cameraEntity = ending::findFirstCameraEntity(world);
        if (!scene.cameraEntity)
        {
            scene.cameraEntity = world.add();
            scene.cameraEntity->name = "EndingCamera";
            scene.cameraEntity->addComponent<our::CameraComponent>();
        }
    }

    void setupVisualAssets()
    {
        scene.scenePlayerMesh = our::AssetLoader<our::Mesh>::get("main-player");
        scene.playerMesh = scene.scenePlayerMesh;
        scene.playerMaterial = our::AssetLoader<our::Material>::get("auto");

        scene.zombieMesh = our::AssetLoader<our::Mesh>::get("zombie");
        scene.zombieMaterial = our::AssetLoader<our::Material>::get("zombie_theme");
        if (!scene.zombieMaterial)
            scene.zombieMaterial = our::AssetLoader<our::Material>::get("auto");

        scene.zombieMotion = our::AssetLoader<our::Motion>::get("zombie-motion");
        if (scene.zombieMotion)
        {
            scene.zombieAttackClip = scene.zombieMotion->findClip("Armature|Bite_ground");
            scene.zombieCrawlClip = scene.zombieMotion->findClip("Armature|Crawl");
            if (!scene.zombieAttackClip)
                scene.zombieAttackClip = scene.zombieCrawlClip;
        }

        const std::string endingOliviaPath = "assets/models/olivia.glb";
        scene.playerMotion = our::mesh_utils::loadMotion(endingOliviaPath);

        {
            our::Mesh *candidate = our::mesh_utils::loadGLB(endingOliviaPath);
            if (candidate && candidate->hasSkinning())
            {
                scene.playerMesh = candidate;
                scene.ownedEndingPlayerMesh = candidate;
            }
            else if (candidate)
            {
                delete candidate;
            }
        }

        if (scene.playerMotion)
        {
            scene.playerLoseClip = scene.playerMotion->findClip("Die");
            scene.playerWinClip = scene.playerMotion->findClip("Salute.001");
        }

        auto &cfg = getApp()->getConfig();
        if (cfg.contains("scene") && cfg["scene"].contains("zombies"))
        {
            const auto &zCfg = cfg["scene"]["zombies"];
            float yawDeg = zCfg.value("modelYawOffsetDegrees", 180.0f);
            runtime.zombieModelYawOffset = glm::radians(yawDeg);
            runtime.endingZombieScale = std::max(0.2f, zCfg.value("modelScaleMultiplier", 0.42f) * 0.42f);
            runtime.endingGroundY = -0.5f + zCfg.value("spawnHeightOffset", -0.25f);
        }

        if (cfg.contains("scene") && cfg["scene"].contains("ending"))
        {
            const auto &endingCfg = cfg["scene"]["ending"];
            if (endingCfg.contains("win"))
            {
                const auto &winCfg = endingCfg["win"];
                if (winCfg.contains("playerPositionOffset"))
                    runtime.winRuntime.playerPositionOffset = winCfg["playerPositionOffset"].get<glm::vec3>();
                if (winCfg.contains("playerRotationDegrees"))
                    runtime.winRuntime.playerRotationDegrees = winCfg["playerRotationDegrees"].get<glm::vec3>();
                if (winCfg.contains("cameraTargetOffset"))
                    runtime.winRuntime.cameraTargetOffset = winCfg["cameraTargetOffset"].get<glm::vec3>();
                if (winCfg.contains("playerGroundOffset"))
                    runtime.winRuntime.playerGroundOffset = winCfg.value("playerGroundOffset", runtime.winRuntime.playerGroundOffset);
                if (winCfg.contains("zoomTargetDistance"))
                    runtime.winRuntime.zoomTargetDistance = winCfg.value("zoomTargetDistance", runtime.winRuntime.zoomTargetDistance);
                if (winCfg.contains("zoomHeightOffset"))
                    runtime.winRuntime.zoomHeightOffset = winCfg.value("zoomHeightOffset", runtime.winRuntime.zoomHeightOffset);
                if (winCfg.contains("zoomDuration"))
                    runtime.winRuntime.zoomDuration = winCfg.value("zoomDuration", runtime.winRuntime.zoomDuration);
            }
        }

        runtime.endingStageCenter = glm::vec3(0.0f, runtime.endingGroundY, 0.0f);

        // Ending scenes should not show gameplay pickups/weapons.
        ending::removeEntitiesByMesh(world, our::AssetLoader<our::Mesh>::get("pistol"));
        ending::removeEntitiesByMesh(world, our::AssetLoader<our::Mesh>::get("rifle"));
        ending::removeEntitiesByMesh(world, our::AssetLoader<our::Mesh>::get("pickup-health"));
    }

    void setupPlayerVisual()
    {
        if (!scene.playerMesh)
            return;

        our::Transform referenceTransform{};
        referenceTransform.position = runtime.endingStageCenter;
        referenceTransform.scale = glm::vec3(100.0f);

        if (scene.scenePlayerMesh)
        {
            if (auto existing = ending::findEntityByMesh(world, scene.scenePlayerMesh))
            {
                referenceTransform = existing->localTransform;
                referenceTransform.position = runtime.endingStageCenter;
                world.markForRemoval(existing);
            }
        }

        scene.playerVisualEntity = world.add();
        scene.playerVisualEntity->name = "EndingMainPlayer";
        scene.playerVisualEntity->localTransform = referenceTransform;

        auto *rendererComp = scene.playerVisualEntity->addComponent<our::MeshRendererComponent>();
        rendererComp->mesh = scene.playerMesh;
        rendererComp->material = scene.playerMaterial;

        auto *skinCarrier = scene.playerVisualEntity->addComponent<our::ZombieComponent>();
        skinCarrier->state = our::ZombieState::Walking;
        if (scene.playerMesh->hasSkinning())
        {
            skinCarrier->skinMatrices.assign(scene.playerMesh->getSkinJointNodes().size(), glm::mat4(1.0f));
        }

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            ending::applyLosePose(scene.playerVisualEntity, runtime.endingGroundY, scene.playerLoseClip);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            ending::applyWinPose(scene.playerVisualEntity, runtime.endingGroundY, runtime.endingStageCenter, runtime.winRuntime);
        }
    }

    void setupOutcomeActors()
    {
        runtime.loseZombies.clear();
        runtime.loseZombieFormationOffsets.clear();

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            ending::setupLoseSceneActors(
                world,
                runtime.loseZombies,
                runtime.loseZombieFormationOffsets,
                scene.playerVisualEntity,
                scene.zombieMesh,
                scene.zombieMaterial,
                runtime.endingGroundY,
                runtime.endingZombieScale,
                runtime.zombieModelYawOffset,
                runtime.loseRuntime);
        }
    }

    void updateOutcomeActors(float deltaTime)
    {
        runtime.elapsedTime += deltaTime;

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            runtime.loseSequenceTime += deltaTime;
            ending::updateLoseActors(
                scene.playerVisualEntity,
                scene.playerMesh,
                scene.playerMotion,
                scene.playerLoseClip,
                runtime.loseZombies,
                runtime.loseZombieFormationOffsets,
                scene.zombieMesh,
                scene.zombieMotion,
                scene.zombieAttackClip,
                scene.zombieCrawlClip,
                runtime.loseSequenceTime,
                runtime.endingGroundY,
                runtime.zombieModelYawOffset,
                runtime.loseRuntime);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            ending::updateWinActors(
                scene.playerVisualEntity,
                scene.playerMesh,
                scene.playerMotion,
                scene.playerWinClip,
                runtime.elapsedTime,
                runtime.endingGroundY,
                runtime.endingStageCenter,
                runtime.winRuntime);
        }
    }

    void updateOutcomeCamera()
    {
        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            ending::updateLoseCamera(
                scene.cameraEntity,
                scene.playerVisualEntity,
                scene.playerLoseClip,
                scene.zombieAttackClip,
                scene.zombieCrawlClip,
                runtime.loseSequenceTime,
                runtime.loseRuntime);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            ending::updateWinCamera(scene.cameraEntity, scene.playerVisualEntity, runtime.elapsedTime, runtime.endingStageCenter, runtime.winRuntime);
        }
    }

public:
    void onInitialize() override
    {
        auto &config = getApp()->getConfig()["scene"];
        if (config.contains("assets"))
        {
            our::deserializeAllAssets(config["assets"]);
        }
        if (config.contains("world"))
        {
            world.deserialize(config["world"]);
        }

        our::SceneManager::validateWorld(&world);

        setupCamera();
        setupVisualAssets();
        ending::clearLocalOccludersAroundStage(world, runtime.endingStageCenter, scene.playerMesh, scene.scenePlayerMesh, scene.zombieMesh);
        runtime.uiAssets.loadDefaultThemeResources();
        setupPlayerVisual();
        setupOutcomeActors();

        world.deleteMarkedEntities();

        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        renderer.setOverlaysVisible(false);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.setHealth(100.0f, 100.0f, 0.0f);

        runtime.elapsedTime = 0.0f;
        runtime.loseSequenceTime = 0.0f;

        ending::cacheStartCameraPose(runtime.winRuntime, scene.cameraEntity);

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            runtime.loseRuntime.zombieBiteStart = ending::computeZombieBiteStart(runtime.loseRuntime, scene.playerLoseClip);
        }
    }

    void onDraw(double deltaTime) override
    {
        auto &keyboard = getApp()->getKeyboard();
        if (keyboard.justPressed(GLFW_KEY_ESCAPE) || keyboard.justPressed(GLFW_KEY_ENTER))
        {
            getApp()->changeState("menu");
            return;
        }

        updateOutcomeActors(static_cast<float>(deltaTime));
        updateOutcomeCamera();

        renderer.setTime(runtime.elapsedTime);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.render(&world);
    }

    void onImmediateGui() override
    {
        const bool isWin = our::GameSession::endingOutcome == our::EndingOutcome::Win;
        if (our::ui::ending::drawOverlay(isWin, runtime.uiAssets))
            getApp()->changeState("menu");
    }

    void onDestroy() override
    {
        renderer.destroy();
        world.clear();
        our::clearAllAssets();

        delete scene.playerMotion;
        scene.playerMotion = nullptr;

        if (scene.ownedEndingPlayerMesh)
        {
            delete scene.ownedEndingPlayerMesh;
            scene.ownedEndingPlayerMesh = nullptr;
        }

        runtime.uiAssets.destroy();

        runtime.loseZombies.clear();
        scene.cameraEntity = nullptr;
        scene.playerVisualEntity = nullptr;
    }
};
