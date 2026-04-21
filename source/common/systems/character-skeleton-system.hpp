#pragma once

#include "../animation/motion.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace our
{
    class CharacterSkeletonSystem
    {
        const Motion *motion = nullptr;
        const MotionClip *clip = nullptr;
        glm::mat4 characterWorldMatrix = glm::mat4(1.0f);

        std::vector<glm::vec3> localT;
        std::vector<glm::quat> localR;
        std::vector<glm::vec3> localS;
        std::vector<glm::mat4> localM;
        std::vector<glm::mat4> globalM;
        std::vector<uint8_t> computed;
        std::unordered_map<std::string, int> boneIndexByName;
        std::unordered_map<std::string, int> boneIndexByLowerName;
        std::unordered_map<std::string, int> boneIndexByNormalizedName;
        mutable std::unordered_map<std::string, int> resolvedBoneQueryCache;

        void rebuildBoneLookup();
        void ensurePoseStorage();
        void sampleLocalPose(float localTime);
        glm::mat4 computeGlobalNodeMatrix(int nodeIndex);

    public:
        void bindRuntimePose(const Motion *motionAsset, const MotionClip *motionClip, float localTime, const glm::mat4 &characterWorld);
        glm::mat4 getBoneWorldMatrix(std::string boneName) const;
    };
}
