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
#include <imgui_impl/imgui_impl_opengl3.h>

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

    inline bool drawEndingOverlay(bool isWin, ImFont *endingTitleFont, ImFont *endingUiFont)
    {
        const char *loseText = "You've lost to the darkness of night";

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        float windowWidth = std::min(980.0f, displaySize.x * 0.82f);

        ImGui::SetNextWindowBgAlpha(0.20f);
        ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.07f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(windowWidth, 0.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_AlwaysAutoResize;

        bool returnToMenu = false;
        if (ImGui::Begin("EndingOverlay", nullptr, flags))
        {
            if (endingTitleFont)
                ImGui::PushFont(endingTitleFont);

            ImGui::PushStyleColor(ImGuiCol_Text, isWin ? ImVec4(1.0f, 0.96f, 0.80f, 1.0f) : ImVec4(1.0f, 0.64f, 0.64f, 1.0f));
            if (isWin)
            {
                const char *winLine1 = "Congratulations, you have survived";
                const char *winLine2 = "until dawn";

                our::ui::centerCurrentWindowText(winLine1);
                ImGui::TextUnformatted(winLine1);

                our::ui::centerCurrentWindowText(winLine2);
                ImGui::TextUnformatted(winLine2);
            }
            else
            {
                float wrapWidth = std::min(860.0f, ImGui::GetWindowWidth() - 36.0f);
                ImVec2 textSize = ImGui::CalcTextSize(loseText, nullptr, false, wrapWidth);
                float centeredTextX = std::max(8.0f, (ImGui::GetWindowWidth() - textSize.x) * 0.5f);
                ImGui::SetCursorPosX(centeredTextX);
                ImGui::PushTextWrapPos(centeredTextX + wrapWidth);
                ImGui::TextWrapped("%s", loseText);
                ImGui::PopTextWrapPos();
            }
            ImGui::PopStyleColor();

            if (endingTitleFont)
                ImGui::PopFont();

            ImGui::Spacing();

            if (endingUiFont)
                ImGui::PushFont(endingUiFont);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.20f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.28f));

            float buttonWidth = 340.0f;
            ImGui::SetCursorPosX(std::max(8.0f, (ImGui::GetWindowWidth() - buttonWidth) * 0.5f));
            if (ImGui::Button("Return to Main Menu", ImVec2(buttonWidth, 42.0f)))
            {
                returnToMenu = true;
            }

            ImGui::PopStyleColor(3);

            if (endingUiFont)
                ImGui::PopFont();
        }
        ImGui::End();

        return returnToMenu;
    }
}

class EndingState : public our::State
{
    our::World world;
    our::ForwardRenderer renderer;

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

    std::vector<our::Entity *> loseZombies;
    std::vector<glm::vec3> loseZombieFormationOffsets;
    float elapsedTime = 0.0f;
    float loseSequenceTime = 0.0f;
    float endingZombieScale = 0.42f;
    float zombieModelYawOffset = glm::pi<float>();
    float endingGroundY = -0.75f;
    glm::vec3 endingStageCenter = glm::vec3(0.0f);
    ImFont *endingTitleFont = nullptr;
    ImFont *endingUiFont = nullptr;

    ending::WinRuntime winRuntime{};
    ending::LoseRuntime loseRuntime{};

    void setupCamera()
    {
        cameraEntity = ending::findFirstCameraEntity(world);
        if (!cameraEntity)
        {
            cameraEntity = world.add();
            cameraEntity->name = "EndingCamera";
            cameraEntity->addComponent<our::CameraComponent>();
        }
    }

    void setupVisualAssets()
    {
        scenePlayerMesh = our::AssetLoader<our::Mesh>::get("main-player");
        playerMesh = scenePlayerMesh;
        playerMaterial = our::AssetLoader<our::Material>::get("auto");

        zombieMesh = our::AssetLoader<our::Mesh>::get("zombie");
        zombieMaterial = our::AssetLoader<our::Material>::get("zombie_theme");
        if (!zombieMaterial)
            zombieMaterial = our::AssetLoader<our::Material>::get("auto");

        zombieMotion = our::AssetLoader<our::Motion>::get("zombie-motion");
        if (zombieMotion)
        {
            zombieAttackClip = zombieMotion->findClip("Armature|Bite_ground");
            zombieCrawlClip = zombieMotion->findClip("Armature|Crawl");
            if (!zombieAttackClip)
                zombieAttackClip = zombieCrawlClip;
        }

        const std::string endingOliviaPath = "assets/models/olivia.glb";
        playerMotion = our::mesh_utils::loadMotion(endingOliviaPath);

        {
            our::Mesh *candidate = our::mesh_utils::loadGLB(endingOliviaPath);
            if (candidate && candidate->hasSkinning())
            {
                playerMesh = candidate;
                ownedEndingPlayerMesh = candidate;
            }
            else if (candidate)
            {
                delete candidate;
            }
        }

        if (playerMotion)
        {
            playerLoseClip = playerMotion->findClip("Die");
            playerWinClip = playerMotion->findClip("Dance");
        }

        auto &cfg = getApp()->getConfig();
        if (cfg.contains("scene") && cfg["scene"].contains("zombies"))
        {
            const auto &zCfg = cfg["scene"]["zombies"];
            float yawDeg = zCfg.value("modelYawOffsetDegrees", 180.0f);
            zombieModelYawOffset = glm::radians(yawDeg);
            endingZombieScale = std::max(0.2f, zCfg.value("modelScaleMultiplier", 0.42f) * 2.10f);
            endingGroundY = -0.5f + zCfg.value("spawnHeightOffset", -0.25f);
        }

        endingStageCenter = glm::vec3(0.0f, endingGroundY, 0.0f);

        ending::removeEntitiesByMesh(world, our::AssetLoader<our::Mesh>::get("pistol"));
    }

    void setupPlayerVisual()
    {
        if (!playerMesh)
            return;

        our::Transform referenceTransform{};
        referenceTransform.position = endingStageCenter;
        referenceTransform.scale = glm::vec3(100.0f);

        if (scenePlayerMesh)
        {
            if (auto existing = ending::findEntityByMesh(world, scenePlayerMesh))
            {
                referenceTransform = existing->localTransform;
                referenceTransform.position = endingStageCenter;
                world.markForRemoval(existing);
            }
        }

        playerVisualEntity = world.add();
        playerVisualEntity->name = "EndingMainPlayer";
        playerVisualEntity->localTransform = referenceTransform;

        auto *rendererComp = playerVisualEntity->addComponent<our::MeshRendererComponent>();
        rendererComp->mesh = playerMesh;
        rendererComp->material = playerMaterial;

        auto *skinCarrier = playerVisualEntity->addComponent<our::ZombieComponent>();
        skinCarrier->state = our::ZombieState::Walking;
        if (playerMesh->hasSkinning())
        {
            skinCarrier->skinMatrices.assign(playerMesh->getSkinJointNodes().size(), glm::mat4(1.0f));
        }

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            ending::applyLosePose(playerVisualEntity, endingGroundY, playerLoseClip);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            ending::applyWinPose(playerVisualEntity, endingGroundY, winRuntime);
        }
    }

    void setupOutcomeActors()
    {
        loseZombies.clear();
        loseZombieFormationOffsets.clear();

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            ending::setupLoseSceneActors(
                world,
                loseZombies,
                loseZombieFormationOffsets,
                playerVisualEntity,
                zombieMesh,
                zombieMaterial,
                endingGroundY,
                endingZombieScale,
                zombieModelYawOffset,
                loseRuntime);
        }
    }

    void updateOutcomeActors(float deltaTime)
    {
        elapsedTime += deltaTime;

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            loseSequenceTime += deltaTime;
            ending::updateLoseActors(
                playerVisualEntity,
                playerMesh,
                playerMotion,
                playerLoseClip,
                loseZombies,
                loseZombieFormationOffsets,
                zombieMesh,
                zombieMotion,
                zombieAttackClip,
                zombieCrawlClip,
                loseSequenceTime,
                endingGroundY,
                zombieModelYawOffset,
                loseRuntime);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            ending::updateWinActors(
                playerVisualEntity,
                playerMesh,
                playerMotion,
                playerWinClip,
                elapsedTime,
                endingGroundY,
                winRuntime);
        }
    }

    void updateOutcomeCamera()
    {
        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            ending::updateLoseCamera(
                cameraEntity,
                playerVisualEntity,
                playerLoseClip,
                zombieAttackClip,
                zombieCrawlClip,
                loseSequenceTime,
                loseRuntime);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            ending::updateWinCamera(cameraEntity, playerVisualEntity, elapsedTime, winRuntime);
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
        ending::clearLocalOccludersAroundStage(world, endingStageCenter, playerMesh, scenePlayerMesh, zombieMesh);
        our::ui::loadPreferredSerifFonts(endingTitleFont, endingUiFont, 46.0f, 28.0f);
        setupPlayerVisual();
        setupOutcomeActors();

        world.deleteMarkedEntities();

        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        renderer.setOverlaysVisible(false);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.setHealth(100.0f, 100.0f, 0.0f);

        elapsedTime = 0.0f;
        loseSequenceTime = 0.0f;

        ending::cacheStartCameraPose(winRuntime, cameraEntity);

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            loseRuntime.zombieBiteStart = ending::computeZombieBiteStart(loseRuntime, playerLoseClip);
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

        renderer.setTime(elapsedTime);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.render(&world);
    }

    void onImmediateGui() override
    {
        const bool isWin = our::GameSession::endingOutcome == our::EndingOutcome::Win;
        if (ending::drawEndingOverlay(isWin, endingTitleFont, endingUiFont))
            getApp()->changeState("menu");
    }

    void onDestroy() override
    {
        renderer.destroy();
        world.clear();
        our::clearAllAssets();

        delete playerMotion;
        playerMotion = nullptr;

        if (ownedEndingPlayerMesh)
        {
            delete ownedEndingPlayerMesh;
            ownedEndingPlayerMesh = nullptr;
        }

        loseZombies.clear();
        cameraEntity = nullptr;
        playerVisualEntity = nullptr;
    }
};
