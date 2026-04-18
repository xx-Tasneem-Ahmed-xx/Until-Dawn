#include "zombie.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"

namespace our
{

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

        shotsTaken = data.value("shotsTaken", shotsTaken);
        if (shotsTaken >= 2)
        {
            state = ZombieState::Dead;
        }
        else if (shotsTaken == 1)
        {
            state = ZombieState::Crawling;
        }
        else
        {
            state = ZombieState::Walking;
        }
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
            attackCooldownTimer -= deltaTime;
            if (attackCooldownTimer < 0.0f)
                attackCooldownTimer = 0.0f;
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

        shotsTaken++;
        if (shotsTaken == 1)
        {
            state = ZombieState::Crawling;
            return false;
        }

        state = ZombieState::Dead;
        deathTime = 0.0f;
        return true;
    }

    float ZombieComponent::getCurrentSpeed() const
    {
        if (state == ZombieState::Dead)
            return 0.0f;
        if (state == ZombieState::Crawling)
            return crawlSpeed;
        return speed;
    }

}
