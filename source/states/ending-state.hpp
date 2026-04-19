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
#include <algorithm>
#include <array>
#include <cctype>
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
    our::Mesh *scenePlayerMesh = nullptr;
    our::Mesh *playerMesh = nullptr;
    our::Material *playerMaterial = nullptr;
    our::Mesh *ownedEndingPlayerMesh = nullptr;

    our::Mesh *zombieMesh = nullptr;
    our::Material *zombieMaterial = nullptr;

    our::Motion *zombieMotion = nullptr;
    our::Motion *playerMotion = nullptr; // Loaded manually from Olivia GLB for ending-only animations.

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

    // Lose scene timings (seconds)
    float losePlayerDeathBlendIn = 0.30f;
    float losePlayerDeathHoldFraction = 0.78f;
    float loseZombieBiteStart = 0.95f;
    float loseZombieBiteClipFraction = 0.72f; // stay on floor portion only
    float loseZombieBiteDelayAfterDeath = 0.35f;
    float losePostActionHold = 1.80f;
    float loseSkyTurnDuration = 4.50f;
    glm::vec3 loseZombieCircleCenterOffset = glm::vec3(0.0f, 0.0f, -0.28f);
    float loseZombieGroundOffset = -0.42f;
    float winPlayerGroundOffset = -0.58f;
    float winCameraBlendInDuration = 2.2f;
    glm::vec3 winCameraStartPosition = glm::vec3(0.0f);
    float winCameraStartYaw = 0.0f;
    float winCameraStartPitch = 0.0f;

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

    void removeEntitiesByMesh(our::Mesh *mesh)
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

    static std::string lowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    const our::MotionClip *pickPlayerDeathClip() const
    {
        if (!(playerMotion && !playerMotion->clips.empty()))
            return nullptr;

        const std::array<std::string, 10> positive = {"die", "death", "dead", "fall", "down", "defeat", "knock", "hurt", "lay", "ground"};
        const std::array<std::string, 8> negative = {"idle", "stand", "walk", "run", "breathe", "crawl", "bite", "attack"};

        const our::MotionClip *best = nullptr;
        float bestScore = -1e9f;
        for (const auto &clip : playerMotion->clips)
        {
            std::string name = lowerCopy(clip.name);
            float score = 0.0f;

            for (const auto &k : positive)
            {
                if (name.find(k) != std::string::npos)
                    score += 5.0f;
            }
            for (const auto &k : negative)
            {
                if (name.find(k) != std::string::npos)
                    score -= 4.0f;
            }

            score += std::min(clip.duration, 10.0f) * 0.2f;

            if (score > bestScore)
            {
                bestScore = score;
                best = &clip;
            }
        }

        return best;
    }

    void clearLocalOccludersAroundStage(const glm::vec3 &center)
    {
        // Keep world components, but remove a few blocking meshes around the stage only.
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

            std::string lowerName = lowerCopy(entity->name);
            if (lowerName.find("floor") != std::string::npos ||
                lowerName.find("street") != std::string::npos ||
                lowerName.find("road") != std::string::npos ||
                lowerName.find("ground") != std::string::npos)
            {
                continue;
            }

            glm::vec3 worldPos = getEntityWorldPosition(entity);
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

    void orientZombieTowardPlayer(our::Entity *zombie)
    {
        if (!(zombie && playerVisualEntity))
            return;

        glm::vec3 playerPos = getEntityWorldPosition(playerVisualEntity);
        glm::vec3 zombiePos = getEntityWorldPosition(zombie);
        glm::vec3 toPlayer = playerPos - zombiePos;
        toPlayer.y = 0.0f;
        if (glm::dot(toPlayer, toPlayer) < 0.0001f)
            return;

        float yaw = std::atan2(-toPlayer.x, -toPlayer.z) + zombieModelYawOffset;
        zombie->localTransform.rotation.y = yaw;
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
            zombieAttackClip = zombieMotion->findClipByKeywords({"bite_ground", "bite ground", "bite"});
            zombieCrawlClip = zombieMotion->findClipByKeywords({"crawl", "running_crawl", "craw"});
            if (!zombieAttackClip)
                zombieAttackClip = zombieCrawlClip;
        }

        // Use lowercase olivia.glb exclusively for ending cinematics.
        const std::string endingOliviaPath = "assets/models/olivia.glb";

        playerMotion = our::mesh_utils::loadMotion(endingOliviaPath);

        // Use the same file for skinned mesh + motion so clip skeleton always matches.
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
            // Hard-priority exact die clip names first (Olivia has one).
            playerLoseClip = playerMotion->findClip("die");
            if (!playerLoseClip)
                playerLoseClip = playerMotion->findClip("Die");
            if (!playerLoseClip)
                playerLoseClip = playerMotion->findClip("Armature|Die");
            if (!playerLoseClip)
                playerLoseClip = playerMotion->findClipByKeywords({"|die", " die", "die", "death", "fall", "lay", "ground"});
            if (!playerLoseClip)
                playerLoseClip = pickPlayerDeathClip();

            // For win ending: prefer a dance celebration from Olivia clips.
            playerWinClip = playerMotion->findClip("dance");
            if (!playerWinClip)
                playerWinClip = playerMotion->findClip("Dance");
            if (!playerWinClip)
                playerWinClip = playerMotion->findClipByKeywords({"dance", "celebrate", "victory", "win"});
            // Safe fallback if no dance-like clip exists in the GLB.
            if (!playerWinClip)
                playerWinClip = playerMotion->findClipByKeywords({"idle", "stand", "breathe", "relax"});

            // If fallback still resolved to idle for any reason, retry stricter death keywords only.
            if (playerLoseClip)
            {
                std::string loseName = lowerCopy(playerLoseClip->name);
                if (loseName.find("idle") != std::string::npos || loseName.find("stand") != std::string::npos)
                {
                    playerLoseClip = playerMotion->findClipByKeywords({"die", "death", "fall", "lay", "ground"});
                }
            }
        }

        // Match zombie front direction with the gameplay scene convention.
        auto &cfg = getApp()->getConfig();
        if (cfg.contains("scene") && cfg["scene"].contains("zombies"))
        {
            const auto &zCfg = cfg["scene"]["zombies"];
            float yawDeg = zCfg.value("modelYawOffsetDegrees", 180.0f);
            zombieModelYawOffset = glm::radians(yawDeg);
            // Slightly boost zombie scale in ending scene so proportions read better next to Olivia.
            endingZombieScale = std::max(0.2f, zCfg.value("modelScaleMultiplier", 0.42f) * 2.10f);
            endingGroundY = -0.5f + zCfg.value("spawnHeightOffset", -0.25f);
        }

        endingStageCenter = glm::vec3(0.0f, endingGroundY, 0.0f);

        // Hide first-person pistol in ending scenes.
        removeEntitiesByMesh(our::AssetLoader<our::Mesh>::get("pistol"));
    }

    void setupPlayerVisual()
    {
        if (!playerMesh)
            return;

        our::Transform referenceTransform{};
        // Use an unobstructed staging area for ending cinematics.
        referenceTransform.position = endingStageCenter;
        referenceTransform.scale = glm::vec3(100.0f);

        if (scenePlayerMesh)
        {
            if (auto existing = findEntityByMesh(scenePlayerMesh))
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
            playerVisualEntity->localTransform.position.y = endingGroundY;
            // Keep Olivia oriented consistently in the lose cinematic.
            playerVisualEntity->localTransform.rotation.y = glm::radians(180.0f);
            // Fallback floor pose in case player GLB has no suitable clip.
            if (!playerLoseClip)
                playerVisualEntity->localTransform.rotation.x = glm::radians(-88.0f);
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            // Keep Olivia grounded in the winning shot.
            playerVisualEntity->localTransform.position.y = endingGroundY + winPlayerGroundOffset;
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
        entity->localTransform.scale = glm::vec3(endingZombieScale);

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
        loseZombieFormationOffsets.clear();
        if (our::GameSession::endingOutcome != our::EndingOutcome::Lose || !playerVisualEntity)
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;

        // Keep zombies close and centered around Olivia.
        float ringR = 1.22f;
        std::array<glm::vec3, 3> offsets = {
            glm::vec3(ringR, 0.0f, 0.0f),
            glm::vec3(-ringR * 0.5f, 0.0f, ringR * 0.866f),
            glm::vec3(-ringR * 0.5f, 0.0f, -ringR * 0.866f)};

        for (const auto &offset : offsets)
        {
            glm::vec3 circleCenter = playerPos + loseZombieCircleCenterOffset;
            glm::vec3 spawnPos = circleCenter + offset;
            spawnPos.y = endingGroundY + loseZombieGroundOffset;
            glm::vec3 toPlayer = playerPos - spawnPos;
            float yaw = std::atan2(-toPlayer.x, -toPlayer.z) + zombieModelYawOffset;
            if (auto *zombieEntity = spawnZombieActor(spawnPos, yaw))
            {
                loseZombies.push_back(zombieEntity);
                loseZombieFormationOffsets.push_back(offset);
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

    void updateCamera()
    {
        if (!(cameraEntity && playerVisualEntity))
            return;

        glm::vec3 playerPos = playerVisualEntity->localTransform.position;

        if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            const float radius = 8.8f;
            const float height = 3.8f;
            const float angularSpeed = 0.16f;
            const float targetPitch = glm::radians(-12.0f);

            float angle = elapsedTime * angularSpeed;
            glm::vec3 orbitPosition = playerPos + glm::vec3(std::sin(angle) * radius, height, std::cos(angle) * radius);

            float blendT = std::clamp(elapsedTime / std::max(0.01f, winCameraBlendInDuration), 0.0f, 1.0f);
            float smoothBlendT = blendT * blendT * (3.0f - 2.0f * blendT);

            cameraEntity->localTransform.position = glm::mix(winCameraStartPosition, orbitPosition, smoothBlendT);

            glm::vec3 toTarget = playerPos - cameraEntity->localTransform.position;
            toTarget.y = 0.0f;
            if (glm::dot(toTarget, toTarget) > 0.0001f)
            {
                float desiredYaw = std::atan2(-toTarget.x, -toTarget.z);
                cameraEntity->localTransform.rotation.y = glm::mix(winCameraStartYaw, desiredYaw, smoothBlendT);
            }

            cameraEntity->localTransform.rotation.x = glm::mix(winCameraStartPitch, targetPitch, smoothBlendT);
            cameraEntity->localTransform.rotation.z = 0.0f;
        }
        else
        {
            float playerClipDuration = playerLoseClip ? std::max(0.01f, playerLoseClip->duration) : 0.0f;
            float zombieClipDuration = zombieAttackClip ? std::max(0.01f, zombieAttackClip->duration)
                                                        : (zombieCrawlClip ? std::max(0.01f, zombieCrawlClip->duration) : 0.0f);
            float actionEndTime = std::max(losePlayerDeathBlendIn + playerClipDuration, loseZombieBiteStart + zombieClipDuration) + losePostActionHold;

            // Lose shot: cinematic side/front angle while action plays.
            float zoomT = std::clamp(loseSequenceTime / std::max(0.01f, actionEndTime), 0.0f, 1.0f);
            float smoothT = zoomT * zoomT * (3.0f - 2.0f * zoomT);

            // Bring scene closer while staying comfortably framed.
            float camDistance = glm::mix(6.1f, 4.8f, smoothT);
            float camHeight = glm::mix(3.05f, 2.65f, smoothT);
            glm::vec3 sideDir = glm::normalize(glm::vec3(0.9f, 0.0f, 0.45f));

            glm::vec3 baseCamPos = playerPos + sideDir * camDistance + glm::vec3(0.0f, camHeight, 0.0f);

            // After one-shot action finishes, turn camera toward the sky.
            float skyT = std::clamp((loseSequenceTime - actionEndTime) / std::max(0.01f, loseSkyTurnDuration), 0.0f, 1.0f);
            float skySmooth = skyT * skyT * (3.0f - 2.0f * skyT);

            cameraEntity->localTransform.position = glm::mix(baseCamPos, baseCamPos + glm::vec3(0.0f, 4.2f, -1.8f), skySmooth);
            lookAtOnGround(cameraEntity, playerPos);
            cameraEntity->localTransform.rotation.x = glm::mix(glm::radians(-19.0f), glm::radians(52.0f), skySmooth);
            cameraEntity->localTransform.rotation.z = 0.0f;
        }
    }

    void updateEndingActors(float deltaTime)
    {
        elapsedTime += deltaTime;

        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            loseSequenceTime += deltaTime;

            if (playerLoseClip)
            {
                // Play Olivia's death animation once, then hold on a lying portion of the clip.
                float clipDuration = std::max(0.01f, playerLoseClip->duration);
                float holdTime = std::clamp(clipDuration * losePlayerDeathHoldFraction, 0.0f, clipDuration);
                float playerTime = std::clamp(loseSequenceTime - losePlayerDeathBlendIn, 0.0f, holdTime);
                applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerLoseClip, playerTime);
            }

            for (size_t i = 0; i < loseZombies.size(); ++i)
            {
                auto *zombieEntity = loseZombies[i];
                if (!zombieEntity)
                    continue;

                // Keep formation centered around Olivia throughout the sequence.
                if (i < loseZombieFormationOffsets.size() && playerVisualEntity)
                {
                    glm::vec3 playerPos = playerVisualEntity->localTransform.position;
                    glm::vec3 circleCenter = playerPos + loseZombieCircleCenterOffset;
                    zombieEntity->localTransform.position = circleCenter + loseZombieFormationOffsets[i];
                    zombieEntity->localTransform.position.y = endingGroundY + loseZombieGroundOffset;
                }

                orientZombieTowardPlayer(zombieEntity);

                // In lose scene, zombies should keep biting on floor.
                const our::MotionClip *clip = zombieAttackClip ? zombieAttackClip : zombieCrawlClip;
                if (!clip)
                    continue;

                float phaseOffset = 0.12f * static_cast<float>(i);
                float biteMaxTime = std::max(0.01f, clip->duration * loseZombieBiteClipFraction);
                float zombieTime = std::clamp(loseSequenceTime - loseZombieBiteStart + phaseOffset, 0.0f, biteMaxTime);
                applyClipToSkinnedEntity(zombieEntity, zombieMesh, zombieMotion, clip, zombieTime);
            }
        }
        else if (our::GameSession::endingOutcome == our::EndingOutcome::Win)
        {
            if (playerVisualEntity)
            {
                playerVisualEntity->localTransform.position.y = endingGroundY + winPlayerGroundOffset;
            }

            if (playerWinClip)
            {
                float playerTime = positiveModulo(elapsedTime, std::max(0.01f, playerWinClip->duration));
                applyClipToSkinnedEntity(playerVisualEntity, playerMesh, playerMotion, playerWinClip, playerTime);
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
        clearLocalOccludersAroundStage(endingStageCenter);
        loadEndingFonts();
        setupPlayerVisual();
        setupLoseSceneActors();

        world.deleteMarkedEntities();

        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        renderer.setOverlaysVisible(false);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.setHealth(100.0f, 100.0f, 0.0f);

        elapsedTime = 0.0f;
        loseSequenceTime = 0.0f;

        if (cameraEntity)
        {
            winCameraStartPosition = cameraEntity->localTransform.position;
            winCameraStartYaw = cameraEntity->localTransform.rotation.y;
            winCameraStartPitch = cameraEntity->localTransform.rotation.x;
        }

        // Sequence requirement: Olivia dies first, then zombies perform their bite/circle action.
        if (our::GameSession::endingOutcome == our::EndingOutcome::Lose)
        {
            if (playerLoseClip)
            {
                float deathDuration = std::max(0.01f, playerLoseClip->duration);
                float deathHoldTime = std::clamp(deathDuration * losePlayerDeathHoldFraction, 0.0f, deathDuration);
                loseZombieBiteStart = losePlayerDeathBlendIn + deathHoldTime + loseZombieBiteDelayAfterDeath;
            }
            else
            {
                loseZombieBiteStart = 1.0f;
            }
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

        updateEndingActors(static_cast<float>(deltaTime));
        updateCamera();

        renderer.setTime(elapsedTime);
        renderer.setSceneExposure(our::GameSession::finalExposure);
        renderer.setMuzzleFlashStrength(0.0f);
        renderer.render(&world);
    }

    void onImmediateGui() override
    {
        const bool isWin = our::GameSession::endingOutcome == our::EndingOutcome::Win;
        const char *loseText = "You've lost to the darkness of night";

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
            if (isWin)
            {
                const char *winLine1 = "Congratulations, you have survived";
                const char *winLine2 = "until dawn";

                ImVec2 line1Size = ImGui::CalcTextSize(winLine1);
                ImVec2 line2Size = ImGui::CalcTextSize(winLine2);

                ImGui::SetCursorPosX(std::max(8.0f, (ImGui::GetWindowWidth() - line1Size.x) * 0.5f));
                ImGui::TextUnformatted(winLine1);

                ImGui::SetCursorPosX(std::max(8.0f, (ImGui::GetWindowWidth() - line2Size.x) * 0.5f));
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
