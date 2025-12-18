#pragma once

#include <array>
#include <string>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
class TransformComponent final : public Component
{
public:
    TransformComponent();
    ~TransformComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    [[nodiscard]] float GetPositionX() const noexcept { return m_position[0]; }
    [[nodiscard]] float GetPositionY() const noexcept { return m_position[1]; }
    [[nodiscard]] float GetRotationZDegrees() const noexcept { return m_rotationZDegrees; }
    [[nodiscard]] float GetScaleX() const noexcept { return m_scale[0]; }
    [[nodiscard]] float GetScaleY() const noexcept { return m_scale[1]; }

    void SetPosition(float x, float y) noexcept;
    void SetRotationZDegrees(float degrees) noexcept;
    void SetScale(float x, float y) noexcept;

private:
    std::array<float, 2> m_position{0.0f, 0.0f};
    float m_rotationZDegrees{0.0f};
    std::array<float, 2> m_scale{1.0f, 1.0f};
};
} // namespace Aetherion::Scene
