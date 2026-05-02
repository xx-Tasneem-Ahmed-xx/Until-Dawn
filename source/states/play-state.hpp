#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/free-camera-controller.hpp>
#include <systems/movement.hpp>
#include <systems/shooting-system.hpp>
#include <systems/collision-system.hpp>
#include <systems/scene-manager.hpp>
#include <systems/hud-system.hpp>
#include <systems/character-skeleton-system.hpp>
#include <systems/attachment-system.hpp>
#include <systems/player-controller.hpp>
#include <systems/zombie-animation-system.hpp>
#include <systems/zombie-spawning-system.hpp>
#include <components/camera.hpp>
#include <components/environment.hpp>
#include <components/free-camera-controller.hpp>
#include <components/health.hpp>
#include <components/mesh-renderer.hpp>
#include <components/player.hpp>
#include <components/weapon.hpp>
#include <components/weapon-attachment.hpp>
#include <components/zombie.hpp>
#include <animation/motion.hpp>
#include <audio-manager.hpp>
#include <asset-loader.hpp>
#include <deserialize-utils.hpp>
#include <game-session.hpp>
#include <ui/ui-theme.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#define ZOMBIE_INITIAL_WAVE_DELAY_SECONDS 0.2f
#define ZOMBIE_BETWEEN_WAVES_DELAY_SECONDS 4.0f

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State
{

    our::World world;
    our::ForwardRenderer renderer;
    our::HUDSystem hudSystem;
    our::FreeCameraControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::ShootingSystem shootingSystem;
    our::CollisionSystem collisionSystem;
    our::ZombieAnimationSystem zombieAnimationSystem;
    our::ZombieSpawningSystem zombieSpawningSystem;
    std::string worldAmbientTrack = "assets/audio/world.wav";
    std::string collisionSfxTrack = "assets/audio/collision.wav";
    std::string ouchSfxTrack = "assets/audio/female-ouch.wav";
    std::string rewardSfxTrack = "assets/audio/reward.wav";
    our::Mesh *pickupHealthMesh = nullptr;
    float worldAmbientGain = 0.45f;
    float collisionSfxCooldownSeconds = 0.12f;
    float collisionSfxCooldownLeft = 0.0f;
    float collisionSfxMinPushDistance = 0.05f;
    float ouchSfxDelaySeconds = 0.08f;
    float pendingOuchSfxTimeLeft = -1.0f;

    using CollisionPair = std::pair<const our::Entity *, const our::Entity *>;
    struct CollisionPairHash
    {
        size_t operator()(const CollisionPair &pair) const noexcept
        {
            const auto a = reinterpret_cast<std::uintptr_t>(pair.first);
            const auto b = reinterpret_cast<std::uintptr_t>(pair.second);
            return std::hash<std::uintptr_t>{}(a) ^ (std::hash<std::uintptr_t>{}(b) << 1);
        }
    };
    std::unordered_set<CollisionPair, CollisionPairHash> previousWallCollisionPairs;
    float muzzleFlashTimeLeft = 0.0f;
    const float muzzleFlashDuration = 0.06f;
    glm::vec2 muzzleFlashCenter = glm::vec2(0.5f, 0.5f);
    float totalTime = 0.0f;
    bool endingQueued = false;
    our::Entity *mainCameraEntity = nullptr;
    our::Entity *mainPlayerEntity = nullptr;

    our::ZombieSpawnerConfig zombieSpawnerConfig{};
    our::ZombieWaveRuntime zombieWaveRuntime{};
    our::Mesh *mainPlayerMesh = nullptr;
    our::Motion *mainPlayerMotion = nullptr;
    const our::MotionClip *mainPlayerIdleClip = nullptr;
    const our::MotionClip *mainPlayerRunClip = nullptr;
    const our::MotionClip *mainPlayerShootClip = nullptr;
    our::Entity *mainPlayerVisualEntity = nullptr;
    our::Transform mainPlayerVisualPrototypeTransform{};
    float mainPlayerModelYawOffset = 0.0f;
    float mainPlayerHeightOffset = -1.5f;
    our::Entity *mainPlayerPistolEntity = nullptr;
    our::Transform mainPlayerPistolPrototypeTransform{};
    glm::vec3 mainPlayerPistolBoneOffset = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 mainPlayerPistolRotationOffset = glm::vec3(0.0f, glm::pi<float>(), 0.0f);
    float mainPlayerPistolScaleMultiplier = 0.03f;
    std::string mainPlayerWeaponBoneName = "RightHand";
    our::WeaponAttachmentComponent *mainPlayerWeaponAttachment = nullptr;
    our::CharacterSkeletonSystem mainPlayerSkeletonSystem;
    our::AttachmentSystem attachmentSystem;
    float mainPlayerFollowDistance = 2.5f;
    glm::vec3 lastMainPlayerAnchorPosition = glm::vec3(0.0f);
    bool mainPlayerAnchorInitialized = false;

    int totalZombiesAcrossAllWaves = 0;
    int zombiesKilledCount = 0;
    float sunriseStartExposure = 0.10f;
    float sunriseEndExposure = 1.00f;
    float sunriseEasePower = 1.20f;

    float initialWaveDelaySeconds = ZOMBIE_INITIAL_WAVE_DELAY_SECONDS;
    float zombieSpawnHeightOffset = 0.0f;
    our::ZombieAnimationConfig zombieAnimationConfig{};
    float playerWallCollisionRetreatDistance = 0.12f;
    float playerVisualWallBufferDistance = 0.65f;
    float houseWallColliderExtraPaddingXZ = 1.4f;
    bool isPaused = false;
    bool musicEnabled = true;
    bool effectsEnabled = true;
    our::ui::pause::Assets pauseAssets{};

public:
    Playstate()
    {
        zombieSpawnerConfig.waveZombieCounts = {3, 5, 7, 10};
        zombieSpawnerConfig.betweenWavesDelaySeconds = ZOMBIE_BETWEEN_WAVES_DELAY_SECONDS;
        zombieWaveRuntime.waitingForNextWave = true;
        zombieWaveRuntime.betweenWaveTimer = ZOMBIE_INITIAL_WAVE_DELAY_SECONDS;
    }

private:
    void applyAudioPreferences()
    {
        auto &audio = our::AudioManager::getInstance();
        if (!audio.isInitialized())
            return;

        audio.setMusicEnabled(musicEnabled);
        audio.setEffectsEnabled(effectsEnabled);

        if (musicEnabled)
        {
            audio.playLoopingSound(worldAmbientTrack, worldAmbientGain);
        }
        else
        {
            audio.stopLoopingSound(worldAmbientTrack);
        }
    }

    void setPauseMode(bool paused)
    {
        if (isPaused == paused)
            return;

        isPaused = paused;
        if (isPaused)
        {
            our::Mouse::unlockMouse(getApp()->getWindow());
        }
        else
        {
            our::Mouse::lockMouse(getApp()->getWindow());
        }
    }

    void renderPauseOverlay()
    {
        if (!isPaused)
            return;

        our::ui::pause::drawBackdrop();
        our::ui::pause::setupPanelWindow();

        if (ImGui::Begin("PauseOverlay", nullptr, our::ui::pause::panelWindowFlags()))
        {
            const char *title = "Settings";
            if (pauseAssets.titleFont)
                ImGui::PushFont(pauseAssets.titleFont);
            our::ui::centerCurrentWindowText(title, 12.0f);
            ImGui::TextUnformatted(title);
            if (pauseAssets.titleFont)
                ImGui::PopFont();

            ImGui::Spacing();
            const char *subtitle = "Paused";
            if (pauseAssets.uiFont)
                ImGui::PushFont(pauseAssets.uiFont);
            our::ui::centerCurrentWindowText(subtitle, 12.0f);
            ImGui::TextUnformatted(subtitle);
            if (pauseAssets.uiFont)
                ImGui::PopFont();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Columns(2, "PauseGrid", false);

            our::ui::pause::centerButtonInCurrentColumn();
            if (our::ui::pause::drawIconTileButton(
                    "pause_music",
                    our::ui::pause::Assets::asTextureId(pauseAssets.musicIcon),
                    "Music",
                    musicEnabled,
                    our::ui::pause::Assets::asTextureId(pauseAssets.disabledOverlayIcon),
                    true))
            {
                musicEnabled = !musicEnabled;
                applyAudioPreferences();
            }

            ImGui::NextColumn();
            our::ui::pause::centerButtonInCurrentColumn();
            if (our::ui::pause::drawIconTileButton(
                    "pause_volume",
                    our::ui::pause::Assets::asTextureId(pauseAssets.volumeIcon),
                    "Volume",
                    effectsEnabled,
                    our::ui::pause::Assets::asTextureId(pauseAssets.disabledOverlayIcon)))
            {
                effectsEnabled = !effectsEnabled;
                applyAudioPreferences();
            }

            ImGui::NextColumn();
            our::ui::pause::centerButtonInCurrentColumn();
            if (our::ui::pause::drawIconTileButton(
                    "pause_menu",
                    our::ui::pause::Assets::asTextureId(pauseAssets.menuIcon),
                    "Menu",
                    true,
                    our::ui::pause::Assets::asTextureId(pauseAssets.disabledOverlayIcon)))
            {
                getApp()->changeState("menu");
            }

            ImGui::NextColumn();
            our::ui::pause::centerButtonInCurrentColumn();
            if (our::ui::pause::drawIconTileButton(
                    "pause_continue",
                    our::ui::pause::Assets::asTextureId(pauseAssets.continueIcon),
                    "Continue",
                    true,
                    our::ui::pause::Assets::asTextureId(pauseAssets.disabledOverlayIcon)))
            {
                setPauseMode(false);
            }

            ImGui::Columns(1);
        }
        ImGui::End();
    }

    bool isMainPlayerFamilyEntity(const our::Entity *entity) const
    {
        if (!entity)
            return false;

        const our::Entity *cursor = entity;
        while (cursor)
        {
            if (cursor == mainPlayerEntity)
                return true;
            cursor = cursor->parent;
        }

        return entity == mainCameraEntity;
    }

    bool isHealthPickupEntity(our::Entity *entity)
    {
        if (!entity)
            return false;

        if (!pickupHealthMesh)
            pickupHealthMesh = our::AssetLoader<our::Mesh>::get("pickup-health");

        if (auto *renderer = entity->getComponent<our::MeshRendererComponent>())
        {
            if (pickupHealthMesh && renderer->mesh == pickupHealthMesh)
                return true;
        }

        return entity->name.rfind("pickup_health", 0) == 0;
    }

    void setupHealthPickupColliders()
    {
        float playerColliderWorldY = 0.8f;
        if (mainCameraEntity)
        {
            if (auto *cameraCollider = mainCameraEntity->getComponent<our::ColliderComponent>())
            {
                playerColliderWorldY = mainCameraEntity->localTransform.position.y + cameraCollider->center.y;
            }
        }

        for (auto entity : world.getEntities())
        {
            if (!isHealthPickupEntity(entity))
                continue;

            auto *collider = entity->getComponent<our::ColliderComponent>();
            if (!collider)
                collider = entity->addComponent<our::ColliderComponent>();

            float maxScaleAxis = std::max({std::abs(entity->localTransform.scale.x),
                                           std::abs(entity->localTransform.scale.y),
                                           std::abs(entity->localTransform.scale.z),
                                           1.0f});
            float triggerHalfSize = std::clamp(0.12f * maxScaleAxis, 1.5f, 5.0f);

            collider->isTrigger = true;
            collider->halfSize = glm::vec3(triggerHalfSize, triggerHalfSize, triggerHalfSize);
            collider->center = glm::vec3(0.0f, playerColliderWorldY - entity->localTransform.position.y, 0.0f);
        }
    }

    void inflateHouseWallColliders()
    {
        our::Mesh *houseAMesh = our::AssetLoader<our::Mesh>::get("env_house_a");
        our::Mesh *houseBMesh = our::AssetLoader<our::Mesh>::get("env_house_b");
        if (!houseAMesh && !houseBMesh)
            return;

        const float extra = std::max(0.0f, houseWallColliderExtraPaddingXZ);
        if (extra <= 0.0f)
            return;

        for (auto entity : world.getEntities())
        {
            auto *env = entity->getComponent<our::EnvironmentComponent>();
            auto *collider = entity->getComponent<our::ColliderComponent>();
            auto *renderer = entity->getComponent<our::MeshRendererComponent>();
            if (!(env && collider && renderer && renderer->mesh))
                continue;

            auto toLower = [](std::string value)
            {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                               { return static_cast<char>(std::tolower(c)); });
                return value;
            };

            if (toLower(env->environmentType) == "floor")
                continue;

            bool isHouseMesh = (renderer->mesh == houseAMesh) || (renderer->mesh == houseBMesh);
            if (!isHouseMesh)
                continue;

            collider->halfSize.x += extra;
            collider->halfSize.z += extra;
        }
    }

    bool collectHealthPickup(our::Entity *pickupEntity)
    {
        if (!pickupEntity)
            return false;

        if (auto *playerHealth = getMainPlayerHealth(); playerHealth)
        {
            playerHealth->currentHealth = playerHealth->maxHealth;
            playerHealth->isAlive = playerHealth->maxHealth > 0.0f;
        }

        pendingOuchSfxTimeLeft = -1.0f;
        if (our::AudioManager::getInstance().isInitialized() && !rewardSfxTrack.empty())
        {
            our::AudioManager::getInstance().playSound(rewardSfxTrack);
        }
        world.markForRemoval(pickupEntity);
        return true;
    }

    void processHealthPickups()
    {
        if (!mainCameraEntity)
            mainCameraEntity = findMainCameraEntity(mainPlayerEntity);

        our::ColliderComponent *playerCollider = mainCameraEntity ? mainCameraEntity->getComponent<our::ColliderComponent>() : nullptr;
        glm::vec3 playerPos = getPlayerTargetPosition();

        for (auto entity : world.getEntities())
        {
            if (!isHealthPickupEntity(entity))
                continue;

            auto *pickupCollider = entity->getComponent<our::ColliderComponent>();
            if (!(playerCollider && pickupCollider))
            {
                glm::vec3 pickupPos = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                glm::vec3 dxz = playerPos - pickupPos;
                dxz.y = 0.0f;
                if (glm::length(dxz) <= 2.0f)
                {
                    collectHealthPickup(entity);
                }
                continue;
            }

            glm::vec3 pMin, pMax, hMin, hMax;
            playerCollider->getWorldBounds(pMin, pMax);
            pickupCollider->getWorldBounds(hMin, hMax);

            bool overlapXZ = (pMin.x <= hMax.x && pMax.x >= hMin.x) &&
                             (pMin.z <= hMax.z && pMax.z >= hMin.z);

            if (overlapXZ)
            {
                collectHealthPickup(entity);
            }
        }
    }

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

    our::Entity *findMainPlayerVisualEntity()
    {
        if (!mainPlayerMesh)
            mainPlayerMesh = our::AssetLoader<our::Mesh>::get("main-player");
        if (!mainPlayerMesh)
            return nullptr;

        if (mainPlayerEntity)
        {
            for (auto entity : world.getEntities())
            {
                if (entity->parent != mainPlayerEntity)
                    continue;
                auto *renderer = entity->getComponent<our::MeshRendererComponent>();
                if (renderer && renderer->mesh == mainPlayerMesh)
                    return entity;
            }
        }

        for (auto entity : world.getEntities())
        {
            auto *renderer = entity->getComponent<our::MeshRendererComponent>();
            if (renderer && renderer->mesh == mainPlayerMesh)
                return entity;
        }

        return nullptr;
    }

    our::Entity *findPistolEntity()
    {
        our::Mesh *pistolMesh = our::AssetLoader<our::Mesh>::get("pistol");
        if (!pistolMesh)
            return nullptr;

        for (auto entity : world.getEntities())
        {
            auto *renderer = entity->getComponent<our::MeshRendererComponent>();
            if (renderer && renderer->mesh == pistolMesh)
                return entity;
        }

        return nullptr;
    }

    void updateMainPlayerPistolAttachment()
    {
        if (!mainPlayerWeaponAttachment)
            return;

        attachmentSystem.updateWeaponTransform(mainPlayerWeaponAttachment);
    }

    void setupMainPlayerWeaponAttachment()
    {
        if (!mainPlayerPistolEntity)
            mainPlayerPistolEntity = findPistolEntity();
        if (!mainPlayerPistolEntity)
            return;

        mainPlayerPistolPrototypeTransform = mainPlayerPistolEntity->localTransform;

        if (!mainPlayerWeaponAttachment)
        {
            mainPlayerWeaponAttachment = mainPlayerPistolEntity->getComponent<our::WeaponAttachmentComponent>();
            if (!mainPlayerWeaponAttachment)
                mainPlayerWeaponAttachment = mainPlayerPistolEntity->addComponent<our::WeaponAttachmentComponent>();
        }

        auto *renderer = mainPlayerPistolEntity->getComponent<our::MeshRendererComponent>();
        if (mainPlayerWeaponAttachment)
        {
            if (renderer)
                mainPlayerWeaponAttachment->mesh = renderer->mesh;

            mainPlayerWeaponAttachment->offsetTransform.position = mainPlayerPistolBoneOffset;
            mainPlayerWeaponAttachment->offsetTransform.rotation = mainPlayerPistolRotationOffset;
            mainPlayerWeaponAttachment->offsetTransform.scale = mainPlayerPistolPrototypeTransform.scale * mainPlayerPistolScaleMultiplier;
            mainPlayerWeaponAttachment->rebuildOffsetMatrix();

            mainPlayerWeaponAttachment->detachedWorldMatrix = mainPlayerPistolEntity->getLocalToWorldMatrix();
            attachmentSystem.attachWeaponToBone(mainPlayerWeaponAttachment, &mainPlayerSkeletonSystem, mainPlayerWeaponBoneName);
        }
    }

    void bindMainPlayerMotionClips()
    {
        mainPlayerMotion = our::AssetLoader<our::Motion>::get("main-player-motion");

        if (!mainPlayerMotion)
        {
            std::cout << "[Motion] main-player-motion asset not found.\n";
            return;
        }

        // std::cout << "[Motion] Olivia clips found (" << mainPlayerMotion->clips.size() << "): ";
        // for (size_t i = 0; i < mainPlayerMotion->clips.size(); ++i)
        // {
        //     std::cout << "[" << i << "] " << mainPlayerMotion->clips[i].name;
        //     if (i + 1 < mainPlayerMotion->clips.size())
        //         std::cout << ", ";
        // }
        // std::cout << "\n";

        auto toLower = [](std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return s;
        };

        auto findClipCaseInsensitive = [&](const std::string &exactName) -> const our::MotionClip *
        {
            if (exactName.empty())
                return nullptr;

            if (const our::MotionClip *exact = mainPlayerMotion->findClip(exactName))
                return exact;

            std::string loweredNeedle = toLower(exactName);
            for (const auto &clip : mainPlayerMotion->clips)
            {
                if (toLower(clip.name) == loweredNeedle)
                    return &clip;
            }
            return nullptr;
        };

        mainPlayerRunClip = findClipCaseInsensitive("Run");
        mainPlayerShootClip = findClipCaseInsensitive("Pistol Shoot");
        mainPlayerIdleClip = findClipCaseInsensitive("Pistol Idle");
    }

    const our::MotionClip *getMainPlayerClipForState(our::PlayerAnimationState state) const
    {
        if (state == our::PlayerAnimationState::Shooting)
            return mainPlayerShootClip ? mainPlayerShootClip : mainPlayerRunClip;
        if (state == our::PlayerAnimationState::Running)
            return mainPlayerRunClip ? mainPlayerRunClip : mainPlayerIdleClip;
        return mainPlayerIdleClip;
    }

    void updateMainPlayerAnimation(float deltaTime)
    {
        if (!mainPlayerEntity)
            mainPlayerEntity = findMainPlayerEntity();
        if (!mainCameraEntity)
            mainCameraEntity = findMainCameraEntity(mainPlayerEntity);
        if (!mainPlayerVisualEntity)
            mainPlayerVisualEntity = findMainPlayerVisualEntity();

        if (!(mainPlayerEntity && mainPlayerVisualEntity))
            return;

        auto *player = mainPlayerEntity->getComponent<our::PlayerComponent>();
        auto *renderer = mainPlayerVisualEntity->getComponent<our::MeshRendererComponent>();
        if (!(player && renderer && renderer->mesh))
            return;

        auto &keyboard = getApp()->getKeyboard();
        bool movementKeysPressed = keyboard.isPressed(GLFW_KEY_W) || keyboard.isPressed(GLFW_KEY_A) || keyboard.isPressed(GLFW_KEY_S) || keyboard.isPressed(GLFW_KEY_D);

        bool startedShootingThisFrame = false;
        if (player->shootRequested && player->animationState != our::PlayerAnimationState::Shooting)
        {
            player->animationState = our::PlayerAnimationState::Shooting;
            player->activeMotionClip.clear();
            player->motionClipTime = 0.0f;
            player->shootClipTime = 0.0f;
            startedShootingThisFrame = true;
        }
        player->shootRequested = false;

        glm::vec3 cameraWorldPos = glm::vec3(0.0f);
        if (mainCameraEntity)
            cameraWorldPos = glm::vec3(mainCameraEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));

        glm::vec3 playerWorldPos = glm::vec3(mainPlayerEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
        glm::vec3 cameraForward = getCameraForwardOnGround();
        glm::vec3 anchorPosition = cameraWorldPos + (cameraForward * mainPlayerFollowDistance);

        lastMainPlayerAnchorPosition = playerWorldPos;
        mainPlayerAnchorInitialized = true;

        bool isMoving = movementKeysPressed;

        if (!startedShootingThisFrame && player->animationState != our::PlayerAnimationState::Shooting)
        {
            player->animationState = isMoving ? our::PlayerAnimationState::Running : our::PlayerAnimationState::Idle;
        }

        const our::MotionClip *clip = getMainPlayerClipForState(player->animationState);
        if (!clip)
            return;

        if (player->animationState == our::PlayerAnimationState::Shooting)
        {
            player->shootClipTime += deltaTime;
        }

        if (player->activeMotionClip != clip->name)
        {
            player->activeMotionClip = clip->name;
            player->motionClipTime = 0.0f;
            if (player->animationState == our::PlayerAnimationState::Shooting)
                player->shootClipTime = 0.0f;
        }
        else
        {
            player->motionClipTime += deltaTime;
        }

        if (player->animationState == our::PlayerAnimationState::Shooting)
        {
            if (clip->duration > 0.0001f)
                player->motionClipTime = std::min(player->motionClipTime, clip->duration);

            bool shootingFinished = (clip->duration <= 0.0001f) || (player->shootClipTime >= clip->duration);
            if (shootingFinished)
            {
                player->animationState = isMoving ? our::PlayerAnimationState::Running : our::PlayerAnimationState::Idle;
                const our::MotionClip *nextClip = getMainPlayerClipForState(player->animationState);
                if (nextClip)
                {
                    player->activeMotionClip = nextClip->name;
                    player->motionClipTime = 0.0f;
                }
                clip = nextClip;
            }
        }
        else if (clip->duration > 0.0001f)
        {
            player->motionClipTime = std::fmod(player->motionClipTime, clip->duration);
        }

        if (!mainPlayerVisualEntity->parent)
        {
            mainPlayerVisualEntity->localTransform.position = anchorPosition;
        }
        mainPlayerVisualEntity->localTransform.position.y = mainPlayerHeightOffset;
        mainPlayerVisualEntity->localTransform.scale = mainPlayerVisualPrototypeTransform.scale;

        glm::vec3 cameraRight = glm::normalize(glm::vec3(-cameraForward.z, 0.0f, cameraForward.x));
        bool forwardPressed = keyboard.isPressed(GLFW_KEY_W);
        bool backwardPressed = keyboard.isPressed(GLFW_KEY_S);
        bool rightPressed = keyboard.isPressed(GLFW_KEY_D);
        bool leftPressed = keyboard.isPressed(GLFW_KEY_A);

        glm::vec3 moveDirection(0.0f);
        if (forwardPressed)
            moveDirection += cameraForward;
        if (backwardPressed)
            moveDirection -= cameraForward;
        if (rightPressed)
            moveDirection += cameraRight;
        if (leftPressed)
            moveDirection -= cameraRight;

        glm::vec3 facingDirection(0.0f);
        if (forwardPressed)
            facingDirection += cameraForward;
        if (backwardPressed)
        {
            if (!forwardPressed && !leftPressed && !rightPressed)
                facingDirection += cameraForward;
            else
                facingDirection -= cameraForward;
        }
        if (rightPressed)
            facingDirection += cameraRight;
        if (leftPressed)
            facingDirection -= cameraRight;

        if (glm::dot(facingDirection, facingDirection) > 0.0001f)
        {
            facingDirection = glm::normalize(facingDirection);
            float yaw = std::atan2(facingDirection.x, facingDirection.z);
            mainPlayerVisualEntity->localTransform.rotation.y = yaw + mainPlayerModelYawOffset;
        }

        mainPlayerSkeletonSystem.bindRuntimePose(
            mainPlayerMotion,
            clip,
            player->motionClipTime,
            mainPlayerVisualEntity->getLocalToWorldMatrix());

        updateMainPlayerPistolAttachment();

        bool canSkin = renderer->mesh->hasSkinning() && mainPlayerMotion && clip;
        if (!canSkin)
        {
            player->skinMatrices.clear();
            return;
        }

        if (!mainPlayerMotion->computeSkinMatrices(
                clip,
                player->motionClipTime,
                renderer->mesh->getSkinJointNodes(),
                renderer->mesh->getInverseBindMatrices(),
                player->skinMatrices))
        {
            player->skinMatrices.clear();
        }
    }

    glm::vec3 getPlayerTargetPosition()
    {
        if (!mainCameraEntity)
            mainCameraEntity = findMainCameraEntity(mainPlayerEntity);
        if (mainCameraEntity)
        {
            return glm::vec3(mainCameraEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
        }
        if (mainPlayerEntity)
        {
            return glm::vec3(mainPlayerEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
        }
        return glm::vec3(0.0f);
    }

    glm::vec3 getMainPlayerCombatTargetPosition()
    {
        if (!mainPlayerVisualEntity)
            mainPlayerVisualEntity = findMainPlayerVisualEntity();

        if (mainPlayerVisualEntity)
        {
            return glm::vec3(mainPlayerVisualEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
        }

        if (!mainPlayerEntity)
            mainPlayerEntity = findMainPlayerEntity();

        if (mainPlayerEntity)
        {
            return glm::vec3(mainPlayerEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
        }

        // Fallback only if player entity is unavailable.
        return getPlayerTargetPosition();
    }

    void lockCameraAndPlayerVerticalToZero()
    {
        if (!mainCameraEntity)
            mainCameraEntity = findMainCameraEntity(mainPlayerEntity);
        if (!mainPlayerEntity)
            mainPlayerEntity = findMainPlayerEntity();

        if (mainPlayerEntity)
            mainPlayerEntity->localTransform.position.y = 0.0f;

        if (mainCameraEntity)
        {
            mainCameraEntity->localTransform.position.y = 0.0f;
            if (mainCameraEntity->parent)
                mainCameraEntity->parent->localTransform.position.y = 0.0f;
        }
    }

    glm::vec3 getCameraForwardOnGround()
    {
        if (!mainCameraEntity)
            mainCameraEntity = findMainCameraEntity(mainPlayerEntity);

        glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
        if (mainCameraEntity)
        {
            glm::mat4 worldMatrix = mainCameraEntity->getLocalToWorldMatrix();
            forward = glm::vec3(worldMatrix * glm::vec4(0, 0, -1, 0));
        }

        forward.y = 0.0f;
        if (glm::dot(forward, forward) < 0.0001f)
            return glm::vec3(0.0f, 0.0f, -1.0f);
        return glm::normalize(forward);
    }

    void loadZombieGameplayConfig(const nlohmann::json &sceneConfig)
    {
        if (!sceneConfig.contains("zombies"))
            return;
        const auto &zombiesConfig = sceneConfig["zombies"];
        if (!zombiesConfig.is_object())
            return;

        zombieSpawningSystem.loadGameplayConfig(
            zombiesConfig,
            zombieSpawnerConfig,
            initialWaveDelaySeconds,
            zombieSpawnHeightOffset);

        zombieAnimationSystem.configureRuntime(
            zombieAnimationConfig,
            zombieSpawnerConfig.zombieModelYawOffset,
            zombieSpawnerConfig.zombieModelScaleMultiplier,
            zombieSpawnerConfig.zombiePrototypeTransform,
            zombieSpawnerConfig.zombieMaterial);
        zombieAnimationSystem.loadGameplayConfig(zombiesConfig, zombieAnimationConfig);

        sunriseStartExposure = std::clamp(zombiesConfig.value("sunriseStartExposure", sunriseStartExposure), 0.0f, 2.0f);
        sunriseEndExposure = std::clamp(zombiesConfig.value("sunriseEndExposure", sunriseEndExposure), 0.0f, 2.0f);
        sunriseEasePower = std::max(0.05f, zombiesConfig.value("sunriseEasePower", sunriseEasePower));
    }

    void recalculateSunriseTargets()
    {
        totalZombiesAcrossAllWaves = zombieSpawningSystem.getTotalPlannedZombieCount(zombieSpawnerConfig);
    }

    float computeSunriseProgress() const
    {
        if (totalZombiesAcrossAllWaves <= 0)
            return 1.0f;

        float t = static_cast<float>(zombiesKilledCount) / static_cast<float>(totalZombiesAcrossAllWaves);
        return std::clamp(t, 0.0f, 1.0f);
    }

    float computeCurrentExposure() const
    {
        float progress = computeSunriseProgress();
        float easedProgress = std::pow(progress, sunriseEasePower);
        return glm::mix(sunriseStartExposure, sunriseEndExposure, easedProgress);
    }

    void updateWaveSystem(float deltaTime)
    {
        zombieSpawningSystem.update(
            &world,
            zombieWaveRuntime,
            zombieSpawnerConfig,
            deltaTime,
            getMainPlayerCombatTargetPosition(),
            getCameraForwardOnGround());
    }

    void updateZombies(float deltaTime)
    {
        glm::vec3 playerTarget = getMainPlayerCombatTargetPosition();
        our::HealthComponent *playerHealth = getMainPlayerHealth();

        zombieAnimationSystem.updateAllZombies(
            &world,
            deltaTime,
            playerTarget,
            playerHealth,
            zombieAnimationConfig);
    }

    void handleCollisions()
    {
        if (!mainPlayerEntity)
            mainPlayerEntity = findMainPlayerEntity();
        if (!mainCameraEntity)
            mainCameraEntity = findMainCameraEntity(mainPlayerEntity);

        auto makeOrderedPair = [](const our::Entity *a, const our::Entity *b)
        {
            return (a < b) ? CollisionPair{a, b} : CollisionPair{b, a};
        };
        std::unordered_set<CollisionPair, CollisionPairHash> currentWallCollisionPairs;

        const int MAX_PASSES = 4;
        for (int pass = 0; pass < MAX_PASSES; ++pass)
        {
            collisionSystem.update(&world);
            auto &allCollisions = collisionSystem.getCurrentCollisions();
            bool anyResolved = false;

            for (const auto &collision : allCollisions)
            {
                our::Entity *entityA = collision.entityA;
                our::Entity *entityB = collision.entityB;

                our::Entity *pickedHealthBox = nullptr;
                if (isHealthPickupEntity(entityA) && isMainPlayerFamilyEntity(entityB))
                {
                    pickedHealthBox = entityA;
                }
                else if (isHealthPickupEntity(entityB) && isMainPlayerFamilyEntity(entityA))
                {
                    pickedHealthBox = entityB;
                }

                if (pickedHealthBox)
                {
                    collectHealthPickup(pickedHealthBox);
                    continue;
                }

                auto envA = entityA->getComponent<our::EnvironmentComponent>();
                auto envB = entityB->getComponent<our::EnvironmentComponent>();

                auto toLower = [](std::string value)
                {
                    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                                   { return static_cast<char>(std::tolower(c)); });
                    return value;
                };

                const std::string envTypeA = envA ? toLower(envA->environmentType) : std::string{};
                const std::string envTypeB = envB ? toLower(envB->environmentType) : std::string{};

                bool isStaticA = envA != nullptr;
                bool isStaticB = envB != nullptr;

                if (isStaticA && isStaticB)
                    continue;

                // Soft dynamic-vs-dynamic separation for zombies so they don't overlap each other.
                if (!isStaticA && !isStaticB)
                {
                    auto *zombieA = entityA->getComponent<our::ZombieComponent>();
                    auto *zombieB = entityB->getComponent<our::ZombieComponent>();
                    if (!(zombieA && zombieB))
                        continue;

                    // Resolve using actual collider overlap in XZ for robust crowd separation.
                    glm::vec3 minA, maxA, minB, maxB;
                    collision.colliderA->getWorldBounds(minA, maxA);
                    collision.colliderB->getWorldBounds(minB, maxB);

                    float overlapX = std::min(maxA.x, maxB.x) - std::max(minA.x, minB.x);
                    float overlapZ = std::min(maxA.z, maxB.z) - std::max(minA.z, minB.z);
                    if (overlapX <= 0.0f || overlapZ <= 0.0f)
                        continue;

                    glm::vec3 posA = glm::vec3(entityA->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                    glm::vec3 posB = glm::vec3(entityB->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));

                    const float separationMargin = 0.04f;
                    if (overlapX <= overlapZ)
                    {
                        float sign = (posA.x < posB.x) ? -1.0f : 1.0f;
                        float pushEach = (overlapX + separationMargin) * 0.5f;
                        entityA->localTransform.position.x += sign * pushEach;
                        entityB->localTransform.position.x -= sign * pushEach;
                    }
                    else
                    {
                        float sign = (posA.z < posB.z) ? -1.0f : 1.0f;
                        float pushEach = (overlapZ + separationMargin) * 0.5f;
                        entityA->localTransform.position.z += sign * pushEach;
                        entityB->localTransform.position.z -= sign * pushEach;
                    }
                    anyResolved = true;

                    continue;
                }

                bool isFloorA = envA && envTypeA == "floor";
                bool isFloorB = envB && envTypeB == "floor";
                bool isWallLikeA = envA && !isFloorA;
                bool isWallLikeB = envB && !isFloorB;

                if (isHealthPickupEntity(entityA) || isHealthPickupEntity(entityB))
                    continue;

                our::CollisionInfo oriented;
                our::Entity *rawDynamic;
                bool collidingWithFloor;
                bool collidingWithWallLike;

                if (isStaticB)
                {
                    oriented = collision;
                    rawDynamic = entityA;
                    collidingWithFloor = isFloorB;
                    collidingWithWallLike = isWallLikeB;
                }
                else
                {
                    oriented.entityA = entityB;
                    oriented.entityB = entityA;
                    oriented.colliderA = collision.colliderB;
                    oriented.colliderB = collision.colliderA;
                    rawDynamic = entityB;
                    collidingWithFloor = isFloorA;
                    collidingWithWallLike = isWallLikeA;
                }

                CollisionPair wallCollisionPair = makeOrderedPair(rawDynamic, oriented.entityB);
                bool isNewWallCollision = false;
                if (collidingWithWallLike)
                {
                    bool firstSeenThisFrame = currentWallCollisionPairs.insert(wallCollisionPair).second;
                    isNewWallCollision = firstSeenThisFrame && (previousWallCollisionPairs.find(wallCollisionPair) == previousWallCollisionPairs.end());
                }

                // If dynamic belongs to player family, push the camera (actual moving body).
                our::Entity *dynamicEntity = rawDynamic;
                if (mainCameraEntity)
                {
                    our::Entity *cursor = rawDynamic;
                    while (cursor)
                    {
                        if (cursor == mainPlayerEntity)
                        {
                            dynamicEntity = mainCameraEntity;
                            break;
                        }
                        cursor = cursor->parent;
                    }
                }

                glm::vec3 pushBack(0.0f);

                if (collidingWithFloor)
                {
                    pushBack = collisionSystem.resolveAABB(oriented);
                    pushBack.x = 0.0f;
                    pushBack.z = 0.0f;
                    if (pushBack.y < 0.0f)
                        pushBack.y = 0.0f;
                }
                else if (collidingWithWallLike)
                {
                    // Use the moving camera collider for overlap computation.
                    our::ColliderComponent *cameraCollider =
                        mainCameraEntity ? mainCameraEntity->getComponent<our::ColliderComponent>() : nullptr;
                    bool useCameraCollider = (dynamicEntity == mainCameraEntity) && cameraCollider;
                    our::ColliderComponent *dynamicCollider = useCameraCollider ? cameraCollider : oriented.colliderA;
                    our::ColliderComponent *wallCollider = oriented.colliderB;

                    glm::vec3 minA, maxA, minB, maxB;
                    dynamicCollider->getWorldBounds(minA, maxA);
                    wallCollider->getWorldBounds(minB, maxB);

                    bool xOverlap = (minA.x <= maxB.x && maxA.x >= minB.x);
                    bool zOverlap = (minA.z <= maxB.z && maxA.z >= minB.z);
                    if (!xOverlap || !zOverlap)
                        continue;

                    float overlapX_pos = maxB.x - minA.x;
                    float overlapX_neg = maxA.x - minB.x;
                    float overlapZ_pos = maxB.z - minA.z;
                    float overlapZ_neg = maxA.z - minB.z;

                    float px = (overlapX_pos < overlapX_neg) ? overlapX_pos : -overlapX_neg;
                    float pz = (overlapZ_pos < overlapZ_neg) ? overlapZ_pos : -overlapZ_neg;

                    if (glm::abs(px) <= glm::abs(pz))
                        pushBack = glm::vec3(px, 0.0f, 0.0f);
                    else
                        pushBack = glm::vec3(0.0f, 0.0f, pz);
                }

                const float epsilon = 0.0005f;
                if (glm::length(pushBack) > epsilon)
                {
                    bool shouldPlayCollisionSfx = false;
                    float impactPushLen = 0.0f;

                    // Add an intentional extra retreat for the main player when
                    // colliding with wall-like geometry so the collision response
                    // is clearly noticeable and prevents sticky penetration feel.
                    if (collidingWithWallLike && dynamicEntity == mainCameraEntity)
                    {
                        glm::vec3 horizontalPush(pushBack.x, 0.0f, pushBack.z);
                        float pushLen = glm::length(horizontalPush);
                        if (pushLen > epsilon)
                        {
                            glm::vec3 retreatDir = horizontalPush / pushLen;
                            float totalRetreat = std::max(0.0f, playerWallCollisionRetreatDistance + playerVisualWallBufferDistance);
                            pushBack += retreatDir * totalRetreat;
                            shouldPlayCollisionSfx = true;
                            impactPushLen = pushLen;
                        }
                    }
                    else if (collidingWithWallLike)
                    {
                        // Add a small margin for non-player entities (especially zombies)
                        // to avoid visible wall penetration due to animation/scale mismatch.
                        glm::vec3 horizontalPush(pushBack.x, 0.0f, pushBack.z);
                        float pushLen = glm::length(horizontalPush);
                        if (pushLen > epsilon)
                        {
                            glm::vec3 retreatDir = horizontalPush / pushLen;
                            float wallSeparation = 0.06f;
                            if (auto *z = dynamicEntity->getComponent<our::ZombieComponent>())
                            {
                                wallSeparation = std::clamp(z->radius * 0.12f, 0.04f, 0.22f);
                            }
                            pushBack += retreatDir * wallSeparation;
                        }
                    }

                    dynamicEntity->localTransform.position += pushBack;

                    if (shouldPlayCollisionSfx && isNewWallCollision && impactPushLen >= collisionSfxMinPushDistance && collisionSfxCooldownLeft <= 0.0f)
                    {
                        if (our::AudioManager::getInstance().isInitialized() && !collisionSfxTrack.empty())
                        {
                            our::AudioManager::getInstance().playSound(collisionSfxTrack);
                        }
                        collisionSfxCooldownLeft = collisionSfxCooldownSeconds;
                        pendingOuchSfxTimeLeft = std::max(0.0f, ouchSfxDelaySeconds);
                    }

                    anyResolved = true;
                }
            }

            if (!anyResolved)
                break;
        }

        previousWallCollisionPairs = std::move(currentWallCollisionPairs);
    }

    void onInitialize() override
    {
        // Reset all per-run runtime state because this state instance is reused across scene changes.
        zombiesKilledCount = 0;
        endingQueued = false;
        totalTime = 0.0f;
        collisionSfxCooldownLeft = 0.0f;
        pendingOuchSfxTimeLeft = -1.0f;
        muzzleFlashTimeLeft = 0.0f;
        mainPlayerAnchorInitialized = false;
        lastMainPlayerAnchorPosition = glm::vec3(0.0f);
        zombieAnimationSystem.resetEffects();

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

        loadZombieGameplayConfig(config);
        if (config.contains("mainPlayer") && config["mainPlayer"].is_object())
        {
            mainPlayerHeightOffset = config["mainPlayer"].value("heightOffset", mainPlayerHeightOffset);

            const auto &mainPlayerConfig = config["mainPlayer"];
            mainPlayerFollowDistance = mainPlayerConfig.value("followDistance", mainPlayerFollowDistance);
            if (mainPlayerConfig.contains("pistolBoneOffset") && mainPlayerConfig["pistolBoneOffset"].is_array())
                mainPlayerPistolBoneOffset = mainPlayerConfig["pistolBoneOffset"].get<glm::vec3>();
            if (mainPlayerConfig.contains("pistolRotationOffset") && mainPlayerConfig["pistolRotationOffset"].is_array())
                mainPlayerPistolRotationOffset = glm::radians(mainPlayerConfig["pistolRotationOffset"].get<glm::vec3>());
            mainPlayerPistolScaleMultiplier = mainPlayerConfig.value("pistolScaleMultiplier", mainPlayerPistolScaleMultiplier);
            mainPlayerWeaponBoneName = mainPlayerConfig.value("pistolBoneName", mainPlayerWeaponBoneName);
            playerVisualWallBufferDistance = std::max(0.0f, mainPlayerConfig.value("wallBufferDistance", playerVisualWallBufferDistance));
            houseWallColliderExtraPaddingXZ = std::max(0.0f, mainPlayerConfig.value("houseColliderPaddingXZ", houseWallColliderExtraPaddingXZ));
        }
        zombieAnimationSystem.initializeAssets("zombie-motion");
        bindMainPlayerMotionClips();

        mainPlayerEntity = findMainPlayerEntity();
        mainCameraEntity = findMainCameraEntity(mainPlayerEntity);
        pickupHealthMesh = our::AssetLoader<our::Mesh>::get("pickup-health");
        mainPlayerMesh = our::AssetLoader<our::Mesh>::get("main-player");
        mainPlayerVisualEntity = findMainPlayerVisualEntity();
        mainPlayerPistolEntity = nullptr;
        mainPlayerWeaponAttachment = nullptr;
        if (mainPlayerVisualEntity)
        {
            mainPlayerVisualPrototypeTransform = mainPlayerVisualEntity->localTransform;

            if (auto *player = mainPlayerEntity ? mainPlayerEntity->getComponent<our::PlayerComponent>() : nullptr)
            {
                if (auto *renderer = mainPlayerVisualEntity->getComponent<our::MeshRendererComponent>(); renderer && renderer->mesh && renderer->mesh->hasSkinning())
                {
                    player->skinMatrices.assign(renderer->mesh->getSkinJointNodes().size(), glm::mat4(1.0f));
                }
            }
        }
        setupMainPlayerWeaponAttachment();
        inflateHouseWallColliders();
        setupHealthPickupColliders();
        lockCameraAndPlayerVerticalToZero();
        zombieSpawningSystem.initializeFromWorld(&world, zombieSpawnerConfig, zombieSpawnHeightOffset);
        zombieAnimationSystem.configureRuntime(
            zombieAnimationConfig,
            zombieSpawnerConfig.zombieModelYawOffset,
            zombieSpawnerConfig.zombieModelScaleMultiplier,
            zombieSpawnerConfig.zombiePrototypeTransform,
            zombieSpawnerConfig.zombieMaterial);
        recalculateSunriseTargets();
        zombieSpawningSystem.resetRuntime(zombieWaveRuntime, initialWaveDelaySeconds);
        isPaused = false;
        musicEnabled = true;
        effectsEnabled = true;
        previousWallCollisionPairs.clear();

        pauseAssets.loadDefaultThemeResources();

        if (our::AudioManager::getInstance().isInitialized())
        {
            our::AudioManager::getInstance().playLoopingSound(worldAmbientTrack, worldAmbientGain);
            applyAudioPreferences();
        }

        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        hudSystem.initialize();
        our::SceneManager::validateWorld(&world);
        our::GameSession::clear();
    }

    void onDraw(double deltaTime) override
    {
        auto &keyboard = getApp()->getKeyboard();
        if (keyboard.justPressed(GLFW_KEY_P))
        {
            setPauseMode(!isPaused);
        }

        if (!isPaused)
        {
            totalTime += (float)deltaTime;
        }
        renderer.setTime(totalTime);
        collisionSfxCooldownLeft = std::max(0.0f, collisionSfxCooldownLeft - static_cast<float>(deltaTime));

        if (pendingOuchSfxTimeLeft >= 0.0f)
        {
            pendingOuchSfxTimeLeft -= static_cast<float>(deltaTime);
            if (pendingOuchSfxTimeLeft <= 0.0f)
            {
                if (our::AudioManager::getInstance().isInitialized() && !ouchSfxTrack.empty())
                {
                    our::AudioManager::getInstance().playSound(ouchSfxTrack);
                }
                pendingOuchSfxTimeLeft = -1.0f;
            }
        }

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

        renderer.setSceneExposure(computeCurrentExposure());

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

        if (isPaused)
        {
            renderer.setSceneExposure(computeCurrentExposure());
            renderer.setMuzzleFlashStrength(0.0f);
            renderer.render(&world);
            our::AudioManager::getInstance().cleanupFinishedSources();
            return;
        }

        // Here, we just run a bunch of systems to control the world logic
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        updateMainPlayerAnimation((float)deltaTime);

        // Update main player weapon (handles cooldown and reload)
        if (mainPlayerWeapon)
        {
            mainPlayerWeapon->update((float)deltaTime);
        }

        updateWaveSystem((float)deltaTime);
        updateZombies((float)deltaTime);
        processHealthPickups();
        collisionSystem.update(&world);
        handleCollisions();
        lockCameraAndPlayerVerticalToZero();
        zombieAnimationSystem.updateBloodSplashEffects(&world, (float)deltaTime);
        world.deleteMarkedEntities();

        float currentHealth = 100.0f;
        float maxHealth = 100.0f;
        if (auto health = getMainPlayerHealth(); health)
        {
            currentHealth = health->currentHealth;
            maxHealth = health->maxHealth;
        }
        renderer.setHealth(currentHealth, maxHealth, (float)deltaTime);

        if (!endingQueued)
        {
            bool playerDefeated = false;
            if (auto *health = getMainPlayerHealth(); health)
            {
                playerDefeated = (!health->isAlive) || (health->currentHealth <= 0.0f);
            }

            if (playerDefeated)
            {
                endingQueued = true;
                our::GameSession::setEndingResult(our::EndingOutcome::Lose, computeCurrentExposure());
                getApp()->changeState("ending");
            }
            else if (zombieWaveRuntime.allWavesCompleted && zombieSpawningSystem.getAliveZombieCount(&world) == 0)
            {
                endingQueued = true;
                our::GameSession::setEndingResult(our::EndingOutcome::Win, computeCurrentExposure());
                getApp()->changeState("ending");
            }
        }

        // Clean up finished audio sources
        our::AudioManager::getInstance().cleanupFinishedSources();

        // And finally we use the renderer system to draw the scene
        renderer.render(&world);

        // Draw ammo HUD over the final frame
        hudSystem.renderAmmoHUD(getApp()->getFrameBufferSize(), mainPlayerWeapon);

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
        if (isPaused)
            return;

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
                if (weapon->shoot())
                {
                    if (auto *player = mainPlayerEntity ? mainPlayerEntity->getComponent<our::PlayerComponent>() : nullptr)
                    {
                        player->shootRequested = true;
                    }

                    muzzleFlashTimeLeft = muzzleFlashDuration;
                    auto frameBufferSize = getApp()->getFrameBufferSize();
                    float crosshairX = 0.0f;
                    float crosshairY = 0.0f;
                    if (auto *player = mainPlayerEntity ? mainPlayerEntity->getComponent<our::PlayerComponent>() : nullptr)
                    {
                        crosshairX = player->crosshairX;
                        crosshairY = player->crosshairY;
                    }
                    our::Ray ray = shootingSystem.buildRayFromCamera(&world, frameBufferSize, crosshairX, crosshairY);
                    our::FireResult fireResult = shootingSystem.fireRay(ray, &world, weapon);

                    zombieAnimationSystem.handleZombieKill(
                        &world,
                        fireResult.hitEntity,
                        fireResult.killedZombie,
                        zombiesKilledCount,
                        zombieAnimationConfig);
                }
            }
        }
    }

    void onImmediateGui() override
    {
        renderPauseOverlay();
    }

    void onDestroy() override
    {
        setPauseMode(false);

        auto &audio = our::AudioManager::getInstance();
        audio.setEffectsEnabled(true);
        audio.setMusicEnabled(true);

        if (our::AudioManager::getInstance().isInitialized())
        {
            our::AudioManager::getInstance().stopLoopingSound(worldAmbientTrack);
        }

        pauseAssets.destroy();

        renderer.destroy();
        hudSystem.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        collisionSystem.clear();
        previousWallCollisionPairs.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};