#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Scene
{
TransformComponent::TransformComponent() = default;

std::string TransformComponent::GetDisplayName() const
{
    return "Transform";
}

void TransformComponent::SetPosition(float x, float y) noexcept
{
    m_position[0] = x;
    m_position[1] = y;
}

void TransformComponent::SetRotationZDegrees(float degrees) noexcept
{
    m_rotationZDegrees = degrees;
}

void TransformComponent::SetScale(float x, float y) noexcept
{
    m_scale[0] = x;
    m_scale[1] = y;
}
} // namespace Aetherion::Scene
