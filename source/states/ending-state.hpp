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
#include <imgui_impl/imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

class EndingState : public our::State
{
    our::World world;
    our::ForwardRenderer renderer;

    our::Entity *cameraEntity = nullptr;
    our::Entity *playerVisualEntity = nullptr;
    our::Mesh *playerMesh = nullptr;
    our::Material *playerMaterial = nullptr;

    our::Mesh *zombieMesh = nullptr;
    our::Material *zombieMaterial = nullptr;

    our::Motion *zombieMotion = nullptr;
    our::Motion *playerMotion = nullptr; // Loaded manually from Olivia GLB for ending-only animations.

    const our::MotionClip *zombieCrawlClip = nullptr;
    const our::MotionClip *zombieAttackClip = nullptr;
    const our::MotionClip *playerLoseClip = nullptr;
    const our::MotionClip *playerStandClip = nullptr;

    std::vector<our::Entity *> loseZombies;
    float elapsedTime = 0.0f;
    ImFont *endingTitleFont = nullptr;
    ImFont *endingUiFont = nullptr;

    void loadEndingFonts()
    {
        if (endingTitleFont && endingUiFont)
            return;

        ImGuiIO &io = ImGui::GetIO();
        bool addedCustomFont = false;

        // Prefer serif fonts for a more cinematic / grand ending presentation.
        const std::array<const char *, 5> serifCandidates = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSerif-Bold.ttf",
            "/usr/share/fonts/truetype/noto/NotoSerif-Bold.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSerifBold.ttf",
            "assets/fonts/Cinzel-Bold.ttf"};

        for (const char *path : serifCandidates)
        {
            if (std::filesystem::exists(path))
            {
                endingTitleFont = io.Fonts->AddFontFromFileTTF(path, 46.0f);
                endingUiFont = io.Fonts->AddFontFromFileTTF(path, 28.0f);
                addedCustomFont = (endingTitleFont != nullptr) && (endingUiFont != nullptr);
                break;
            }
        }

        if (addedCustomFont)
        {
            // We add fonts during runtime (state initialize), so we must rebuild atlas and refresh
            // renderer font texture before the next ImGui frame to avoid backend assertions.
            io.Fonts->Build();
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }

        // Fallback safely to default ImGui font if no serif font is available.
        if (!endingTitleFont)
            endingTitleFont = io.FontDefault;
        if (!endingUiFont)
            endingUiFont = io.FontDefault;
    }

    our::Entity *findFirstCameraEntity()
    {
        for (auto entity : world.getEntities())
        {
            if (entity->getComponent<our::CameraComponent>())
                return entity;
        }
        return nullptr;
    }

    our::Entity *findEntityByMesh(our::Mesh *mesh)
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

    static glm::vec3 getEntityWorldPosition(our::Entity *entity)
    {
        if (!entity)
            return glm::vec3(0.0f);
        return glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
    }

    static float positiveModulo(float value, float period)
    {
        if (period <= 0.0f)
            return 0.0f;
        float wrapped = std::fmod(value, period);
        if (wrapped < 0.0f)
            wrapped += period;
        return wrapped;
    }

    static void lookAtOnGround(our::Entity *entity, const glm::vec3 &target)
    {
        if (!entity)
            return;
        glm::vec3 position = getEntityWorldPosition(entity);
        glm::vec3 toTarget = target - position;
        toTarget.y = 0.0f;
        if (glm::dot(toTarget, toTarget) < 0.0001f)
            return;

        glm::vec3 direction = glm::normalize(toTarget);
        float yaw = std::atan2(-direction.x, -direction.z);
        entity->localTransform.rotation.y = yaw;
    }

    void setupCamera()
    {
        cameraEntity = findFirstCameraEntity();
        if (!cameraEntity)
        {
            cameraEntity = world.add();
            cameraEntity->name = "EndingCamera";
            cameraEntity->addComponent<our::CameraComponent>();
        }
    }

    void setupVisualAssets()
    {
        playerMesh = our::AssetLoader<our::Mesh>::get("main-player");
        playerMaterial = our::AssetLoader<our::Material>::get("auto");

        zombieMesh = our::AssetLoader<our::Mesh>::get("zombie");
        zombieMaterial = our::AssetLoader<our::Material>::get("zombie_theme");
        if (!zombieMaterial)
            zombieMaterial = our::AssetLoader<our::Material>::get("auto");

        zombieMotion = our::AssetLoader<our::Motion>::get("zombie-motion");
        if (zombieMotion)
        {
            zombieAttackClip = zombieMotion->findClipByKeywords({"bite_ground", "bite ground", "bite"});
            zombieCrawlClip = zombieMotion->findClipByKeywords({"crawl", "running_crawl", "craw"});
            if (!zombieAttackClip)
                zombieAttackClip = zombieCrawlClip;
        }

        // Player mesh and player motion are separate assets in this project setup.
        playerMotion = our::mesh_utils::loadMotion("assets/models/Olivia.glb");
        if (playerMotion)
        {
            playerLoseClip = playerMotion->findClipByKeywords({"lay", "ground", "die", "death", "fall"});
            playerStandClip = playerMotion->findClipByKeywords({"idle", "stand", "breathe", "relax"});
        }
    }

    void setupPlayerVisual()
    {
        if (!playerMesh)
            return;

        our::Transform referenceTransform{};
        referenceTransform.position = glm::vec3(6.0f, 4.0f, 10.0f);
        referenceTransform.scale = glm::vec3(100.0f);

        if (auto existing = findEntityByMesh(playerMesh))
        {
            referenceTransform = existing->localTransform;
            world.markForRemoval(existing);
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
            playerVisualEntity->localTransform.position.y -= 3.2f;
            // Fallback floor pose in case player GLB has no suitable clip.
            if (!playerLoseClip)
                playerVisualEntity->localTransform.rotation.x = glm::radians(-88.0f);
        }
    }

    our::Entity *spawnZombieActor(const glm::vec3 &position, float yaw)
    {
        if (!(zombieMesh && zombieMaterial))
            return nullptr;

        our::Entity *entity = world.add();
        entity->name = "EndingZombie";
        entity->localTransform.position = position;
        entity->localTransform.rotation = glm::vec3(0.0f, yaw, 0.0f);
        entity->localTransform.scale = glm::vec3(0.42f);

        auto *rendererComp = entity->addComponent<our::MeshRendererComponent>();
        rendererComp->mesh = zombieMesh;
        rendererComp->material = zombieMaterial;

        auto *zombieComp = entity->addComponent<our::ZombieComponent>();
        zombieComp->state = our::ZombieState::Crawling;
        zombieComp->shotsTaken = 1;
        if (zombieMesh->hasSkinning())
        {
            zombieComp->skinMatrices.assign(zombieMesh->getSkinJointNodes().size(), glm::mat4(1.0f));
        }

        return entity;
    }

    void setupLoseSceneActors()
    {
        loseZombies.clear();
        if (our::GameSession::endingOutcome != our::EndingOutcome::Lose || !playerVisualEntity)
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;
        std::array<glm::vec3, 3> offsets = {
            glm::vec3(2.0f, 0.0f, 1.2f),
            glm::vec3(-1.8f, 0.0f, 1.0f),
            glm::vec3(0.0f, 0.0f, -2.2f)};

        for (const auto &offset : offsets)
        {
            glm::vec3 spawnPos = playerPos + offset;
            spawnPos.y = -0.5f;
            glm::vec3 toPlayer = playerPos - spawnPos;
            float yaw = std::atan2(-toPlayer.x, -toPlayer.z);
            if (auto *zombieEntity = spawnZombieActor(spawnPos, yaw))
            {
                loseZombies.push_back(zombieEntity);
            }
        }
    }

    void applyClipToSkinnedEntity(our::Entity *entity, our::Mesh *mesh, our::Motion *motion, const our::MotionClip *clip, float localTime)
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

        // Important: don't add root rotation to entity transform every frame.
        // That causes cumulative spinning over time.
    }

    void updateCamera(float deltaTime)
    {
        if (!(cameraEntity && playerVisualEntity))
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;

        if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            const float radius = 7.5f;
            const float height = 4.2f;
            const float angularSpeed = 0.65f;

            float angle = elapsedTime * angularSpeed;
            cameraEntity->localTransform.position = playerPos + glm::vec3(std::sin(angle) * radius, height, std::cos(angle) * radius);
            lookAtOnGround(cameraEntity, playerPos);
            cameraEntity->localTransform.rotation.x = glm::radians(-18.0f);
        }
        else
        {
            cameraEntity->localTransform.position = playerPos + glm::vec3(0.0f, 2.6f, 6.2f);
            lookAtOnGround(cameraEntity, playerPos);
            cameraEntity->localTransform.rotation.x = glm::radians(-14.0f);
        }
    }

    void updateEndingActors(float deltaTime)
    {
        elapsedTime += deltaTime;

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            if (playerLoseClip)
            {
                float playerTime = positiveModulo(elapsedTime, std::max(0.01f, playerLoseClip->duration));
                applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerLoseClip, playerTime);
            }

            for (size_t i = 0; i < loseZombies.size(); ++i)
            {
                auto *zombieEntity = loseZombies[i];
                if (!zombieEntity)
                    continue;

                const our::MotionClip *clip = (i % 2 == 0) ? zombieAttackClip : zombieCrawlClip;
                if (!clip)
                    clip = zombieAttackClip ? zombieAttackClip : zombieCrawlClip;
                if (!clip)
                    continue;

                float phaseOffset = 0.35f * static_cast<float>(i);
                float zombieTime = positiveModulo(elapsedTime + phaseOffset, std::max(0.01f, clip->duration));
                applyClipToSkinnedEntity(zombieEntity, zombieMesh, zombieMotion, clip, zombieTime);
            }
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            if (playerStandClip)
            {
                float playerTime = positiveModulo(elapsedTime, std::max(0.01f, playerStandClip->duration));
                applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerStandClip, playerTime);
            }
            else if (playerVisualEntity)
            {
                playerVisualEntity->localTransform.rotation.x = 0.0f;
            }
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
        loadEndingFonts();
        setupPlayerVisual();
        setupLoseSceneActors();

        world.deleteMarkedEntities();

        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.setHealth(100.0f, 100.0f, 0.0f);

        elapsedTime = 0.0f;
    }

    void onDraw(double deltaTime) override
    {
        auto &keyboard = getApp()->getKeyboard();
        if (keyboard.justPressed(GLFW_KEY_ESCAPE) || keyboard.justPressed(GLFW_KEY_ENTER))
        {
            getApp()->changeState("menu");
            return;
        }

        updateEndingActors(static_cast<float>(deltaTime));
        updateCamera(static_cast<float>(deltaTime));

        renderer.setTime(elapsedTime);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.render(&world);
    }

    void onImmediateGui() override
    {
        const bool isWin = our::GameSession::endingOutcome == our::EndingOutcome::Win;
        const char *endingText = isWin
                                     ? "Congratulations, you have survived until dawn"
                                     : "You've lost to the darkness of night";

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        float windowWidth = std::min(980.0f, displaySize.x * 0.82f);

        ImGui::SetNextWindowBgAlpha(0.20f);
        ImGui::SetNextWindowPos(ImVec2(displaySize.x * 0.5f, displaySize.y * 0.07f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(windowWidth, 0.0f), ImGuiCond_Always);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_AlwaysAutoResize;

        if (ImGui::Begin("EndingOverlay", nullptr, flags))
        {
            if (endingTitleFont)
                ImGui::PushFont(endingTitleFont);

            ImGui::PushStyleColor(ImGuiCol_Text, isWin ? ImVec4(1.0f, 0.96f, 0.80f, 1.0f) : ImVec4(1.0f, 0.64f, 0.64f, 1.0f));
            float wrapWidth = std::min(860.0f, ImGui::GetWindowWidth() - 36.0f);
            ImVec2 textSize = ImGui::CalcTextSize(endingText, nullptr, false, wrapWidth);
            float centeredTextX = std::max(8.0f, (ImGui::GetWindowWidth() - textSize.x) * 0.5f);
            ImGui::SetCursorPosX(centeredTextX);
            ImGui::PushTextWrapPos(centeredTextX + wrapWidth);
            ImGui::TextWrapped("%s", endingText);
            ImGui::PopTextWrapPos();
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
                getApp()->changeState("menu");
            }

            ImGui::PopStyleColor(3);

            if (endingUiFont)
                ImGui::PopFont();
        }
        ImGui::End();
    }

    void onDestroy() override
    {
        renderer.destroy();
        world.clear();
        our::clearAllAssets();

        delete playerMotion;
        playerMotion = nullptr;

        loseZombies.clear();
        cameraEntity = nullptr;
        playerVisualEntity = nullptr;
    }
};
