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

void LightComponent::SetAmbientColor(float r, float g, float b) noexcept
{
    m_ambientColor[0] = r;
    m_ambientColor[1] = g;
    m_ambientColor[2] = b;
}
} // namespace Aetherion::Scene
