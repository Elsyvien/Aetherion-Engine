#pragma once

#include <array>
#include <string>

#include "Aetherion/Physics/PhysicsWorld.h"
#include "Aetherion/Scene/Component.h"


namespace Aetherion::Scene {

/// @brief Component for rigid body physics simulation
class RigidbodyComponent final : public Component {
public:
  using MotionType = Physics::MotionType;

  RigidbodyComponent();
  ~RigidbodyComponent() override = default;

  [[nodiscard]] std::string GetDisplayName() const override;

  // Motion type
  [[nodiscard]] MotionType GetMotionType() const noexcept {
    return m_motionType;
  }
  void SetMotionType(MotionType type) noexcept {
    m_motionType = type;
    m_dirty = true;
  }

  // Mass (only affects dynamic bodies)
  [[nodiscard]] float GetMass() const noexcept { return m_mass; }
  void SetMass(float mass) noexcept;

  // Damping
  [[nodiscard]] float GetLinearDamping() const noexcept {
    return m_linearDamping;
  }
  void SetLinearDamping(float damping) noexcept;

  [[nodiscard]] float GetAngularDamping() const noexcept {
    return m_angularDamping;
  }
  void SetAngularDamping(float damping) noexcept;

  // Gravity
  [[nodiscard]] bool UseGravity() const noexcept { return m_useGravity; }
  void SetUseGravity(bool useGravity) noexcept {
    m_useGravity = useGravity;
    m_dirty = true;
  }

  // Material properties
  [[nodiscard]] float GetFriction() const noexcept { return m_friction; }
  void SetFriction(float friction) noexcept;

  [[nodiscard]] float GetRestitution() const noexcept { return m_restitution; }
  void SetRestitution(float restitution) noexcept;

  // Physics handle (managed by PhysicsSystem)
  [[nodiscard]] Physics::BodyHandle GetBodyHandle() const noexcept {
    return m_bodyHandle;
  }
  void SetBodyHandle(Physics::BodyHandle handle) noexcept {
    m_bodyHandle = handle;
  }

  // Dirty flag for physics system to know when to recreate
  [[nodiscard]] bool IsDirty() const noexcept { return m_dirty; }
  void ClearDirty() noexcept { m_dirty = false; }

private:
  MotionType m_motionType{MotionType::Dynamic};
  float m_mass{1.0f};
  float m_linearDamping{0.05f};
  float m_angularDamping{0.05f};
  bool m_useGravity{true};
  float m_friction{0.5f};
  float m_restitution{0.0f};

  Physics::BodyHandle m_bodyHandle{};
  bool m_dirty{true};
};

} // namespace Aetherion::Scene
