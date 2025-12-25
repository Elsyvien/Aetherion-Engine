#pragma once

#include <array>
#include <string>

#include "Aetherion/Physics/PhysicsWorld.h"
#include "Aetherion/Scene/Component.h"


namespace Aetherion::Scene {

/// @brief Component for collision shape definition
class ColliderComponent final : public Component {
public:
  using ShapeType = Physics::ShapeType;

  ColliderComponent();
  ~ColliderComponent() override = default;

  [[nodiscard]] std::string GetDisplayName() const override;

  // Shape type
  [[nodiscard]] ShapeType GetShapeType() const noexcept { return m_shapeType; }
  void SetShapeType(ShapeType type) noexcept {
    m_shapeType = type;
    m_dirty = true;
  }

  // Box half extents (used when shapeType == Box)
  [[nodiscard]] std::array<float, 3> GetHalfExtents() const noexcept {
    return m_halfExtents;
  }
  void SetHalfExtents(float x, float y, float z) noexcept;

  // Sphere/Capsule radius
  [[nodiscard]] float GetRadius() const noexcept { return m_radius; }
  void SetRadius(float radius) noexcept;

  // Capsule height (cylinder portion, total height = height + 2*radius)
  [[nodiscard]] float GetHeight() const noexcept { return m_height; }
  void SetHeight(float height) noexcept;

  // Trigger mode (no physics response, just events)
  [[nodiscard]] bool IsTrigger() const noexcept { return m_isTrigger; }
  void SetTrigger(bool isTrigger) noexcept {
    m_isTrigger = isTrigger;
    m_dirty = true;
  }

  // Offset from entity center
  [[nodiscard]] std::array<float, 3> GetOffset() const noexcept {
    return m_offset;
  }
  void SetOffset(float x, float y, float z) noexcept;

  // Dirty flag for physics system
  [[nodiscard]] bool IsDirty() const noexcept { return m_dirty; }
  void ClearDirty() noexcept { m_dirty = false; }

private:
  ShapeType m_shapeType{ShapeType::Box};
  std::array<float, 3> m_halfExtents{0.5f, 0.5f, 0.5f};
  float m_radius{0.5f};
  float m_height{1.0f};
  bool m_isTrigger{false};
  std::array<float, 3> m_offset{0.0f, 0.0f, 0.0f};
  bool m_dirty{true};
};

} // namespace Aetherion::Scene
