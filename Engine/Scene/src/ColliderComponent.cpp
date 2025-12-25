#include "Aetherion/Scene/ColliderComponent.h"

#include <algorithm>

namespace Aetherion::Scene {

ColliderComponent::ColliderComponent() = default;

std::string ColliderComponent::GetDisplayName() const { return "Collider"; }

void ColliderComponent::SetHalfExtents(float x, float y, float z) noexcept {
  m_halfExtents[0] = std::max(0.001f, x);
  m_halfExtents[1] = std::max(0.001f, y);
  m_halfExtents[2] = std::max(0.001f, z);
  m_dirty = true;
}

void ColliderComponent::SetRadius(float radius) noexcept {
  m_radius = std::max(0.001f, radius);
  m_dirty = true;
}

void ColliderComponent::SetHeight(float height) noexcept {
  m_height = std::max(0.001f, height);
  m_dirty = true;
}

void ColliderComponent::SetOffset(float x, float y, float z) noexcept {
  m_offset[0] = x;
  m_offset[1] = y;
  m_offset[2] = z;
  m_dirty = true;
}

} // namespace Aetherion::Scene
