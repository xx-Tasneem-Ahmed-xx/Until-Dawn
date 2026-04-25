#include "zombie.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"
#include <algorithm>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>

namespace our
{

    void ZombieComponent::syncStateWithHealth()
    {
        if (health <= 0)
            state = ZombieState::Dead;
        else if (health <= 0.5 * maxHealth)
            state = ZombieState::Crawling;
        else
            state = ZombieState::Walking;
    }

    void ZombieComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;
        radius = data.value("radius", radius);
        speed = data.value("speed", speed);
        crawlSpeed = data.value("crawlSpeed", crawlSpeed);
        damage = data.value("damage", damage);
        attackRange = data.value("attackRange", attackRange);
        attackCooldown = data.value("attackCooldown", attackCooldown);
        corpseLifetime = data.value("corpseLifetime", corpseLifetime);

        syncStateWithHealth();
        attackCooldownTimer = 0.0f;
        deathTime = 0.0f;
        motionTime = 0.0f;
        activeMotionClip.clear();
        motionClipTime = 0.0f;
    }

    void ZombieComponent::update(float deltaTime)
    {
        motionTime += deltaTime;

        if (attackCooldownTimer > 0.0f)
        {
            attackCooldownTimer = std::max(0.0f, attackCooldownTimer - deltaTime);
        }

        if (state == ZombieState::Dead)
        {
            deathTime += deltaTime;
        }
    }

    bool ZombieComponent::registerShot()
    {
        if (state == ZombieState::Dead)
            return true;
        health -= 10;
        syncStateWithHealth();
        if (state == ZombieState::Dead)
            deathTime = 0.0f;
        return state == ZombieState::Dead;
    }

    float ZombieComponent::getCurrentSpeed() const
    {
        if (state == ZombieState::Dead)
            return 0.0f;
        if (state == ZombieState::Crawling)
            return crawlSpeed;
        return speed;
    }

    void ZombieComponent::applyHealthState(bool isAlive)
    {
        if (!isAlive)
            state = ZombieState::Dead;
    }

    bool ZombieComponent::updateDeathTransform(Transform &transform, float deathFallDegrees, float deathSink) const
    {
        if (!isDead())
            return false;

        float corpseDuration = std::max(0.01f, corpseLifetime);
        float t = glm::clamp(deathTime / corpseDuration, 0.0f, 1.0f);
        transform.rotation.x = -glm::radians(deathFallDegrees) * t;
        transform.rotation.z = 0.0f;
        transform.position.y = baseY - deathSink * t;

        return shouldDespawn();
    }

    void ZombieComponent::updateMovementAndCombat(
        Transform &transform,
        const glm::vec3 &zombieWorldPosition,
        const glm::vec3 &playerTarget,
        float yawOffset,
        float deltaTime,
        HealthComponent *playerHealth)
    {
        if (isDead())
            return;

        glm::vec3 toPlayer = playerTarget - zombieWorldPosition;
        toPlayer.y = 0.0f;
        float distanceToPlayer = glm::length(toPlayer);
        if (distanceToPlayer <= 0.0001f)
            return;

        glm::vec3 direction = toPlayer / distanceToPlayer;
        float yaw = std::atan2(-direction.x, -direction.z);
        transform.rotation.y = yaw + yawOffset;

        const float effectiveAttackRange = std::max(0.01f, attackRange);
        if (distanceToPlayer > effectiveAttackRange)
        {
            state = (health <= 0.5 * maxHealth && health > 0) ? ZombieState::Crawling : ZombieState::Walking;

            // Clamp movement so the zombie never crosses inside the attack threshold in one frame.
            float maxAdvance = std::max(0.0f, distanceToPlayer - effectiveAttackRange);
            float step = std::min(getCurrentSpeed() * deltaTime, maxAdvance);
            if (step > 0.0f)
            {
                transform.position += direction * step;
            }
            return;
        }

        state = ZombieState::Attacking;
        if (playerHealth && playerHealth->isAlive && canAttack())
        {
            playerHealth->takeDamage(damage);
            resetAttackCooldown();
        }
    }

}
