#pragma once

#include <array>
#include <string>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
class LightComponent final : public Component
{
public:
    LightComponent();
    ~LightComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
    void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }

    [[nodiscard]] std::array<float, 3> GetColor() const noexcept { return m_color; }
    void SetColor(float r, float g, float b) noexcept;

    [[nodiscard]] float GetIntensity() const noexcept { return m_intensity; }
    void SetIntensity(float intensity) noexcept;

    [[nodiscard]] std::array<float, 3> GetAmbientColor() const noexcept { return m_ambientColor; }
    void SetAmbientColor(float r, float g, float b) noexcept;

private:
    bool m_enabled{true};
    std::array<float, 3> m_color{1.0f, 1.0f, 1.0f};
    float m_intensity{1.0f};
    std::array<float, 3> m_ambientColor{0.18f, 0.18f, 0.20f};
};
} // namespace Aetherion::Scene
