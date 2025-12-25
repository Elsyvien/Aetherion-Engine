#pragma once

#include <memory>
#include <unordered_map>

#include "Aetherion/Core/Types.h"
#include "Aetherion/Physics/PhysicsWorld.h"

namespace Aetherion::Scene {
class Scene;
}

namespace Aetherion::Physics {

/// @brief System that manages physics simulation for a scene
///
/// The PhysicsSystem is responsible for:
/// - Creating physics bodies for entities with Rigidbody + Collider components
/// - Stepping the physics simulation each frame
/// - Syncing physics transforms back to scene TransformComponents
class PhysicsSystem {
public:
  PhysicsSystem();
  ~PhysicsSystem();

  PhysicsSystem(const PhysicsSystem &) = delete;
  PhysicsSystem &operator=(const PhysicsSystem &) = delete;

  /// @brief Initialize the physics system
  /// @return true if successful
  bool Initialize();

  /// @brief Shutdown the physics system
  void Shutdown();

  /// @brief Bind a scene to the physics system
  /// @param scene Scene to simulate
  void BindScene(Scene::Scene *scene);

  /// @brief Unbind the current scene
  void UnbindScene();

  /// @brief Synchronize scene entities with physics bodies
  /// Creates/destroys bodies as needed based on component changes
  void SyncBodies();

  /// @brief Step the physics simulation
  /// @param deltaTime Time step in seconds
  void Update(float deltaTime);

  /// @brief Get the physics world
  [[nodiscard]] PhysicsWorld *GetPhysicsWorld() noexcept {
    return m_physicsWorld.get();
  }
  [[nodiscard]] const PhysicsWorld *GetPhysicsWorld() const noexcept {
    return m_physicsWorld.get();
  }

  /// @brief Check if physics is enabled
  [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
  void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }

  /// @brief Set fixed timestep for physics
  void SetFixedTimestep(float timestep) noexcept { m_fixedTimestep = timestep; }
  [[nodiscard]] float GetFixedTimestep() const noexcept {
    return m_fixedTimestep;
  }

private:
  void CreateBodyForEntity(Core::EntityId entityId);
  void DestroyBodyForEntity(Core::EntityId entityId);
  void WriteBackTransforms();

  std::unique_ptr<PhysicsWorld> m_physicsWorld;
  Scene::Scene *m_scene{nullptr};

  // Map entity ID to physics body handle
  std::unordered_map<Core::EntityId, BodyHandle> m_entityBodies;

  bool m_enabled{true};
  float m_fixedTimestep{1.0f / 60.0f};
  float m_accumulator{0.0f};
};

} // namespace Aetherion::Physics
