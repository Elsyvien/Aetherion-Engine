#pragma once

#include "Aetherion/Scripting/ScriptInstance.h"
#include <memory>
#include <string>

// Forward declare sol types
namespace sol {
class state;
} // namespace sol

namespace Aetherion::Scripting {

/// @brief A Lua script instance attached to an entity
/// Manages a Lua table with lifecycle functions (on_create, on_update,
/// on_destroy)
class LuaScriptInstance : public ScriptInstance {
public:
  /// @brief Create from a Lua script source (file path or inline code)
  /// @param luaState The shared Lua state
  /// @param scriptSource Either a file path (.lua) or inline Lua code
  /// @param isFile If true, scriptSource is treated as a file path
  LuaScriptInstance(sol::state *luaState, const std::string &scriptSource,
                    bool isFile = true);
  ~LuaScriptInstance() override;

  // ScriptInstance interface
  void SetEntity(Scene::Entity *entity) override;
  void OnCreate() override;
  void OnUpdate(float deltaTime) override;
  void OnDestroy() override;

  /// @brief Check if the script loaded successfully
  [[nodiscard]] bool IsValid() const noexcept { return m_valid; }

  /// @brief Get any error message from loading/running
  [[nodiscard]] const std::string &GetError() const noexcept { return m_error; }

  /// @brief Get the script source path or identifier
  [[nodiscard]] const std::string &GetSource() const noexcept {
    return m_source;
  }

  /// @brief Reload the script from its source
  bool Reload();

  /// @brief Call a custom function on the script
  bool CallFunction(const std::string &functionName);

private:
  bool LoadScript();
  void BindEntityToLua();

  sol::state *m_luaState{nullptr};
  std::string m_source;
  bool m_isFile{true};
  bool m_valid{false};
  std::string m_error;
  Scene::Entity *m_entity{nullptr};

  struct ScriptData;
  std::unique_ptr<ScriptData> m_scriptData;
};

} // namespace Aetherion::Scripting
