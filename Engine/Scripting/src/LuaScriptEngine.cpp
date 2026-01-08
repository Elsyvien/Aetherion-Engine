#ifdef AETHERION_ENABLE_LUA
#include <sol/sol.hpp>
#endif

#include "Aetherion/Scripting/LuaScriptEngine.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scripting/LuaBindings.h"
#include "Aetherion/Scripting/LuaScriptInstance.h"

#include <iostream>
#include <sstream>

namespace Aetherion::Scripting {

struct LuaScriptEngine::Impl {
#ifdef AETHERION_ENABLE_LUA
  std::unique_ptr<sol::state> luaState;
#endif
  std::vector<std::filesystem::path> packagePaths;
};

LuaScriptEngine::LuaScriptEngine() : m_impl(std::make_unique<Impl>()) {}

LuaScriptEngine::~LuaScriptEngine() {
  if (m_initialized) {
    Shutdown();
  }
}

void LuaScriptEngine::Initialize() {
  if (m_initialized)
    return;

#ifdef AETHERION_ENABLE_LUA
  m_impl->luaState = std::make_unique<sol::state>();
  m_impl->luaState->open_libraries(sol::lib::base, sol::lib::math,
                                   sol::lib::string, sol::lib::table,
                                   sol::lib::coroutine,
                                   sol::lib::os, // Limited OS functionality
                                   sol::lib::package);

  // Set up package paths
  UpdatePackagePaths();

  // Register engine bindings
  RegisterEngineBindings();

  m_initialized = true;
#else
  // Lua not enabled - log warning
  std::cerr << "[LuaScriptEngine] Lua scripting not enabled. Build with "
               "AETHERION_ENABLE_LUA=ON"
            << std::endl;
  m_initialized = false;
#endif
}

void LuaScriptEngine::Shutdown() {
  if (!m_initialized)
    return;

#ifdef AETHERION_ENABLE_LUA
  m_impl->luaState.reset();
#endif

  m_initialized = false;
  m_currentScene = nullptr;
}

std::unique_ptr<ScriptInstance>
LuaScriptEngine::CreateInstance(const std::string &scriptSource) {
#ifdef AETHERION_ENABLE_LUA
  if (!m_initialized || !m_impl->luaState) {
    return nullptr;
  }

  // Determine if source is a file path or inline code
  bool isFile = (scriptSource.find(".lua") != std::string::npos);

  auto instance = std::make_unique<LuaScriptInstance>(m_impl->luaState.get(),
                                                      scriptSource, isFile);

  return instance;
#else
  (void)scriptSource;
  return nullptr;
#endif
}

void LuaScriptEngine::OnUpdate(float deltaTime) {
#ifdef AETHERION_ENABLE_LUA
  if (!m_initialized || !m_impl->luaState)
    return;

  // Update the global delta time accessible to scripts
  (*m_impl->luaState)["aetherion"]["deltaTime"] = deltaTime;
#else
  (void)deltaTime;
#endif
}

void LuaScriptEngine::SetScene(Scene::Scene *scene) {
  m_currentScene = scene;

#ifdef AETHERION_ENABLE_LUA
  if (m_initialized && m_impl->luaState) {
    // Re-register scene-dependent bindings
    LuaBindings::RegisterSceneQueries(*m_impl->luaState, scene);
  }
#endif
}

bool LuaScriptEngine::ExecuteString(const std::string &code,
                                    std::string &errorOut) {
#ifdef AETHERION_ENABLE_LUA
  if (!m_initialized || !m_impl->luaState) {
    errorOut = "Lua engine not initialized";
    return false;
  }

  try {
    auto result =
        m_impl->luaState->safe_script(code, sol::script_pass_on_error);
    if (!result.valid()) {
      sol::error err = result;
      errorOut = err.what();
      return false;
    }
    return true;
  } catch (const std::exception &e) {
    errorOut = e.what();
    return false;
  }
#else
  (void)code;
  errorOut = "Lua not enabled";
  return false;
#endif
}

bool LuaScriptEngine::ReloadScript(const std::string &scriptPath) {
#ifdef AETHERION_ENABLE_LUA
  if (!m_initialized || !m_impl->luaState)
    return false;

  try {
    auto result = m_impl->luaState->safe_script_file(scriptPath,
                                                     sol::script_pass_on_error);
    if (!result.valid()) {
      sol::error err = result;
      std::cerr << "[LuaScriptEngine] Reload failed: " << err.what()
                << std::endl;
      return false;
    }
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[LuaScriptEngine] Reload exception: " << e.what()
              << std::endl;
    return false;
  }
#else
  (void)scriptPath;
  return false;
#endif
}

void LuaScriptEngine::AddPackagePath(const std::filesystem::path &path) {
  m_impl->packagePaths.push_back(path);

#ifdef AETHERION_ENABLE_LUA
  if (m_initialized && m_impl->luaState) {
    UpdatePackagePaths();
  }
#endif
}

#ifdef AETHERION_ENABLE_LUA
void LuaScriptEngine::UpdatePackagePaths() {
  if (!m_impl->luaState)
    return;

  std::stringstream paths;
  for (const auto &p : m_impl->packagePaths) {
    paths << p.string() << "/?.lua;";
    paths << p.string() << "/?/init.lua;";
  }

  std::string currentPath = (*m_impl->luaState)["package"]["path"];
  (*m_impl->luaState)["package"]["path"] = paths.str() + currentPath;
}
#endif

void LuaScriptEngine::RegisterEngineBindings() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_impl->luaState)
    return;

  auto &lua = *m_impl->luaState;

  // Create main aetherion namespace
  lua["aetherion"] = lua.create_table();
  lua["aetherion"]["deltaTime"] = 0.0f;
  lua["aetherion"]["version"] = "0.0.1";

  // Register all bindings
  LuaBindings::RegisterAll(lua, m_currentScene);
#endif
}

void LuaScriptEngine::RegisterEntityBindings() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_impl->luaState)
    return;
  LuaBindings::RegisterEntity(*m_impl->luaState, m_currentScene);
#endif
}

void LuaScriptEngine::RegisterComponentBindings() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_impl->luaState)
    return;
  LuaBindings::RegisterComponents(*m_impl->luaState);
#endif
}

void LuaScriptEngine::RegisterMathBindings() {
#ifdef AETHERION_ENABLE_LUA
  if (!m_impl->luaState)
    return;
  LuaBindings::RegisterMath(*m_impl->luaState);
#endif
}

} // namespace Aetherion::Scripting
