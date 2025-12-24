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
    [[nodiscard]] float GetPositionZ() const noexcept { return m_position[2]; }
    [[nodiscard]] float GetRotationXDegrees() const noexcept { return m_rotationDegrees[0]; }
    [[nodiscard]] float GetRotationYDegrees() const noexcept { return m_rotationDegrees[1]; }
    [[nodiscard]] float GetRotationZDegrees() const noexcept { return m_rotationDegrees[2]; }
    [[nodiscard]] float GetScaleX() const noexcept { return m_scale[0]; }
    [[nodiscard]] float GetScaleY() const noexcept { return m_scale[1]; }
    [[nodiscard]] float GetScaleZ() const noexcept { return m_scale[2]; }
    [[nodiscard]] Core::EntityId GetParentId() const noexcept { return m_parentId; }
    [[nodiscard]] bool HasParent() const noexcept { return m_parentId != 0; }
    [[nodiscard]] const std::vector<Core::EntityId>& GetChildren() const noexcept { return m_children; }

    void SetPosition(float x, float y, float z) noexcept;
    void SetRotationDegrees(float xDegrees, float yDegrees, float zDegrees) noexcept;
    void SetScale(float x, float y, float z) noexcept;
    void SetParent(Core::EntityId parentId) noexcept;
    void ClearParent() noexcept;
    void AddChild(Core::EntityId childId);
    void RemoveChild(Core::EntityId childId);
    void ClearChildren() noexcept;

private:
    std::array<float, 3> m_position{0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_rotationDegrees{0.0f, 0.0f, 0.0f};
    std::array<float, 3> m_scale{1.0f, 1.0f, 1.0f};
    Core::EntityId m_parentId{0};
    std::vector<Core::EntityId> m_children;
};
} // namespace Aetherion::Scene
