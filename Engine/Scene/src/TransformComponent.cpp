#include "Aetherion/Scene/TransformComponent.h"

#include <algorithm>

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

void TransformComponent::SetParent(Core::EntityId parentId) noexcept
{
    m_parentId = parentId;
}

void TransformComponent::ClearParent() noexcept
{
    m_parentId = 0;
}

void TransformComponent::AddChild(Core::EntityId childId)
{
    if (childId == 0)
    {
        return;
    }

    if (std::find(m_children.begin(), m_children.end(), childId) == m_children.end())
    {
        m_children.push_back(childId);
    }
}

void TransformComponent::RemoveChild(Core::EntityId childId)
{
    auto it = std::remove(m_children.begin(), m_children.end(), childId);
    if (it != m_children.end())
    {
        m_children.erase(it, m_children.end());
    }
}

void TransformComponent::ClearChildren() noexcept
{
    m_children.clear();
}
} // namespace Aetherion::Scene
