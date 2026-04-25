#pragma once

#include "../ecs/component.hpp"
#include "../audio-manager.hpp"
#include <iostream>

namespace our
{

    // Base weapon class representing a weapon that can shoot projectiles or raycast
    class WeaponComponent : public Component
    {
    public:
        int ammo = 30;
        int maxAmmo = 30;
        float maxRange = 50.0f;
        float fireRate = 10.0f; // Shots per second (inverse = cooldown between shots)
        float damage = 10.0f;
        float reloadTime = 1.5f;
        bool isReloading = false;
        float reloadTimer = 0.0f;  // Time elapsed during current reload
        float fireCooldown = 0.0f; // Time until next shot can be fired
        std::string shootSound = "";
        std::string emptySound = "assets/audio/empty-gun.wav";
        std::string reloadSound = "assets/audio/reload.wav";

        WeaponComponent() = default;
        virtual ~WeaponComponent() = default;

        static std::string getID() { return "Weapon"; }

        // Reads weapon data from the given json object
        void deserialize(const nlohmann::json &data) override;

        virtual bool shoot()
        {
            if (ammo <= 0)
            {
                if (AudioManager::getInstance().isInitialized() && !emptySound.empty())
                {
                    AudioManager::getInstance().playSound(emptySound);
                }
                return false;
            }
            if (fireCooldown > 0.0f)
            {
                return false;
            }

            ammo--;
            fireCooldown = 1.0f / fireRate; // Set cooldown based on fire rate

            std::cout << "shoot | Remaining ammo: " << ammo << std::endl;

            // Play shoot sound once per successful shot
            if (AudioManager::getInstance().isInitialized() && !shootSound.empty())
            {
                AudioManager::getInstance().playSound(shootSound);
            }

            return true;
        }

        // handles cooldown
        virtual void update(float deltaTime)
        {
            if (fireCooldown > 0.0f)
            {
                fireCooldown -= deltaTime;
                if (fireCooldown < 0.0f)
                {
                    fireCooldown = 0.0f;
                }
            }

            if (isReloading)
            {
                reloadTimer += deltaTime;
                if (reloadTimer >= reloadTime)
                {
                    ammo = maxAmmo;
                    isReloading = false;
                    reloadTimer = 0.0f;
                    std::cout << "Reload complete | Ammo: " << ammo << "/" << maxAmmo << std::endl;
                }
            }
        }

        virtual bool reload()
        {
            if (isReloading)
            {
                std::cout << "Already reloading..." << std::endl;
                return false;
            }

            if (ammo == maxAmmo)
            {
                std::cout << "Magazine is full" << std::endl;
                return false;
            }

            isReloading = true;
            reloadTimer = 0.0f;
            std::cout << "Reloading... (will take " << reloadTime << " seconds)" << std::endl;

            if (AudioManager::getInstance().isInitialized() && !reloadSound.empty())
            {
                AudioManager::getInstance().playSound(reloadSound);
            }
            return true;
        }
    };

    class Pistol : public WeaponComponent
    {
    public:
        Pistol()
        {
            ammo = 15;
            maxAmmo = 15;
            maxRange = 30.0f;
            fireRate = 5.0f;
            damage = 8.0f;
            reloadTime = 2.0f;
            shootSound = "assets/audio/gunshot20.wav";
        }

        static std::string getID() { return "Pistol"; }
    };

    class Rifle : public WeaponComponent
    {
    public:
        Rifle()
        {
            ammo = 30;
            maxAmmo = 30;
            maxRange = 80.0f;
            fireRate = 10.0f;
            damage = 15.0f;
            reloadTime = 1.5f;
            shootSound = "assets/audio/ak47.wav";
        }

        static std::string getID() { return "Rifle"; }
    };
}