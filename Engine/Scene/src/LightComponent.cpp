#include "Aetherion/Scene/LightComponent.h"

#include <algorithm>

namespace Aetherion::Scene
{
LightComponent::LightComponent() = default;

std::string LightComponent::GetDisplayName() const
{
    return "Light";
}

void LightComponent::SetColor(float r, float g, float b) noexcept
{
    m_color[0] = r;
    m_color[1] = g;
    m_color[2] = b;
}

void LightComponent::SetIntensity(float intensity) noexcept
{
    m_intensity = std::max(0.0f, intensity);
}

void LightComponent::SetRange(float range) noexcept
{
    m_range = std::max(0.01f, range);
}

void LightComponent::SetInnerConeAngle(float degrees) noexcept
{
    const float clamped = std::max(0.0f, std::min(179.0f, degrees));
    m_innerConeAngle = clamped;
    if (m_outerConeAngle < m_innerConeAngle)
    {
        m_outerConeAngle = m_innerConeAngle;
    }
}

void LightComponent::SetOuterConeAngle(float degrees) noexcept
{
    const float clamped = std::max(0.0f, std::min(179.0f, degrees));
    m_outerConeAngle = clamped;
    if (m_innerConeAngle > m_outerConeAngle)
    {
        m_innerConeAngle = m_outerConeAngle;
    }
}

void LightComponent::SetAmbientColor(float r, float g, float b) noexcept        
{
    m_ambientColor[0] = r;
    m_ambientColor[1] = g;
    m_ambientColor[2] = b;
}
} // namespace Aetherion::Scene
