#pragma once

#include "../ecs/component.hpp"
#include "../ecs/transform.hpp"
#include "health.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace our
{

    enum class ZombieState
    {
        Walking,
        Attacking,
        Crawling,
        Dead
    };

    // This component marks an entity as a zombie and provides zombie-related data
    // Member 3 will expand this with movement and behavior logic
    class ZombieComponent : public Component
    {
    public:
        float radius = 1.0f;         // Bounding sphere radius for collision detection
        float speed = 2.2f;          // Walk speed toward player
        float crawlSpeed = 0.9f;     // Speed after first shot
        float damage = 10.0f;        // Damage dealt to player per attack
        float attackRange = 1.8f;    // Distance at which zombie starts attacking
        float attackCooldown = 1.0f; // Seconds between attacks
        float corpseLifetime = 1.2f; // Seconds to keep dead zombie before despawn
        float maxHealth = 20.0f;
        float health = 20.0f;

        ZombieState state = ZombieState::Walking;
        float attackCooldownTimer = 0.0f;
        float deathTime = 0.0f;
        float motionTime = 0.0f;
        float baseY = 0.0f;
        std::string activeMotionClip;
        float motionClipTime = 0.0f;
        std::vector<glm::mat4> skinMatrices;

        // The ID of this component type is "Zombie"
        static std::string getID() { return "Zombie"; }

        // Reads zombie data from the given json object
        void deserialize(const nlohmann::json &data) override;

        // Called each frame for runtime cooldown/timer updates.
        void update(float deltaTime);

        // Registers a shot and updates state.
        // Returns true if this shot kills the zombie.
        bool registerShot();

        // Returns true if zombie may apply attack damage this frame.
        bool canAttack() const { return state != ZombieState::Dead && attackCooldownTimer <= 0.0f; }

        // Starts attack cooldown after a successful attack.
        void resetAttackCooldown() { attackCooldownTimer = attackCooldown; }

        // Current movement speed based on state.
        float getCurrentSpeed() const;

        void applyHealthState(bool isAlive);

        // Applies dead pose to transform and returns true if zombie should despawn now.
        bool updateDeathTransform(Transform &transform, float deathFallDegrees, float deathSink) const;

        // Updates movement/combat against player and writes transform changes.
        void updateMovementAndCombat(
            Transform &transform,
            const glm::vec3 &zombieWorldPosition,
            const glm::vec3 &playerTarget,
            float yawOffset,
            float deltaTime,
            HealthComponent *playerHealth);

        bool isDead() const { return state == ZombieState::Dead; }
        bool shouldDespawn() const { return isDead() && deathTime >= corpseLifetime; }

    private:
        void syncStateWithHealth();
    };

}
