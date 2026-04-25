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

#define ZOMBIE_ATTACK_TRIGGER_DISTANCE 1.8f
#define ZOMBIE_INITIAL_WAVE_DELAY_SECONDS 0.2f
#define ZOMBIE_BETWEEN_WAVES_DELAY_SECONDS 4.0f
#define ZOMBIE_MIN_SPAWN_PLAYER_DISTANCE 7.0f

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State
{

    our::World world;
    our::ForwardRenderer renderer;
    our::HUDSystem hudSystem;
    our::FreeCameraControllerSystem cameraController;
    our::PlayerControllerSystem playerController;
    our::MovementSystem movementSystem;
    our::ShootingSystem shootingSystem;
    our::CollisionSystem collisionSystem;
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

    our::Mesh *zombieMesh = nullptr;
    our::Material *zombieMaterial = nullptr;
    our::Mesh *bloodSplashMesh = nullptr;
    our::Material *bloodSplashMaterial = nullptr;
    our::Motion *zombieMotion = nullptr;
    const our::MotionClip *walkClip = nullptr;
    const our::MotionClip *attackClip = nullptr;
    const our::MotionClip *crawlClip = nullptr;
    const our::MotionClip *dieClip = nullptr;
    const our::MotionClip *crawlDieClip = nullptr;
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
    our::Transform zombiePrototypeTransform{};
    float zombieModelYawOffset = 0.0f;
    float zombieGroundY = -0.5f;
    std::vector<glm::vec3> zombieSpawnPoints;

    std::vector<int> waveZombieCounts = {3, 5, 7, 10};
    size_t currentWaveIndex = 0;
    int zombiesSpawnedThisWave = 0;
    float zombieSpawnIntervalSeconds = 0.8f;
    float zombieSpawnTimer = 0.0f;
    bool waitingForNextWave = true;
    float betweenWaveTimer = ZOMBIE_INITIAL_WAVE_DELAY_SECONDS;
    bool allWavesCompleted = false;
    int totalZombiesAcrossAllWaves = 0;
    int zombiesKilledCount = 0;
    float sunriseStartExposure = 0.10f;
    float sunriseEndExposure = 1.00f;
    float sunriseEasePower = 1.20f;

    float initialWaveDelaySeconds = ZOMBIE_INITIAL_WAVE_DELAY_SECONDS;
    float betweenWavesDelaySeconds = ZOMBIE_BETWEEN_WAVES_DELAY_SECONDS;
    float minSpawnPlayerDistance = ZOMBIE_MIN_SPAWN_PLAYER_DISTANCE;

    float zombieWalkSpeed = 1.8f;
    float zombieCrawlSpeed = 0.8f;
    float zombieDamage = 8.0f;
    float zombieAttackRange = ZOMBIE_ATTACK_TRIGGER_DISTANCE;
    float zombieAttackCooldown = 1.0f;
    float zombieCorpseLifetime = 1.25f;
    float zombieRadius = 1.0f;
    float zombieSpawnHeightOffset = 0.0f;
    float zombieModelScaleMultiplier = 1.0f;
    float zombieSpawnMaxDistance = 22.0f;
    float zombieSpawnViewHalfAngleDegrees = 24.0f;
    float zombieModelYawOffsetDegrees = 0.0f;

    float zombieWalkBobAmplitude = 0.055f;
    float zombieCrawlBobAmplitude = 0.025f;
    float zombieAttackBobAmplitude = 0.085f;
    float zombieWalkBobFrequency = 7.0f;
    float zombieCrawlBobFrequency = 4.2f;
    float zombieAttackBobFrequency = 11.0f;
    float zombieWalkRollDegrees = 7.0f;
    float zombieAttackPitchDegrees = 13.0f;
    float zombieAttackPitchFrequency = 8.0f;
    float zombieCrawlPitchDegrees = 58.0f;
    float zombieCrawlHeightDrop = 0.22f;
    float zombieDeathFallDegrees = 82.0f;
    float zombieDeathSink = 0.30f;
    float playerWallCollisionRetreatDistance = 0.12f;
    float playerVisualWallBufferDistance = 0.65f;
    float houseWallColliderExtraPaddingXZ = 1.4f;
    float bloodSplashLifetimeSeconds = 1.1f;
    float bloodSplashScaleMultiplier = 8.0f;
    float bloodSplashHeightOffset = 0.9f;

    struct BloodSplashFx
    {
        our::Entity *entity = nullptr;
        float timeLeft = 0.0f;
    };

    std::vector<BloodSplashFx> activeBloodSplashes;
    bool isPaused = false;
    bool musicEnabled = true;
    bool effectsEnabled = true;
    our::ui::pause::Assets pauseAssets{};

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

    const our::MotionClip *getMotionClipForState(const our::ZombieComponent *zombie) const
    {
        if (!zombie)
            return walkClip;

        our::ZombieState state = zombie->state;
        if (state == our::ZombieState::Attacking)
            return attackClip;
        if (state == our::ZombieState::Crawling)
            return crawlClip;
        if (state == our::ZombieState::Dead)
        {
            // Prefer a floor-style death animation for second-shot zombie deaths.
            if (zombie->shotsTaken >= 2 && crawlDieClip)
                return crawlDieClip;
            return dieClip;
        }
        return walkClip;
    }

    void updateZombieClipPlayback(our::ZombieComponent *zombie, float deltaTime)
    {
        const our::MotionClip *clip = getMotionClipForState(zombie);
        if (!clip)
            return;

        if (zombie->activeMotionClip != clip->name)
        {
            zombie->activeMotionClip = clip->name;
            zombie->motionClipTime = 0.0f;
        }
        else
        {
            zombie->motionClipTime += deltaTime;
        }

        if (clip->duration > 0.0001f)
        {
            if (zombie->state == our::ZombieState::Dead)
            {
                zombie->motionClipTime = std::min(zombie->motionClipTime, clip->duration);
            }
            else
            {
                zombie->motionClipTime = std::fmod(zombie->motionClipTime, clip->duration);
            }
        }
    }

    void updateZombieSkinMatrices(our::Entity *entity, our::ZombieComponent *zombie)
    {
        if (!(entity && zombie && zombieMotion))
        {
            return;
        }

        auto *renderer = entity->getComponent<our::MeshRendererComponent>();
        if (!(renderer && renderer->mesh && renderer->mesh->hasSkinning()))
        {
            zombie->skinMatrices.clear();
            return;
        }

        const our::MotionClip *clip = getMotionClipForState(zombie);
        if (!clip)
        {
            zombie->skinMatrices.clear();
            return;
        }

        if (!zombieMotion->computeSkinMatrices(
                clip,
                zombie->motionClipTime,
                renderer->mesh->getSkinJointNodes(),
                renderer->mesh->getInverseBindMatrices(),
                zombie->skinMatrices))
        {
            zombie->skinMatrices.clear();
        }
    }

    void bindZombieMotionClips()
    {
        zombieMotion = our::AssetLoader<our::Motion>::get("zombie-motion");
        if (!zombieMotion)
        {
            std::cout << "[Motion] zombie-motion asset not found.\n";
            return;
        }

        auto toLower = [](std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return s;
        };

        auto hasAnyKeyword = [&](const std::string &name, std::initializer_list<std::string> words)
        {
            std::string lowered = toLower(name);
            for (const auto &w : words)
            {
                if (lowered.find(toLower(w)) != std::string::npos)
                    return true;
            }
            return false;
        };

        // Pick a likely locomotion clip while avoiding crawl/attack/death clips.
        walkClip = nullptr;
        for (const auto &clip : zombieMotion->clips)
        {
            bool hasWalkWord = hasAnyKeyword(clip.name, {"walk", "run", "locomotion", "move"});
            bool hasBadWord = hasAnyKeyword(clip.name, {"crawl", "attack", "hit", "bite", "die", "death", "dead", "hurt", "injured"});
            if (hasWalkWord && !hasBadWord)
            {
                if (!walkClip || clip.duration > walkClip->duration)
                    walkClip = &clip;
            }
        }

        if (!walkClip)
            walkClip = zombieMotion->findClipByKeywords({"walk", "run", "locomotion", "move"});
        if (!walkClip)
            walkClip = zombieMotion->findClipByKeywords({"idle"});
        attackClip = zombieMotion->findClipByKeywords({"attack", "hit", "slash", "bite"});
        crawlClip = zombieMotion->findClipByKeywords({"crawl", "injured", "hurt"});

        // Split death clips into standing-style vs floor/prone-style.
        dieClip = nullptr;
        crawlDieClip = nullptr;
        for (const auto &clip : zombieMotion->clips)
        {
            const bool isDeath = hasAnyKeyword(clip.name, {"die", "death", "dead", "fall"});
            const bool isFloorStyle = hasAnyKeyword(clip.name, {"die2", "die_2", "death2", "ground", "prone", "lying", "bite_ground", "crawl"});

            if (isDeath && isFloorStyle)
            {
                if (!crawlDieClip || clip.duration > crawlDieClip->duration)
                    crawlDieClip = &clip;
            }
            else if (isDeath)
            {
                if (!dieClip || clip.duration > dieClip->duration)
                    dieClip = &clip;
            }
        }

        if (!dieClip)
            dieClip = zombieMotion->findClipByKeywords({"die", "death", "dead", "fall"});
        if (!crawlDieClip)
            crawlDieClip = zombieMotion->findClipByKeywords({"die2", "die_2", "death2", "ground", "prone", "lying", "bite_ground"});

        if (!walkClip && !zombieMotion->clips.empty())
            walkClip = &zombieMotion->clips[0];
        if (!attackClip)
            attackClip = walkClip;
        if (!crawlClip)
            crawlClip = walkClip;
        if (!dieClip)
            dieClip = walkClip;
        if (!crawlDieClip)
            crawlDieClip = dieClip;

        std::cout << "[Motion] Loaded " << zombieMotion->clips.size() << " clip(s) from " << zombieMotion->sourcePath << "\n";
        for (const auto &clip : zombieMotion->clips)
        {
            std::cout << "  - " << clip.name << " | duration=" << clip.duration << " | channels=" << clip.channelCount << "\n";
        }
        std::cout << "[Motion] Bound clips -> walk: " << (walkClip ? walkClip->name : "none")
                  << ", attack: " << (attackClip ? attackClip->name : "none")
                  << ", crawl: " << (crawlClip ? crawlClip->name : "none")
                  << ", die: " << (dieClip ? dieClip->name : "none")
                  << ", crawl-die: " << (crawlDieClip ? crawlDieClip->name : "none") << "\n";
    }

    void loadZombieGameplayConfig(const nlohmann::json &sceneConfig)
    {
        if (!sceneConfig.contains("zombies"))
            return;
        const auto &zombiesConfig = sceneConfig["zombies"];
        if (!zombiesConfig.is_object())
            return;

        if (zombiesConfig.contains("waveZombieCounts") && zombiesConfig["waveZombieCounts"].is_array())
        {
            std::vector<int> parsedCounts;
            for (const auto &v : zombiesConfig["waveZombieCounts"])
            {
                if (!v.is_number_integer())
                    continue;
                int c = v.get<int>();
                if (c > 0)
                    parsedCounts.push_back(c);
            }
            if (!parsedCounts.empty())
            {
                waveZombieCounts = parsedCounts;
            }
        }

        zombieSpawnIntervalSeconds = std::max(0.01f, zombiesConfig.value("spawnIntervalSeconds", zombieSpawnIntervalSeconds));
        initialWaveDelaySeconds = std::max(0.0f, zombiesConfig.value("initialWaveDelaySeconds", initialWaveDelaySeconds));
        betweenWavesDelaySeconds = std::max(0.0f, zombiesConfig.value("betweenWavesDelaySeconds", betweenWavesDelaySeconds));
        minSpawnPlayerDistance = std::max(0.0f, zombiesConfig.value("minSpawnPlayerDistance", minSpawnPlayerDistance));

        zombieWalkSpeed = std::max(0.0f, zombiesConfig.value("walkSpeed", zombieWalkSpeed));
        zombieCrawlSpeed = std::max(0.0f, zombiesConfig.value("crawlSpeed", zombieCrawlSpeed));
        zombieDamage = std::max(0.0f, zombiesConfig.value("damage", zombieDamage));
        zombieAttackRange = std::max(0.05f, zombiesConfig.value("attackRange", zombieAttackRange));
        zombieAttackCooldown = std::max(0.01f, zombiesConfig.value("attackCooldown", zombieAttackCooldown));
        zombieCorpseLifetime = std::max(0.0f, zombiesConfig.value("corpseLifetime", zombieCorpseLifetime));
        zombieRadius = std::max(0.05f, zombiesConfig.value("radius", zombieRadius));
        zombieSpawnHeightOffset = zombiesConfig.value("spawnHeightOffset", zombieSpawnHeightOffset);
        zombieModelScaleMultiplier = std::max(0.05f, zombiesConfig.value("modelScaleMultiplier", zombieModelScaleMultiplier));
        zombieSpawnMaxDistance = std::max(minSpawnPlayerDistance + 0.1f, zombiesConfig.value("spawnMaxDistance", zombieSpawnMaxDistance));
        zombieSpawnViewHalfAngleDegrees = std::clamp(zombiesConfig.value("spawnViewHalfAngleDegrees", zombieSpawnViewHalfAngleDegrees), 1.0f, 85.0f);
        zombieModelYawOffsetDegrees = zombiesConfig.value("modelYawOffsetDegrees", zombieModelYawOffsetDegrees);
        zombieModelYawOffset = glm::radians(zombieModelYawOffsetDegrees);

        zombieWalkBobAmplitude = std::max(0.0f, zombiesConfig.value("walkBobAmplitude", zombieWalkBobAmplitude));
        zombieCrawlBobAmplitude = std::max(0.0f, zombiesConfig.value("crawlBobAmplitude", zombieCrawlBobAmplitude));
        zombieAttackBobAmplitude = std::max(0.0f, zombiesConfig.value("attackBobAmplitude", zombieAttackBobAmplitude));
        zombieWalkBobFrequency = std::max(0.0f, zombiesConfig.value("walkBobFrequency", zombieWalkBobFrequency));
        zombieCrawlBobFrequency = std::max(0.0f, zombiesConfig.value("crawlBobFrequency", zombieCrawlBobFrequency));
        zombieAttackBobFrequency = std::max(0.0f, zombiesConfig.value("attackBobFrequency", zombieAttackBobFrequency));
        zombieWalkRollDegrees = std::max(0.0f, zombiesConfig.value("walkRollDegrees", zombieWalkRollDegrees));
        zombieAttackPitchDegrees = std::max(0.0f, zombiesConfig.value("attackPitchDegrees", zombieAttackPitchDegrees));
        zombieAttackPitchFrequency = std::max(0.0f, zombiesConfig.value("attackPitchFrequency", zombieAttackPitchFrequency));
        zombieCrawlPitchDegrees = std::max(0.0f, zombiesConfig.value("crawlPitchDegrees", zombieCrawlPitchDegrees));
        zombieCrawlHeightDrop = std::max(0.0f, zombiesConfig.value("crawlHeightDrop", zombieCrawlHeightDrop));
        zombieDeathFallDegrees = std::max(0.0f, zombiesConfig.value("deathFallDegrees", zombieDeathFallDegrees));
        zombieDeathSink = std::max(0.0f, zombiesConfig.value("deathSink", zombieDeathSink));

        bloodSplashLifetimeSeconds = std::max(0.05f, zombiesConfig.value("bloodSplashLifetimeSeconds", bloodSplashLifetimeSeconds));
        bloodSplashScaleMultiplier = std::max(0.05f, zombiesConfig.value("bloodSplashScaleMultiplier", bloodSplashScaleMultiplier));
        bloodSplashHeightOffset = zombiesConfig.value("bloodSplashHeightOffset", bloodSplashHeightOffset);

        sunriseStartExposure = std::clamp(zombiesConfig.value("sunriseStartExposure", sunriseStartExposure), 0.0f, 2.0f);
        sunriseEndExposure = std::clamp(zombiesConfig.value("sunriseEndExposure", sunriseEndExposure), 0.0f, 2.0f);
        sunriseEasePower = std::max(0.05f, zombiesConfig.value("sunriseEasePower", sunriseEasePower));
    }

    void recalculateSunriseTargets()
    {
        totalZombiesAcrossAllWaves = 0;
        for (int waveCount : waveZombieCounts)
        {
            if (waveCount > 0)
                totalZombiesAcrossAllWaves += waveCount;
        }
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

    void cacheZombiePrototypeAndSpawnPoints()
    {
        zombieMesh = our::AssetLoader<our::Mesh>::get("zombie");
        zombieMaterial = our::AssetLoader<our::Material>::get("zombie_theme");
        if (!zombieMaterial)
            zombieMaterial = our::AssetLoader<our::Material>::get("auto");
        zombiePrototypeTransform = our::Transform{};
        zombiePrototypeTransform.position = glm::vec3(0.0f, -0.5f, 0.0f);

        std::vector<our::Entity *> startupZombieEntities;
        for (auto entity : world.getEntities())
        {
            auto *renderer = entity->getComponent<our::MeshRendererComponent>();
            if (!(renderer && renderer->mesh == zombieMesh))
                continue;

            startupZombieEntities.push_back(entity);
            zombieSpawnPoints.push_back(entity->localTransform.position);
            zombiePrototypeTransform = entity->localTransform;
            if (renderer->material)
                zombieMaterial = renderer->material;
        }

        for (auto entity : startupZombieEntities)
        {
            world.markForRemoval(entity);
        }
        world.deleteMarkedEntities();

        zombieGroundY = zombiePrototypeTransform.position.y + zombieSpawnHeightOffset;

        if (zombieSpawnPoints.empty())
        {
            zombieSpawnPoints.push_back(glm::vec3(0.0f, -0.5f, 2.0f));
            zombieSpawnPoints.push_back(glm::vec3(5.0f, -0.5f, 4.0f));
            zombieSpawnPoints.push_back(glm::vec3(-5.0f, -0.5f, 4.0f));
            zombieSpawnPoints.push_back(glm::vec3(0.0f, -0.5f, -2.0f));
        }

        std::cout << "[Zombies] mesh=" << (zombieMesh ? "loaded" : "missing")
                  << ", gltfBaseColorTexture=" << ((zombieMesh && zombieMesh->hasGLTFBaseColorTexture()) ? "yes" : "no")
                  << ", material=" << (zombieMaterial ? "loaded" : "missing") << "\n";
    }

    void cacheBloodSplashAssets()
    {
        bloodSplashMesh = our::AssetLoader<our::Mesh>::get("blood-splash");
        if (!bloodSplashMesh)
            bloodSplashMesh = our::AssetLoader<our::Mesh>::get("blood");
        if (!bloodSplashMesh)
            bloodSplashMesh = our::AssetLoader<our::Mesh>::get("Blood");

        bloodSplashMaterial = our::AssetLoader<our::Material>::get("blood-fx");
        if (!bloodSplashMaterial)
            bloodSplashMaterial = our::AssetLoader<our::Material>::get("auto");

        // Some GLB blood assets rely on textures with alpha that may end up effectively invisible in this pipeline.
        // Force no mesh texture so tinted material color is always visible.
        if (bloodSplashMesh)
        {
            bloodSplashMesh->setGLTFBaseColorTexture(0);
            bloodSplashMesh->setGLTFBaseColorFactor(glm::vec4(1.0f));
        }

        std::cout << "[BloodFX] mesh=" << (bloodSplashMesh ? "loaded" : "missing")
                  << ", material=" << (bloodSplashMaterial ? "loaded" : "missing")
                  << ", lifetime=" << bloodSplashLifetimeSeconds
                  << ", scale=" << bloodSplashScaleMultiplier
                  << ", yOffset=" << bloodSplashHeightOffset << "\n";
    }

    bool spawnBloodSplashAt(const glm::vec3 &position, float yaw, float sourceScale)
    {
        if (!bloodSplashMesh)
        {
            std::cout << "[BloodFX] spawn skipped: blood mesh not loaded\n";
            return false;
        }

        auto makeSplashEntity = [&](const glm::vec3 &rotation)
        {
            our::Entity *splash = world.add();
            splash->name = "BloodSplash";
            splash->parent = nullptr;
            splash->localTransform = zombiePrototypeTransform;
            splash->localTransform.position = position;
            splash->localTransform.position.y += bloodSplashHeightOffset;
            splash->localTransform.rotation = rotation;
            float finalScale = std::max(0.05f, sourceScale * bloodSplashScaleMultiplier);
            splash->localTransform.scale = glm::vec3(finalScale);

            auto *renderer = splash->addComponent<our::MeshRendererComponent>();
            renderer->mesh = bloodSplashMesh;
            renderer->material = bloodSplashMaterial ? bloodSplashMaterial : zombieMaterial;

            activeBloodSplashes.push_back({splash, bloodSplashLifetimeSeconds});
        };

        // Spawn as a crossed pair to keep visibility even if the asset is a thin card.
        makeSplashEntity(glm::vec3(0.0f, yaw, 0.0f));
        makeSplashEntity(glm::vec3(glm::half_pi<float>(), yaw, 0.0f));

        std::cout << "[BloodFX] spawned at ("
                  << position.x << ", " << position.y << ", " << position.z
                  << ") with scale=" << (sourceScale * bloodSplashScaleMultiplier) << "\n";
        return true;
    }

    void updateBloodSplashEffects(float deltaTime)
    {
        for (auto &fx : activeBloodSplashes)
        {
            fx.timeLeft -= deltaTime;
            if (fx.timeLeft <= 0.0f && fx.entity)
            {
                world.markForRemoval(fx.entity);
                fx.entity = nullptr;
            }
        }

        activeBloodSplashes.erase(
            std::remove_if(activeBloodSplashes.begin(), activeBloodSplashes.end(), [](const BloodSplashFx &fx)
                           { return fx.timeLeft <= 0.0f || fx.entity == nullptr; }),
            activeBloodSplashes.end());
    }

    glm::vec3 pickSpawnPoint(int spawnedIndex)
    {
        glm::vec3 playerPos = getPlayerTargetPosition();
        glm::vec3 forward = getCameraForwardOnGround();

        // Deterministic spread inside the camera view cone to keep zombies spawning in front of player.
        int hash = spawnedIndex * 73 + static_cast<int>(currentWaveIndex) * 131 + 17;
        float tAngle = static_cast<float>(hash % 1000) / 999.0f; // [0,1]
        float tDist = static_cast<float>((hash * 37) % 1000) / 999.0f;

        float halfAngleRad = glm::radians(zombieSpawnViewHalfAngleDegrees);
        float angle = (tAngle * 2.0f - 1.0f) * halfAngleRad;

        float c = std::cos(angle);
        float s = std::sin(angle);
        glm::vec3 dir;
        dir.x = forward.x * c - forward.z * s;
        dir.y = 0.0f;
        dir.z = forward.x * s + forward.z * c;
        if (glm::dot(dir, dir) < 0.0001f)
            dir = forward;
        else
            dir = glm::normalize(dir);

        float distance = minSpawnPlayerDistance + (zombieSpawnMaxDistance - minSpawnPlayerDistance) * tDist;
        glm::vec3 spawn = playerPos + dir * distance;
        spawn.y = zombieGroundY;
        return spawn;
    }

    void spawnZombie(const glm::vec3 &spawnPosition)
    {
        if (!zombieMesh)
            return;

        our::Entity *zombieEntity = world.add();
        zombieEntity->name = "WaveZombie_" + std::to_string(currentWaveIndex + 1) + "_" + std::to_string(zombiesSpawnedThisWave + 1);
        zombieEntity->parent = nullptr;
        zombieEntity->localTransform = zombiePrototypeTransform;
        zombieEntity->localTransform.position = spawnPosition;
        zombieEntity->localTransform.position.y = zombieGroundY;
        zombieEntity->localTransform.scale *= zombieModelScaleMultiplier;

        auto *renderer = zombieEntity->addComponent<our::MeshRendererComponent>();
        renderer->mesh = zombieMesh;
        renderer->material = zombieMaterial;

        auto *health = zombieEntity->addComponent<our::HealthComponent>();
        health->maxHealth = 2.0f;
        health->currentHealth = 2.0f;
        health->isAlive = true;

        auto *zombie = zombieEntity->addComponent<our::ZombieComponent>();
        zombie->state = our::ZombieState::Walking;
        zombie->shotsTaken = 0;
        zombie->radius = zombieRadius;
        zombie->speed = zombieWalkSpeed;
        zombie->crawlSpeed = zombieCrawlSpeed;
        zombie->damage = zombieDamage;
        zombie->attackRange = zombieAttackRange;
        zombie->attackCooldown = zombieAttackCooldown;
        zombie->corpseLifetime = zombieCorpseLifetime;
        zombie->baseY = zombieGroundY;

        if (zombieMesh && zombieMesh->hasSkinning())
        {
            zombie->skinMatrices.assign(zombieMesh->getSkinJointNodes().size(), glm::mat4(1.0f));
        }

        glm::vec3 playerPos = getPlayerTargetPosition();
        glm::vec3 toPlayer = playerPos - zombieEntity->localTransform.position;
        toPlayer.y = 0.0f;
        if (glm::dot(toPlayer, toPlayer) > 0.0001f)
        {
            glm::vec3 direction = glm::normalize(toPlayer);
            float yaw = std::atan2(-direction.x, -direction.z);
            zombieEntity->localTransform.rotation.y = yaw + zombieModelYawOffset;
        }
    }

    int getAliveZombieCount()
    {
        int alive = 0;
        for (auto entity : world.getEntities())
        {
            auto *zombie = entity->getComponent<our::ZombieComponent>();
            auto *health = entity->getComponent<our::HealthComponent>();
            if (zombie && health && health->isAlive)
                alive++;
        }
        return alive;
    }

    void beginWave(size_t waveIndex)
    {
        currentWaveIndex = waveIndex;
        zombiesSpawnedThisWave = 0;
        zombieSpawnTimer = 0.0f;
        waitingForNextWave = false;
    }

    void updateWaveSystem(float deltaTime)
    {
        if (allWavesCompleted)
            return;

        if (waitingForNextWave)
        {
            betweenWaveTimer -= deltaTime;
            if (betweenWaveTimer <= 0.0f)
            {
                beginWave(currentWaveIndex);
            }
            return;
        }

        int targetForWave = waveZombieCounts[currentWaveIndex];
        zombieSpawnTimer -= deltaTime;
        while (zombiesSpawnedThisWave < targetForWave && zombieSpawnTimer <= 0.0f)
        {
            spawnZombie(pickSpawnPoint(zombiesSpawnedThisWave));
            zombiesSpawnedThisWave++;
            zombieSpawnTimer += zombieSpawnIntervalSeconds;
        }

        if (zombiesSpawnedThisWave >= targetForWave && getAliveZombieCount() == 0)
        {
            if (currentWaveIndex + 1 >= waveZombieCounts.size())
            {
                allWavesCompleted = true;
                waitingForNextWave = false;
            }
            else
            {
                waitingForNextWave = true;
                currentWaveIndex++;
                betweenWaveTimer = betweenWavesDelaySeconds;
            }
        }
    }

    void updateZombies(float deltaTime)
    {
        glm::vec3 playerTarget = getPlayerTargetPosition();
        our::HealthComponent *playerHealth = getMainPlayerHealth();

        for (auto entity : world.getEntities())
        {
            auto *zombie = entity->getComponent<our::ZombieComponent>();
            auto *health = entity->getComponent<our::HealthComponent>();
            if (!(zombie && health))
                continue;

            zombie->update(deltaTime);
            updateZombieClipPlayback(zombie, deltaTime);
            updateZombieSkinMatrices(entity, zombie);

            if (!health->isAlive)
            {
                zombie->state = our::ZombieState::Dead;
            }

            if (zombie->isDead())
            {
                float corpseDuration = std::max(0.01f, zombie->corpseLifetime);
                float t = std::clamp(zombie->deathTime / corpseDuration, 0.0f, 1.0f);
                entity->localTransform.rotation.x = -glm::radians(zombieDeathFallDegrees) * t;
                entity->localTransform.rotation.z = 0.0f;
                entity->localTransform.position.y = zombie->baseY - zombieDeathSink * t;

                if (zombie->shouldDespawn())
                {
                    world.markForRemoval(entity);
                }
                continue;
            }

            glm::vec3 zombiePosition = glm::vec3(entity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
            glm::vec3 toPlayer = playerTarget - zombiePosition;
            toPlayer.y = 0.0f;
            float distanceToPlayer = glm::length(toPlayer);

            if (distanceToPlayer > 0.0001f)
            {
                glm::vec3 direction = toPlayer / distanceToPlayer;
                float yaw = std::atan2(-direction.x, -direction.z);
                entity->localTransform.rotation.y = yaw + zombieModelYawOffset;

                float desiredCombatDistance = zombie->attackRange;
                if (distanceToPlayer > desiredCombatDistance)
                {
                    if (zombie->shotsTaken >= 1)
                    {
                        zombie->state = our::ZombieState::Crawling;
                    }
                    else
                    {
                        zombie->state = our::ZombieState::Walking;
                    }
                    entity->localTransform.position += direction * (zombie->getCurrentSpeed() * deltaTime);
                }
                else
                {
                    zombie->state = our::ZombieState::Attacking;

                    if (playerHealth && playerHealth->isAlive && zombie->canAttack())
                    {
                        playerHealth->takeDamage(zombie->damage);
                        zombie->resetAttackCooldown();
                    }
                }

                const bool usingSkinnedAnimation = !zombie->skinMatrices.empty();

                if (usingSkinnedAnimation)
                {
                    // Let the skeleton drive pose; keep entity transform stable.
                    entity->localTransform.position.y = zombie->baseY;
                    entity->localTransform.rotation.x = 0.0f;
                    entity->localTransform.rotation.z = 0.0f;
                    entity->localTransform.scale = zombiePrototypeTransform.scale * zombieModelScaleMultiplier;
                }
                else
                {
                    float bobAmplitude = zombieWalkBobAmplitude;
                    float bobFrequency = zombieWalkBobFrequency;
                    float posePitch = 0.0f;
                    float poseRoll = 0.0f;
                    float poseBaseY = zombie->baseY;
                    float attackPitchFrequencyLocal = zombieAttackPitchFrequency;

                    if (const our::MotionClip *active = getMotionClipForState(zombie); active && active->duration > 0.0001f)
                    {
                        float baseFrequency = glm::two_pi<float>() / active->duration;
                        if (zombie->state == our::ZombieState::Attacking)
                        {
                            bobFrequency = baseFrequency;
                            attackPitchFrequencyLocal = baseFrequency;
                        }
                        else if (zombie->state == our::ZombieState::Crawling)
                            bobFrequency = baseFrequency;
                        else if (zombie->state == our::ZombieState::Walking)
                            bobFrequency = baseFrequency;
                    }

                    if (zombie->state == our::ZombieState::Crawling)
                    {
                        bobAmplitude = zombieCrawlBobAmplitude;
                        bobFrequency = zombieCrawlBobFrequency;
                        posePitch = glm::radians(zombieCrawlPitchDegrees);
                        poseBaseY -= zombieCrawlHeightDrop;
                    }
                    else if (zombie->state == our::ZombieState::Attacking)
                    {
                        bobAmplitude = zombieAttackBobAmplitude;
                        float attackPoseOsc = std::abs(std::sin(zombie->motionTime * attackPitchFrequencyLocal));
                        posePitch = glm::radians(zombieAttackPitchDegrees) * attackPoseOsc;
                    }
                    else
                    {
                        poseRoll = glm::radians(zombieWalkRollDegrees) * std::sin(zombie->motionTime * 0.5f * zombieWalkBobFrequency);
                    }

                    float bob = (bobAmplitude > 0.0f && bobFrequency > 0.0f)
                                    ? std::sin(zombie->motionTime * bobFrequency) * bobAmplitude
                                    : 0.0f;

                    entity->localTransform.position.y = poseBaseY + bob;
                    entity->localTransform.rotation.x = -posePitch;
                    entity->localTransform.rotation.z = poseRoll;
                    entity->localTransform.scale = zombiePrototypeTransform.scale * zombieModelScaleMultiplier;
                }
            }
        }
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
                if (!isStaticA && !isStaticB)
                    continue;

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
        currentWaveIndex = 0;
        zombiesSpawnedThisWave = 0;
        zombieSpawnTimer = 0.0f;
        waitingForNextWave = true;
        betweenWaveTimer = initialWaveDelaySeconds;
        allWavesCompleted = false;
        zombiesKilledCount = 0;
        endingQueued = false;
        totalTime = 0.0f;
        collisionSfxCooldownLeft = 0.0f;
        pendingOuchSfxTimeLeft = -1.0f;
        muzzleFlashTimeLeft = 0.0f;
        mainPlayerAnchorInitialized = false;
        lastMainPlayerAnchorPosition = glm::vec3(0.0f);
        activeBloodSplashes.clear();

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

        if (config.contains("renderer") && config["renderer"].is_object())
        {
            const auto &rendererConfig = config["renderer"];
            if (rendererConfig.contains("muzzleFlashCenter") && rendererConfig["muzzleFlashCenter"].is_array())
            {
                const auto &flashCenter = rendererConfig["muzzleFlashCenter"];
                if (flashCenter.size() >= 2 && flashCenter[0].is_number() && flashCenter[1].is_number())
                {
                    muzzleFlashCenter = glm::vec2(
                        flashCenter[0].get<float>(),
                        flashCenter[1].get<float>());
                    muzzleFlashCenter = glm::clamp(muzzleFlashCenter, glm::vec2(0.0f), glm::vec2(1.0f));
                }
            }
        }

        bindZombieMotionClips();
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
            mainPlayerModelYawOffset = mainPlayerVisualPrototypeTransform.rotation.y;

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
        cacheZombiePrototypeAndSpawnPoints();
        cacheBloodSplashAssets();
        recalculateSunriseTargets();
        betweenWaveTimer = initialWaveDelaySeconds;
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

        auto &mouse = getApp()->getMouse();
        auto windowSize = getApp()->getWindowSize();
        playerController.update(&world, (float)deltaTime, mouse.getMouseDelta().x, mouse.getMouseDelta().y, windowSize.x, windowSize.y, getApp()->getWindow());

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
        updateBloodSplashEffects((float)deltaTime);
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
            else if (allWavesCompleted && getAliveZombieCount() == 0)
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
                    our::Ray ray = shootingSystem.buildRayFromCamera(&world);
                    our::FireResult fireResult = shootingSystem.fireRay(ray, &world, weapon);

                    // Spawn blood instantly on second-shot kill and remove zombie before render.
                    if (fireResult.killedZombie && fireResult.hitEntity)
                    {
                        zombiesKilledCount++;
                        glm::vec3 worldPos = glm::vec3(fireResult.hitEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
                        float maxAxisScale = std::max({std::abs(fireResult.hitEntity->localTransform.scale.x),
                                                       std::abs(fireResult.hitEntity->localTransform.scale.y),
                                                       std::abs(fireResult.hitEntity->localTransform.scale.z),
                                                       1.0f});
                        spawnBloodSplashAt(worldPos, fireResult.hitEntity->localTransform.rotation.y, maxAxisScale);
                        world.markForRemoval(fireResult.hitEntity);
                    }
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