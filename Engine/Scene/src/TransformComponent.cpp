#include "Aetherion/Scene/TransformComponent.h"

#include <algorithm>

namespace Aetherion::Scene
{
TransformComponent::TransformComponent() = default;

std::string TransformComponent::GetDisplayName() const
{
    return "Transform";
}

void TransformComponent::SetPosition(float x, float y, float z) noexcept
{
    m_position[0] = x;
    m_position[1] = y;
    m_position[2] = z;
}

void TransformComponent::SetRotationDegrees(float xDegrees, float yDegrees, float zDegrees) noexcept
{
    m_rotationDegrees[0] = xDegrees;
    m_rotationDegrees[1] = yDegrees;
    m_rotationDegrees[2] = zDegrees;
}

void TransformComponent::SetScale(float x, float y, float z) noexcept
{
    m_scale[0] = x;
    m_scale[1] = y;
    m_scale[2] = z;
}

void TransformComponent::SetPosition(const std::array<float, 3>& position) noexcept
{
    m_position = position;
}

void TransformComponent::SetRotationDegrees(const std::array<float, 3>& rotationDegrees) noexcept
{
    m_rotationDegrees = rotationDegrees;
}

void TransformComponent::SetScale(const std::array<float, 3>& scale) noexcept
{
    m_scale = scale;
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
