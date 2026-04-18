#include "motion.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <glm/gtx/quaternion.hpp>

namespace
{
    std::string toLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return s;
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

    const MotionClip *Motion::findClipByKeywords(std::initializer_list<std::string> keywords) const
    {
        std::vector<std::string> loweredKeywords;
        loweredKeywords.reserve(keywords.size());
        for (const auto &k : keywords)
        {
            loweredKeywords.push_back(toLower(k));
        }

        for (const auto &clip : clips)
        {
            std::string loweredName = toLower(clip.name);
            for (const auto &keyword : loweredKeywords)
            {
                if (!keyword.empty() && loweredName.find(keyword) != std::string::npos)
                {
                    return &clip;
                }
            }
        }
        return nullptr;
    }

    bool Motion::sampleClipRootPose(const MotionClip *clip, float localTime, glm::vec3 &translation, glm::quat &rotation, glm::vec3 &scale) const
    {
        if (!clip)
            return false;

        int rootNode = -1;
        for (const auto &channel : clip->channels)
        {
            if (channel.targetNode >= 0)
            {
                rootNode = channel.targetNode;
                break;
            }
        }
        if (rootNode < 0 || rootNode >= static_cast<int>(nodes.size()))
            return false;

        translation = nodes[rootNode].baseTranslation;
        rotation = nodes[rootNode].baseRotation;
        scale = nodes[rootNode].baseScale;

        float sampleTime = localTime;
        if (clip->duration > 0.0001f)
        {
            sampleTime = std::fmod(localTime, clip->duration);
            if (sampleTime < 0.0f)
                sampleTime += clip->duration;
            sampleTime += clip->startTime;
        }

        bool touched = false;
        for (const auto &channel : clip->channels)
        {
            if (channel.targetNode != rootNode)
                continue;
            if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(clip->samplers.size()))
                continue;

            const auto &sampler = clip->samplers[channel.samplerIndex];
            glm::vec4 v = sampleSampler(sampler, sampleTime);
            if (channel.path == MotionPath::Translation)
            {
                translation = glm::vec3(v);
                touched = true;
            }
            else if (channel.path == MotionPath::Scale)
            {
                scale = glm::vec3(v);
                touched = true;
            }
            else if (channel.path == MotionPath::Rotation)
            {
                rotation = glm::normalize(glm::quat(v.w, v.x, v.y, v.z));
                touched = true;
            }
        }

        return touched;
    }

    bool Motion::computeSkinMatrices(const MotionClip *clip, float localTime,
                                     const std::vector<int> &jointNodes,
                                     const std::vector<glm::mat4> &inverseBind,
                                     std::vector<glm::mat4> &outSkinMatrices) const
    {
        if (!clip)
            return false;
        if (jointNodes.empty())
            return false;
        if (inverseBind.size() < jointNodes.size())
            return false;
        if (nodes.empty())
            return false;

        std::vector<glm::vec3> localT(nodes.size(), glm::vec3(0.0f));
        std::vector<glm::quat> localR(nodes.size(), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        std::vector<glm::vec3> localS(nodes.size(), glm::vec3(1.0f));
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            localT[i] = nodes[i].baseTranslation;
            localR[i] = nodes[i].baseRotation;
            localS[i] = nodes[i].baseScale;
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
            if (channel.targetNode < 0 || channel.targetNode >= static_cast<int>(nodes.size()))
                continue;
            if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(clip->samplers.size()))
                continue;

            const auto &sampler = clip->samplers[channel.samplerIndex];
            if (sampler.times.empty() || sampler.values.empty())
                continue;

            glm::vec4 v = sampleSampler(sampler, sampleTime);
            if (channel.path == MotionPath::Translation)
            {
                localT[channel.targetNode] = glm::vec3(v);
            }
            else if (channel.path == MotionPath::Scale)
            {
                localS[channel.targetNode] = glm::vec3(v);
            }
            else if (channel.path == MotionPath::Rotation)
            {
                localR[channel.targetNode] = glm::normalize(glm::quat(v.w, v.x, v.y, v.z));
            }
        }

        std::vector<glm::mat4> localM(nodes.size(), glm::mat4(1.0f));
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const glm::mat4 T = glm::translate(glm::mat4(1.0f), localT[i]);
            const glm::mat4 R = glm::mat4_cast(localR[i]);
            const glm::mat4 S = glm::scale(glm::mat4(1.0f), localS[i]);
            localM[i] = T * R * S;
        }

        std::vector<glm::mat4> globalM(nodes.size(), glm::mat4(1.0f));
        std::vector<uint8_t> computed(nodes.size(), 0);

        std::function<glm::mat4(int)> computeGlobal = [&](int nodeIndex) -> glm::mat4
        {
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
                return glm::mat4(1.0f);
            if (computed[nodeIndex])
                return globalM[nodeIndex];

            if (nodes[nodeIndex].parent >= 0 && nodes[nodeIndex].parent < static_cast<int>(nodes.size()))
            {
                globalM[nodeIndex] = computeGlobal(nodes[nodeIndex].parent) * localM[nodeIndex];
            }
            else
            {
                globalM[nodeIndex] = localM[nodeIndex];
            }
            computed[nodeIndex] = 1;
            return globalM[nodeIndex];
        };

        outSkinMatrices.assign(jointNodes.size(), glm::mat4(1.0f));
        for (size_t j = 0; j < jointNodes.size(); ++j)
        {
            const int nodeIndex = jointNodes[j];
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
            {
                outSkinMatrices[j] = glm::mat4(1.0f);
                continue;
            }
            outSkinMatrices[j] = computeGlobal(nodeIndex) * inverseBind[j];
        }

        return true;
    }

}
