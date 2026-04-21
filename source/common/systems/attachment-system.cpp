#include "attachment-system.hpp"

#include "../ecs/entity.hpp"
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace our
{
    void AttachmentSystem::applyWorldMatrixToEntity(Entity *entity, const glm::mat4 &worldMatrix)
    {
        if (!entity)
            return;

        glm::vec3 skew(0.0f);
        glm::vec4 perspective(0.0f);
        glm::vec3 translation(0.0f);
        glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale(1.0f);
        if (!glm::decompose(worldMatrix, scale, rotation, translation, skew, perspective))
            return;

        rotation = glm::conjugate(rotation);
        entity->parent = nullptr;
        entity->localTransform.position = translation;
        entity->localTransform.rotation = glm::eulerAngles(glm::normalize(rotation));
        entity->localTransform.scale = scale;
    }

    void AttachmentSystem::attachWeaponToBone(Weapon *weapon, Character *character, std::string boneName)
    {
        if (!(weapon && weapon->getOwner()))
            return;

        weapon->attached = true;
        weapon->attachedBoneName = std::move(boneName);
        weapon->attachedCharacter = character;
        weapon->detachedWorldMatrix = weapon->getOwner()->getLocalToWorldMatrix();
    }

    void AttachmentSystem::detachWeapon(Weapon *weapon)
    {
        if (!weapon)
            return;

        weapon->attached = false;
        weapon->attachedCharacter = nullptr;
    }

    void AttachmentSystem::switchWeapon(Weapon *currentWeapon, Weapon *newWeapon, Character *character, const std::string &boneName)
    {
        if (currentWeapon)
            detachWeapon(currentWeapon);
        if (newWeapon)
            attachWeaponToBone(newWeapon, character, boneName);
    }

    void AttachmentSystem::updateWeaponTransform(Weapon *weapon) const
    {
        if (!(weapon && weapon->getOwner()))
            return;

        glm::mat4 worldMatrix = weapon->detachedWorldMatrix;
        if (weapon->attached && weapon->attachedCharacter)
        {
            glm::mat4 handMatrix = weapon->attachedCharacter->getBoneWorldMatrix(weapon->attachedBoneName);
            weapon->computedWorldMatrix = handMatrix * weapon->offsetMatrix;
            worldMatrix = weapon->computedWorldMatrix;
            weapon->detachedWorldMatrix = worldMatrix;
        }

        applyWorldMatrixToEntity(weapon->getOwner(), worldMatrix);
    }
}
