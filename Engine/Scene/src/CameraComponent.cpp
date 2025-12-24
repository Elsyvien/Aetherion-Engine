#include "Aetherion/Scene/CameraComponent.h"

namespace Aetherion::Scene
{
CameraComponent::CameraComponent() = default;

std::string CameraComponent::GetDisplayName() const
{
    return "Camera";
}
} // namespace Aetherion::Scene
