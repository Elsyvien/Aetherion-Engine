#ifdef AETHERION_ENABLE_LUA
#include <sol/sol.hpp>
#endif

#include "Aetherion/Scripting/LuaScriptInstance.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"

#include <fstream>
#include <iostream>
#include <sstream>


namespace Aetherion::Scripting {

struct LuaScriptInstance::ScriptData {
#ifdef AETHERION_ENABLE_LUA
  sol::table scriptTable;
  sol::protected_function onCreate;
  sol::protected_function onUpdate;
  sol::protected_function onDestroy;
#endif
};

LuaScriptInstance::LuaScriptInstance(sol::state *luaState,
                                     const std::string &scriptSource,
                                     bool isFile)
    : m_luaState(luaState), m_source(scriptSource), m_isFile(isFile),
      m_scriptData(std::make_unique<ScriptData>()) {
  LoadScript();
}

LuaScriptInstance::~LuaScriptInstance() = default;

bool LuaScriptInstance::LoadScript() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_luaState) {
    m_error = "No Lua state provided";
    m_valid = false;
    return false;
  }

  try {
    sol::state &lua = *m_luaState;

    // Execute the script to get the behavior table
    sol::protected_function_result result;

    if (m_isFile) {
      // Load from file
      result = lua.safe_script_file(m_source, sol::script_pass_on_error);
    } else {
      // Execute inline code
      result = lua.safe_script(m_source, sol::script_pass_on_error);
    }

    if (!result.valid()) {
      sol::error err = result;
      m_error = err.what();
      m_valid = false;
      return false;
    }

    // Check if the script returned a table (common pattern)
    if (result.get_type() == sol::type::table) {
      m_scriptData->scriptTable = result;
    } else {
      // Script might register globally - try to find it by name
      // Extract name from file path
      std::string scriptName = m_source;
      size_t lastSlash = scriptName.find_last_of("/\\");
      if (lastSlash != std::string::npos) {
        scriptName = scriptName.substr(lastSlash + 1);
      }
      size_t dot = scriptName.find('.');
      if (dot != std::string::npos) {
        scriptName = scriptName.substr(0, dot);
      }

      sol::object global = lua[scriptName];
      if (global.is<sol::table>()) {
        m_scriptData->scriptTable = global;
      } else {
        // Create empty table as fallback
        m_scriptData->scriptTable = lua.create_table();
      }
    }

    // Cache function references for performance
    m_scriptData->onCreate = m_scriptData->scriptTable["on_create"];
    m_scriptData->onUpdate = m_scriptData->scriptTable["on_update"];
    m_scriptData->onDestroy = m_scriptData->scriptTable["on_destroy"];

    m_valid = true;
    return true;
  } catch (const std::exception &e) {
    m_error = e.what();
    m_valid = false;
    return false;
  }
#else
  m_error = "Lua not enabled";
  m_valid = false;
  return false;
#endif
}

void LuaScriptInstance::SetEntity(Scene::Entity *entity) {
  m_entity = entity;
#ifdef AETHERION_ENABLE_LUA
  if (!m_entity && m_valid && m_luaState) {
    m_scriptData->scriptTable["entity_id"] = sol::lua_nil;
    m_scriptData->scriptTable["entity_name"] = sol::lua_nil;
    m_scriptData->scriptTable["position"] = sol::lua_nil;
    return;
  }
#endif
  BindEntityToLua();
}

void LuaScriptInstance::BindEntityToLua() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_valid || !m_entity || !m_luaState)
    return;

  // Store entity reference in the script table
  m_scriptData->scriptTable["entity_id"] = m_entity->GetId();
  m_scriptData->scriptTable["entity_name"] = m_entity->GetName();

  // For convenience, store transform data that can be accessed directly
  if (auto transform = m_entity->GetComponent<Scene::TransformComponent>()) {
    sol::table pos = m_luaState->create_table();
    pos["x"] = transform->GetPositionX();
    pos["y"] = transform->GetPositionY();
    pos["z"] = transform->GetPositionZ();
    m_scriptData->scriptTable["position"] = pos;
  }
#endif
}

void LuaScriptInstance::OnCreate() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_valid || !m_scriptData->onCreate.valid())
    return;

  try {
    sol::protected_function_result result =
        m_scriptData->onCreate(m_scriptData->scriptTable);
    if (!result.valid()) {
      sol::error err = result;
      std::cerr << "[LuaScript] on_create error: " << err.what() << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[LuaScript] on_create exception: " << e.what() << std::endl;
  }
#endif
}

void LuaScriptInstance::OnUpdate(float deltaTime) {
#ifdef AETHERION_ENABLE_LUA
  if (!m_valid || !m_scriptData->onUpdate.valid())
    return;

  try {
    // Update position data before calling update
    if (m_entity) {
      if (auto transform =
              m_entity->GetComponent<Scene::TransformComponent>()) {
        sol::table pos = m_scriptData->scriptTable["position"];
        if (pos.valid()) {
          pos["x"] = transform->GetPositionX();
          pos["y"] = transform->GetPositionY();
          pos["z"] = transform->GetPositionZ();
        }
      }
    }

    sol::protected_function_result result =
        m_scriptData->onUpdate(m_scriptData->scriptTable, deltaTime);

    if (!result.valid()) {
      sol::error err = result;
      std::cerr << "[LuaScript] on_update error: " << err.what() << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[LuaScript] on_update exception: " << e.what() << std::endl;
  }
#else
  (void)deltaTime;
#endif
}

void LuaScriptInstance::OnDestroy() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_valid || !m_scriptData->onDestroy.valid())
    return;

  try {
    sol::protected_function_result result =
        m_scriptData->onDestroy(m_scriptData->scriptTable);
    if (!result.valid()) {
      sol::error err = result;
      std::cerr << "[LuaScript] on_destroy error: " << err.what() << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "[LuaScript] on_destroy exception: " << e.what() << std::endl;
  }
#endif
}

bool LuaScriptInstance::Reload() {
  m_valid = false;
  m_error.clear();
  return LoadScript();
}

bool LuaScriptInstance::CallFunction(const std::string &functionName) {
#ifdef AETHERION_ENABLE_LUA
  if (!m_valid)
    return false;

  try {
    sol::protected_function fn = m_scriptData->scriptTable[functionName];
    if (!fn.valid())
      return false;

    sol::protected_function_result result = fn(m_scriptData->scriptTable);
    return result.valid();
  } catch (...) {
    return false;
  }
#else
  (void)functionName;
  return false;
#endif
}

} // namespace Aetherion::Scripting
