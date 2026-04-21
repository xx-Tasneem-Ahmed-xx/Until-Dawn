#pragma once

#include "character-skeleton-system.hpp"
#include "../components/weapon-attachment.hpp"
#include <string>

namespace our
{
    class Entity;

    class AttachmentSystem
    {
        static void applyWorldMatrixToEntity(Entity *entity, const glm::mat4 &worldMatrix);

    public:
        using Weapon = WeaponAttachmentComponent;
        using Character = CharacterSkeletonSystem;

        void attachWeaponToBone(Weapon *weapon, Character *character, std::string boneName);
        void detachWeapon(Weapon *weapon);
        void switchWeapon(Weapon *currentWeapon, Weapon *newWeapon, Character *character, const std::string &boneName);
        void updateWeaponTransform(Weapon *weapon) const;
    };
}
