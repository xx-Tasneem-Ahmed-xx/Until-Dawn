#include "player.hpp"

namespace our
{

    void PlayerComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        isMainPlayer = data.value("isMainPlayer", isMainPlayer);
    }

}
