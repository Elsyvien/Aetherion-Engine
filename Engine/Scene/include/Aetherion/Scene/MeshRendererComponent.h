#pragma once

#include <array>
#include <string>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
class MeshRendererComponent final : public Component
{
public:
    MeshRendererComponent();
    ~MeshRendererComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }
    void SetVisible(bool visible) noexcept { m_visible = visible; }

    [[nodiscard]] std::array<float, 3> GetColor() const noexcept { return m_color; }
    void SetColor(float r, float g, float b) noexcept;

    [[nodiscard]] float GetRotationSpeedDegPerSec() const noexcept { return m_rotationSpeedDegPerSec; }
    void SetRotationSpeedDegPerSec(float speed) noexcept;

private:
    bool m_visible{true};
    std::array<float, 3> m_color{1.0f, 1.0f, 1.0f};
    float m_rotationSpeedDegPerSec{0.0f};
};
} // namespace Aetherion::Scene
