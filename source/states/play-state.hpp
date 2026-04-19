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
#include <components/camera.hpp>
#include <components/environment.hpp>
#include <components/free-camera-controller.hpp>
#include <components/health.hpp>
#include <components/mesh-renderer.hpp>
#include <components/player.hpp>
#include <components/weapon.hpp>
#include <components/zombie.hpp>
#include <animation/motion.hpp>
#include <audio-manager.hpp>
#include <asset-loader.hpp>
#include <deserialize-utils.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
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
    our::MovementSystem movementSystem;
    our::ShootingSystem shootingSystem;
    our::CollisionSystem collisionSystem;
    std::string worldAmbientTrack = "assets/audio/world.wav";
    float worldAmbientGain = 0.45f;
    float muzzleFlashTimeLeft = 0.0f;
    const float muzzleFlashDuration = 0.06f;
    float totalTime = 0.0f;
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
    glm::vec3 mainPlayerPistolHandOffset = glm::vec3(0.20f, 0.95f, -0.06f);
    glm::vec3 mainPlayerPistolRotationOffset = glm::vec3(0.0f, glm::pi<float>(), 0.0f);
    float mainPlayerPistolScaleMultiplier = 0.03f;
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
    float bloodSplashLifetimeSeconds = 1.1f;
    float bloodSplashScaleMultiplier = 8.0f;
    float bloodSplashHeightOffset = 0.9f;

    struct BloodSplashFx
    {
        our::Entity *entity = nullptr;
        float timeLeft = 0.0f;
    };

    std::vector<BloodSplashFx> activeBloodSplashes;

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
        if (!mainPlayerPistolEntity)
            mainPlayerPistolEntity = findPistolEntity();
        if (!(mainPlayerPistolEntity && mainPlayerVisualEntity))
            return;

        glm::vec3 playerWorldPosition = glm::vec3(mainPlayerVisualEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));
        glm::quat playerWorldRotation = glm::quat(mainPlayerVisualEntity->localTransform.rotation);
        glm::vec3 handWorld = playerWorldPosition + (playerWorldRotation * mainPlayerPistolHandOffset);

        mainPlayerPistolEntity->parent = nullptr;
        mainPlayerPistolEntity->localTransform.position = handWorld;
        mainPlayerPistolEntity->localTransform.rotation = mainPlayerVisualEntity->localTransform.rotation + mainPlayerPistolRotationOffset;
        mainPlayerPistolEntity->localTransform.scale = mainPlayerPistolPrototypeTransform.scale * mainPlayerPistolScaleMultiplier;
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
        bool thirdPersonActive = glm::distance(cameraWorldPos, playerWorldPos) > 1.2f;
        glm::vec3 cameraForward = getCameraForwardOnGround();
        glm::vec3 anchorPosition = cameraWorldPos;
        if (thirdPersonActive)
        {
            anchorPosition += cameraForward * 2.1f;
        }

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
        auto &allCollisions = collisionSystem.getCurrentCollisions();

        for (const auto &collision : allCollisions)
        {
            our::Entity *entityA = collision.entityA;
            our::Entity *entityB = collision.entityB;

            auto envA = entityA->getComponent<our::EnvironmentComponent>();
            auto envB = entityB->getComponent<our::EnvironmentComponent>();

            bool isStaticA = envA && (envA->environmentType == "wall" || envA->environmentType == "floor");
            bool isStaticB = envB && (envB->environmentType == "wall" || envB->environmentType == "floor");
            bool isFloorA = envA && envA->environmentType == "floor";
            bool isFloorB = envB && envB->environmentType == "floor";

            if (isStaticA && isStaticB)
                continue;
            if (!isStaticA && !isStaticB)
                continue;

            our::Entity *dynamicEntity = isStaticA ? entityB : entityA;
            bool collidingWithFloor = isStaticA ? isFloorA : isFloorB;

            our::CollisionInfo oriented;
            if (isStaticB)
            {
                oriented = collision;
            }
            else
            {
                oriented.entityA = entityB;
                oriented.entityB = entityA;
                oriented.colliderA = collision.colliderB;
                oriented.colliderB = collision.colliderA;
            }

            glm::vec3 pushBack = collisionSystem.resolveAABB(oriented);

            if (collidingWithFloor)
            {
                pushBack.x = 0.0f;
                pushBack.z = 0.0f;
                if (pushBack.y < 0.0f)
                    pushBack.y = 0.0f;
            }

            const float epsilon = 0.001f;
            if (glm::length(pushBack) > epsilon)
            {
                dynamicEntity->localTransform.position += pushBack;
            }
        }
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

        loadZombieGameplayConfig(config);
        if (config.contains("mainPlayer") && config["mainPlayer"].is_object())
        {
            mainPlayerHeightOffset = config["mainPlayer"].value("heightOffset", mainPlayerHeightOffset);

            const auto &mainPlayerConfig = config["mainPlayer"];
            if (mainPlayerConfig.contains("pistolHandOffset") && mainPlayerConfig["pistolHandOffset"].is_array())
                mainPlayerPistolHandOffset = mainPlayerConfig["pistolHandOffset"].get<glm::vec3>();
            if (mainPlayerConfig.contains("pistolRotationOffset") && mainPlayerConfig["pistolRotationOffset"].is_array())
                mainPlayerPistolRotationOffset = glm::radians(mainPlayerConfig["pistolRotationOffset"].get<glm::vec3>());
            mainPlayerPistolScaleMultiplier = mainPlayerConfig.value("pistolScaleMultiplier", mainPlayerPistolScaleMultiplier);
        }
        bindZombieMotionClips();
        bindMainPlayerMotionClips();

        mainPlayerEntity = findMainPlayerEntity();
        mainCameraEntity = findMainCameraEntity(mainPlayerEntity);
        mainPlayerMesh = our::AssetLoader<our::Mesh>::get("main-player");
        mainPlayerVisualEntity = findMainPlayerVisualEntity();
        mainPlayerPistolEntity = findPistolEntity();
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
        if (mainPlayerPistolEntity)
        {
            mainPlayerPistolPrototypeTransform = mainPlayerPistolEntity->localTransform;
            updateMainPlayerPistolAttachment();
        }
        lockCameraAndPlayerVerticalToZero();
        cacheZombiePrototypeAndSpawnPoints();
        cacheBloodSplashAssets();
        recalculateSunriseTargets();
        zombiesKilledCount = 0;
        waitingForNextWave = true;
        betweenWaveTimer = initialWaveDelaySeconds;

        if (our::AudioManager::getInstance().isInitialized())
        {
            our::AudioManager::getInstance().playLoopingSound(worldAmbientTrack, worldAmbientGain);
        }

        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        hudSystem.initialize();
        our::SceneManager::validateWorld(&world);
        totalTime = 0.0f;
    }

    void onDraw(double deltaTime) override
    {
        totalTime += (float)deltaTime;
        renderer.setTime(totalTime);

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

    void onDestroy() override
    {
        if (our::AudioManager::getInstance().isInitialized())
        {
            our::AudioManager::getInstance().stopLoopingSound(worldAmbientTrack);
        }

        // Don't forget to destroy the renderer
        renderer.destroy();
        hudSystem.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        collisionSystem.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};