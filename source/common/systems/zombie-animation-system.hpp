#pragma once

#include "../animation/motion.hpp"
#include "../asset-loader.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/zombie.hpp"
#include "../ecs/entity.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <string>

namespace our
{

    struct ZombieClipOverrides
    {
        std::string walk = "Armature|Walk";
        std::string attack = "Armature|Attack";
        std::string crawl = "Armature|Crawl";
        std::string die = "Armature|Bite_ground";
        std::string crawlDie = "Armature|Bite_ground";
    };

    class ZombieAnimationSystem
    {
        Motion *zombieMotion = nullptr;
        const MotionClip *walkClip = nullptr;
        const MotionClip *attackClip = nullptr;
        const MotionClip *crawlClip = nullptr;
        const MotionClip *dieClip = nullptr;
        const MotionClip *crawlDieClip = nullptr;

        const MotionClip *firstAvailableClip() const
        {
            if (!(zombieMotion && !zombieMotion->clips.empty()))
                return nullptr;
            return &zombieMotion->clips[0];
        }

        const MotionClip *findClipByExactNameInsensitive(const std::string &clipName) const
        {
            if (!(zombieMotion && !clipName.empty()))
                return nullptr;

            auto toLower = [](std::string s)
            {
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                               { return static_cast<char>(std::tolower(c)); });
                return s;
            };

            std::string wanted = toLower(clipName);
            for (const auto &clip : zombieMotion->clips)
            {
                if (toLower(clip.name) == wanted)
                    return &clip;
            }
            return nullptr;
        }

    public:
        void bindMotionClips(const std::string &motionAssetName = "zombie-motion", const ZombieClipOverrides &overrides = {})
        {
            zombieMotion = AssetLoader<Motion>::get(motionAssetName);
            if (!zombieMotion)
            {
                std::cout << "[Motion] " << motionAssetName << " asset not found.\n";
                return;
            }

            walkClip = findClipByExactNameInsensitive(overrides.walk);
            attackClip = findClipByExactNameInsensitive(overrides.attack);
            crawlClip = findClipByExactNameInsensitive(overrides.crawl);
            dieClip = findClipByExactNameInsensitive(overrides.die);
            crawlDieClip = findClipByExactNameInsensitive(overrides.crawlDie);

            if (!walkClip)
                walkClip = firstAvailableClip();
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

        const MotionClip *getMotionClipForState(const ZombieComponent *zombie) const
        {
            if (!zombie)
                return walkClip;

            ZombieState state = zombie->state;
            if (state == ZombieState::Attacking)
                return attackClip;
            if (state == ZombieState::Crawling)
                return crawlClip;
            if (state == ZombieState::Dead)
            {
                if (zombie->shotsTaken >= 2 && crawlDieClip)
                    return crawlDieClip;
                return dieClip;
            }
            return walkClip;
        }

        void updateZombieAnimation(Entity *entity, ZombieComponent *zombie, float deltaTime) const
        {
            if (!zombie)
                return;

            const MotionClip *clip = getMotionClipForState(zombie);
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
                if (zombie->state == ZombieState::Dead)
                    zombie->motionClipTime = std::min(zombie->motionClipTime, clip->duration);
                else
                    zombie->motionClipTime = std::fmod(zombie->motionClipTime, clip->duration);
            }

            if (!(entity && zombieMotion))
                return;

            auto *renderer = entity->getComponent<MeshRendererComponent>();
            if (!(renderer && renderer->mesh && renderer->mesh->hasSkinning()))
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
    };

}
