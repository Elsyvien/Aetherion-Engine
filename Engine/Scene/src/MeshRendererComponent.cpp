#include "Aetherion/Scene/MeshRendererComponent.h"

namespace Aetherion::Scene
{
MeshRendererComponent::MeshRendererComponent() = default;

std::string MeshRendererComponent::GetDisplayName() const
{
    return "Mesh Renderer";
}

void MeshRendererComponent::SetColor(float r, float g, float b) noexcept
{
    m_color[0] = r;
    m_color[1] = g;
    m_color[2] = b;
}

void MeshRendererComponent::SetRotationSpeedDegPerSec(float speed) noexcept
{
    m_rotationSpeedDegPerSec = speed;
}
} // namespace Aetherion::Scene
