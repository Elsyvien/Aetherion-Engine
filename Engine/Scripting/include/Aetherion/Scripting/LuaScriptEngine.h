#pragma once

#include "Aetherion/Scripting/ScriptEngine.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

// Forward declare sol types to avoid exposing Lua headers in the public API
namespace sol {
class state;
}

namespace Aetherion::Scene {
class Scene;
class Entity;
} // namespace Aetherion::Scene

namespace Aetherion::Scripting {

/// @brief Lua-based script engine implementation
/// Provides a lightweight, fast scripting runtime for game behaviors
class LuaScriptEngine : public ScriptEngine {
public:
  LuaScriptEngine();
  ~LuaScriptEngine() override;

  // ScriptEngine interface
  void Initialize() override;
  void Shutdown() override;
  std::unique_ptr<ScriptInstance>
  CreateInstance(const std::string &scriptSource) override;
  void OnUpdate(float deltaTime) override;

  /// @brief Set the current scene context for script bindings
  void SetScene(Scene::Scene *scene);

  /// @brief Execute a Lua string directly (useful for console/debugging)
  bool ExecuteString(const std::string &code, std::string &errorOut);

  /// @brief Hot-reload a script file
  bool ReloadScript(const std::string &scriptPath);

  /// @brief Add a search path for Lua require()
  void AddPackagePath(const std::filesystem::path &path);

  /// @brief Check if engine is initialized
  [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
  void RegisterEngineBindings();
  void RegisterEntityBindings();
  void RegisterComponentBindings();
  void RegisterMathBindings();
  void UpdatePackagePaths();

  struct Impl;
  std::unique_ptr<Impl> m_impl;
  bool m_initialized{false};
  Scene::Scene *m_currentScene{nullptr};
};

} // namespace Aetherion::Scripting
