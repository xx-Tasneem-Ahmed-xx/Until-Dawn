#pragma once

#include <initializer_list>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace our
{

    enum class MotionPath
    {
        Translation,
        Rotation,
        Scale
    };

    struct MotionSampler
    {
        std::vector<float> times;
        std::vector<glm::vec4> values;
        MotionPath path = MotionPath::Translation;
    };

    struct MotionChannel
    {
        int targetNode = -1;
        int samplerIndex = -1;
        MotionPath path = MotionPath::Translation;
    };

    struct MotionNode
    {
        std::string name;
        int parent = -1;
        std::vector<int> children;
        glm::vec3 baseTranslation = glm::vec3(0.0f);
        glm::quat baseRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 baseScale = glm::vec3(1.0f);
    };

    struct MotionClip
    {
        std::string name;
        float startTime = 0.0f;
        float endTime = 0.0f;
        float duration = 0.0f;
        int channelCount = 0;
        std::vector<MotionSampler> samplers;
        std::vector<MotionChannel> channels;
    };

    class Motion
    {
    public:
        std::string sourcePath;
        std::vector<MotionClip> clips;
        std::vector<MotionNode> nodes;

        const MotionClip *findClip(const std::string &clipName) const
        {
            for (const auto &clip : clips)
            {
                if (clip.name == clipName)
                    return &clip;
            }
            return nullptr;
        }

        // Returns first clip whose name contains any keyword (case-insensitive).
        const MotionClip *findClipByKeywords(std::initializer_list<std::string> keywords) const;

        // Samples the first animated node of a clip at local time.
        // Returns false if no valid animation data exists.
        bool sampleClipRootPose(const MotionClip *clip, float localTime, glm::vec3 &translation, glm::quat &rotation, glm::vec3 &scale) const;

        // Computes per-joint skinning matrices for the given clip and local time.
        bool computeSkinMatrices(const MotionClip *clip, float localTime,
                                 const std::vector<int> &jointNodes,
                                 const std::vector<glm::mat4> &inverseBind,
                                 std::vector<glm::mat4> &outSkinMatrices) const;
    };

}
