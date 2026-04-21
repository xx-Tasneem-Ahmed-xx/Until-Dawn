#include "weapon-attachment.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace our
{
    void WeaponAttachmentComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        if (data.contains("offset") && data["offset"].is_object())
        {
            offsetTransform.deserialize(data["offset"]);
        }

        attached = data.value("attached", attached);
        attachedBoneName = data.value("bone", attachedBoneName);
        rebuildOffsetMatrix();
    }

    void WeaponAttachmentComponent::rebuildOffsetMatrix()
    {
        offsetMatrix = offsetTransform.toMat4();
    }
}
