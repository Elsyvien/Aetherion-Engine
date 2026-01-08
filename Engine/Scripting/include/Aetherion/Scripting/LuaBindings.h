#pragma once

// Forward declarations to avoid Lua headers in public API
namespace sol {
class state;
}

namespace Aetherion::Scene {
class Scene;
class Entity;
class TransformComponent;
class MeshRendererComponent;
class LightComponent;
class CameraComponent;
class RigidbodyComponent;
class ColliderComponent;
class AudioSourceComponent;
class ParticleEmitterComponent;
} // namespace Aetherion::Scene

namespace Aetherion::Scripting {

/// @brief Register all Aetherion engine bindings with a Lua state
/// This creates the 'aetherion' global table and registers:
/// - Entity access and manipulation
/// - Component getters/setters
/// - Math types (Vec3, Quat, etc.)
/// - Scene queries
/// - Logging functions
class LuaBindings {
public:
  /// @brief Register all bindings with the given Lua state
  static void RegisterAll(sol::state &lua, Scene::Scene *scene);

  /// @brief Register just math types (Vec3, etc.)
  static void RegisterMath(sol::state &lua);

  /// @brief Register entity-related bindings
  static void RegisterEntity(sol::state &lua, Scene::Scene *scene);

  /// @brief Register component bindings
  static void RegisterComponents(sol::state &lua);

  /// @brief Register logging/debug functions
  static void RegisterLogging(sol::state &lua);

  /// @brief Register scene query functions
  static void RegisterSceneQueries(sol::state &lua, Scene::Scene *scene);

  /// @brief Register time/delta utility
  static void RegisterTime(sol::state &lua);
};

} // namespace Aetherion::Scripting
