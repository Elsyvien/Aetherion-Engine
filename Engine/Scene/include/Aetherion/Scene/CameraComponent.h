#pragma once

#include <string>

#include "Aetherion/Scene/Component.h"

namespace Aetherion::Scene
{
class CameraComponent final : public Component
{
public:
    enum class ProjectionType
    {
        Perspective = 0,
        Orthographic = 1
    };

    CameraComponent();
    ~CameraComponent() override = default;

    [[nodiscard]] std::string GetDisplayName() const override;

    [[nodiscard]] ProjectionType GetProjectionType() const noexcept { return m_projectionType; }
    void SetProjectionType(ProjectionType type) noexcept { m_projectionType = type; }

    [[nodiscard]] float GetVerticalFov() const noexcept { return m_verticalFov; }
    void SetVerticalFov(float fov) noexcept { m_verticalFov = fov; }

    [[nodiscard]] float GetNearClip() const noexcept { return m_nearClip; }
    void SetNearClip(float nearClip) noexcept { m_nearClip = nearClip; }

    [[nodiscard]] float GetFarClip() const noexcept { return m_farClip; }
    void SetFarClip(float farClip) noexcept { m_farClip = farClip; }

    [[nodiscard]] float GetOrthographicSize() const noexcept { return m_orthographicSize; }
    void SetOrthographicSize(float size) noexcept { m_orthographicSize = size; }

    [[nodiscard]] bool IsPrimary() const noexcept { return m_isPrimary; }
    void SetPrimary(bool primary) noexcept { m_isPrimary = primary; }

private:
    ProjectionType m_projectionType{ProjectionType::Perspective};
    float m_verticalFov{45.0f};
    float m_nearClip{0.1f};
    float m_farClip{1000.0f};
    float m_orthographicSize{10.0f};
    bool m_isPrimary{true};
};
} // namespace Aetherion::Scene
