#pragma once

#include <array>
#include <string>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
class LightComponent final : public Component
{
public:
    enum class LightType
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    LightComponent();
    ~LightComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    [[nodiscard]] LightType GetType() const noexcept { return m_type; }
    void SetType(LightType type) noexcept { m_type = type; }

    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
    void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }

    [[nodiscard]] std::array<float, 3> GetColor() const noexcept { return m_color; }
    void SetColor(float r, float g, float b) noexcept;

    [[nodiscard]] float GetIntensity() const noexcept { return m_intensity; }
    void SetIntensity(float intensity) noexcept;

    [[nodiscard]] float GetRange() const noexcept { return m_range; }
    void SetRange(float range) noexcept;

    [[nodiscard]] float GetInnerConeAngle() const noexcept { return m_innerConeAngle; }
    void SetInnerConeAngle(float degrees) noexcept;

    [[nodiscard]] float GetOuterConeAngle() const noexcept { return m_outerConeAngle; }
    void SetOuterConeAngle(float degrees) noexcept;

    [[nodiscard]] std::array<float, 3> GetAmbientColor() const noexcept { return m_ambientColor; }
    void SetAmbientColor(float r, float g, float b) noexcept;

    [[nodiscard]] bool IsPrimary() const noexcept { return m_isPrimary; }
    void SetPrimary(bool primary) noexcept { m_isPrimary = primary; }

private:
    LightType m_type{LightType::Directional};
    bool m_enabled{true};
    std::array<float, 3> m_color{1.0f, 1.0f, 1.0f};
    float m_intensity{1.0f};
    float m_range{10.0f};
    float m_innerConeAngle{15.0f};
    float m_outerConeAngle{30.0f};
    std::array<float, 3> m_ambientColor{0.18f, 0.18f, 0.20f};
    bool m_isPrimary{false};
};
} // namespace Aetherion::Scene
