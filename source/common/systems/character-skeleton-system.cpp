#include "character-skeleton-system.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace
{
    std::string toLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::string normalizeBoneName(const std::string &name)
    {
        std::string normalized;
        normalized.reserve(name.size());
        for (unsigned char c : name)
        {
            if (std::isalnum(c))
                normalized.push_back(static_cast<char>(std::tolower(c)));
        }
        return normalized;
    }

    size_t findFrameIndex(const std::vector<float> &times, float t)
    {
        if (times.size() <= 1)
            return 0;
        if (t <= times.front())
            return 0;
        if (t >= times.back())
            return times.size() - 2;

        auto it = std::upper_bound(times.begin(), times.end(), t);
        size_t i1 = static_cast<size_t>(std::distance(times.begin(), it));
        if (i1 == 0)
            return 0;
        return i1 - 1;
    }

    glm::vec4 sampleSampler(const our::MotionSampler &sampler, float t)
    {
        if (sampler.times.empty() || sampler.values.empty())
            return glm::vec4(0.0f);

        if (sampler.times.size() == 1 || sampler.values.size() == 1)
            return sampler.values.front();

        size_t i0 = findFrameIndex(sampler.times, t);
        size_t i1 = std::min(i0 + 1, sampler.times.size() - 1);

        float t0 = sampler.times[i0];
        float t1 = sampler.times[i1];
        float alpha = (t1 > t0) ? ((t - t0) / (t1 - t0)) : 0.0f;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        if (sampler.path == our::MotionPath::Rotation)
        {
            glm::quat q0 = glm::normalize(glm::quat(sampler.values[i0].w, sampler.values[i0].x, sampler.values[i0].y, sampler.values[i0].z));
            glm::quat q1 = glm::normalize(glm::quat(sampler.values[i1].w, sampler.values[i1].x, sampler.values[i1].y, sampler.values[i1].z));
            glm::quat qs = glm::normalize(glm::slerp(q0, q1, alpha));
            return glm::vec4(qs.x, qs.y, qs.z, qs.w);
        }

        return glm::mix(sampler.values[i0], sampler.values[i1], alpha);
    }
}

namespace our
{
    void CharacterSkeletonSystem::rebuildBoneLookup()
    {
        boneIndexByName.clear();
        boneIndexByLowerName.clear();
        boneIndexByNormalizedName.clear();
        resolvedBoneQueryCache.clear();
        if (!motion)
            return;

        boneIndexByName.reserve(motion->nodes.size());
        boneIndexByLowerName.reserve(motion->nodes.size());
        boneIndexByNormalizedName.reserve(motion->nodes.size());
        for (size_t i = 0; i < motion->nodes.size(); ++i)
        {
            const std::string &name = motion->nodes[i].name;
            if (!name.empty() && !boneIndexByName.count(name))
            {
                const int index = static_cast<int>(i);
                boneIndexByName[name] = index;

                const std::string lowered = toLower(name);
                if (!boneIndexByLowerName.count(lowered))
                    boneIndexByLowerName[lowered] = index;

                const std::string normalized = normalizeBoneName(name);
                if (!normalized.empty() && !boneIndexByNormalizedName.count(normalized))
                    boneIndexByNormalizedName[normalized] = index;
            }
        }
    }

    void CharacterSkeletonSystem::ensurePoseStorage()
    {
        const size_t nodeCount = motion ? motion->nodes.size() : 0;
        localT.resize(nodeCount, glm::vec3(0.0f));
        localR.resize(nodeCount, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        localS.resize(nodeCount, glm::vec3(1.0f));
        localM.resize(nodeCount, glm::mat4(1.0f));
        globalM.resize(nodeCount, glm::mat4(1.0f));
        computed.resize(nodeCount, 0);
    }

    void CharacterSkeletonSystem::sampleLocalPose(float localTime)
    {
        if (!motion)
            return;

        for (size_t i = 0; i < motion->nodes.size(); ++i)
        {
            localT[i] = motion->nodes[i].baseTranslation;
            localR[i] = motion->nodes[i].baseRotation;
            localS[i] = motion->nodes[i].baseScale;
            computed[i] = 0;
        }

        if (!clip)
        {
            for (size_t i = 0; i < motion->nodes.size(); ++i)
            {
                localM[i] = glm::translate(glm::mat4(1.0f), localT[i]) * glm::mat4_cast(localR[i]) * glm::scale(glm::mat4(1.0f), localS[i]);
            }
            return;
        }

        float sampleTime = localTime;
        if (clip->duration > 0.0001f)
        {
            sampleTime = std::fmod(localTime, clip->duration);
            if (sampleTime < 0.0f)
                sampleTime += clip->duration;
            sampleTime += clip->startTime;
        }

        for (const auto &channel : clip->channels)
        {
            if (channel.targetNode < 0 || channel.targetNode >= static_cast<int>(motion->nodes.size()))
                continue;
            if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(clip->samplers.size()))
                continue;

            const auto &sampler = clip->samplers[channel.samplerIndex];
            if (sampler.times.empty() || sampler.values.empty())
                continue;

            glm::vec4 sampled = sampleSampler(sampler, sampleTime);
            if (channel.path == MotionPath::Translation)
                localT[channel.targetNode] = glm::vec3(sampled);
            else if (channel.path == MotionPath::Scale)
                localS[channel.targetNode] = glm::vec3(sampled);
            else if (channel.path == MotionPath::Rotation)
                localR[channel.targetNode] = glm::normalize(glm::quat(sampled.w, sampled.x, sampled.y, sampled.z));
        }

        for (size_t i = 0; i < motion->nodes.size(); ++i)
        {
            localM[i] = glm::translate(glm::mat4(1.0f), localT[i]) * glm::mat4_cast(localR[i]) * glm::scale(glm::mat4(1.0f), localS[i]);
        }
    }

    glm::mat4 CharacterSkeletonSystem::computeGlobalNodeMatrix(int nodeIndex)
    {
        if (!motion || nodeIndex < 0 || nodeIndex >= static_cast<int>(motion->nodes.size()))
            return glm::mat4(1.0f);
        if (computed[nodeIndex])
            return globalM[nodeIndex];

        const int parent = motion->nodes[nodeIndex].parent;
        if (parent >= 0 && parent < static_cast<int>(motion->nodes.size()))
            globalM[nodeIndex] = computeGlobalNodeMatrix(parent) * localM[nodeIndex];
        else
            globalM[nodeIndex] = localM[nodeIndex];

        computed[nodeIndex] = 1;
        return globalM[nodeIndex];
    }

    void CharacterSkeletonSystem::bindRuntimePose(const Motion *motionAsset, const MotionClip *motionClip, float localTime, const glm::mat4 &characterWorld)
    {
        bool motionChanged = motion != motionAsset;
        motion = motionAsset;
        clip = motionClip;
        characterWorldMatrix = characterWorld;

        if (!motion)
            return;

        if (motionChanged)
            rebuildBoneLookup();

        ensurePoseStorage();
        sampleLocalPose(localTime);

        for (size_t i = 0; i < motion->nodes.size(); ++i)
            computeGlobalNodeMatrix(static_cast<int>(i));
    }

    glm::mat4 CharacterSkeletonSystem::getBoneWorldMatrix(std::string boneName) const
    {
        if (!motion)
            return characterWorldMatrix;

        auto cached = resolvedBoneQueryCache.find(boneName);
        int nodeIndex = -1;
        if (cached != resolvedBoneQueryCache.end())
        {
            nodeIndex = cached->second;
        }
        else
        {
            auto itExact = boneIndexByName.find(boneName);
            if (itExact != boneIndexByName.end())
            {
                nodeIndex = itExact->second;
            }
            else
            {
                const std::string lowered = toLower(boneName);
                auto itLower = boneIndexByLowerName.find(lowered);
                if (itLower != boneIndexByLowerName.end())
                {
                    nodeIndex = itLower->second;
                }
                else
                {
                    const std::string normalized = normalizeBoneName(boneName);
                    auto itNormalized = boneIndexByNormalizedName.find(normalized);
                    if (itNormalized != boneIndexByNormalizedName.end())
                    {
                        nodeIndex = itNormalized->second;
                    }
                    else if (!normalized.empty())
                    {
                        for (const auto &[candidateName, candidateIndex] : boneIndexByNormalizedName)
                        {
                            if (candidateName.size() >= normalized.size() &&
                                candidateName.compare(candidateName.size() - normalized.size(), normalized.size(), normalized) == 0)
                            {
                                nodeIndex = candidateIndex;
                                break;
                            }
                        }
                    }
                }
            }

            resolvedBoneQueryCache[boneName] = nodeIndex;
        }

        if (nodeIndex < 0 || nodeIndex >= static_cast<int>(globalM.size()))
            return characterWorldMatrix;

        return characterWorldMatrix * globalM[nodeIndex];
    }
}
