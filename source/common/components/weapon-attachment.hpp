#pragma once

#include "../ecs/component.hpp"
#include "../ecs/transform.hpp"
#include "../mesh/mesh.hpp"
#include <glm/mat4x4.hpp>
#include <string>

namespace our
{
    class CharacterSkeletonSystem;

    class WeaponAttachmentComponent : public Component
    {
    public:
        Mesh *mesh = nullptr;
        Transform offsetTransform{};
        glm::mat4 offsetMatrix = glm::mat4(1.0f);

        bool attached = false;
        std::string attachedBoneName;
        CharacterSkeletonSystem *attachedCharacter = nullptr;

        glm::mat4 detachedWorldMatrix = glm::mat4(1.0f);
        glm::mat4 computedWorldMatrix = glm::mat4(1.0f);

        static std::string getID() { return "Weapon Attachment"; }

        void deserialize(const nlohmann::json &data) override;
        void rebuildOffsetMatrix();
    };
}
