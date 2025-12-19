#pragma once

#include <array>
#include <string>
#include <vector>

#include "Aetherion/Scene/Component.h"
#include "Aetherion/Core/Types.h"

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
    [[nodiscard]] Core::EntityId GetParentId() const noexcept { return m_parentId; }
    [[nodiscard]] bool HasParent() const noexcept { return m_parentId != 0; }
    [[nodiscard]] const std::vector<Core::EntityId>& GetChildren() const noexcept { return m_children; }

    void SetPosition(float x, float y) noexcept;
    void SetRotationZDegrees(float degrees) noexcept;
    void SetScale(float x, float y) noexcept;
    void SetParent(Core::EntityId parentId) noexcept;
    void ClearParent() noexcept;
    void AddChild(Core::EntityId childId);
    void RemoveChild(Core::EntityId childId);
    void ClearChildren() noexcept;

private:
    std::array<float, 2> m_position{0.0f, 0.0f};
    float m_rotationZDegrees{0.0f};
    std::array<float, 2> m_scale{1.0f, 1.0f};
    Core::EntityId m_parentId{0};
    std::vector<Core::EntityId> m_children;
};
} // namespace Aetherion::Scene
