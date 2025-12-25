#include "Aetherion/Scene/RigidbodyComponent.h"

#include <algorithm>

namespace Aetherion::Scene {

RigidbodyComponent::RigidbodyComponent() = default;

std::string RigidbodyComponent::GetDisplayName() const { return "Rigidbody"; }

void RigidbodyComponent::SetMass(float mass) noexcept {
  m_mass = std::max(0.001f, mass);
  m_dirty = true;
}

void RigidbodyComponent::SetLinearDamping(float damping) noexcept {
  m_linearDamping = std::max(0.0f, damping);
  m_dirty = true;
}

void RigidbodyComponent::SetAngularDamping(float damping) noexcept {
  m_angularDamping = std::max(0.0f, damping);
  m_dirty = true;
}

void RigidbodyComponent::SetFriction(float friction) noexcept {
  m_friction = std::clamp(friction, 0.0f, 1.0f);
  m_dirty = true;
}

void RigidbodyComponent::SetRestitution(float restitution) noexcept {
  m_restitution = std::clamp(restitution, 0.0f, 1.0f);
  m_dirty = true;
}

} // namespace Aetherion::Scene
