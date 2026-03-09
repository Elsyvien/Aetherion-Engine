#pragma once

#include "Aetherion/Core/Types.h"
#include "Aetherion/Scripting/ScriptEngine.h"
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Aetherion::Scene {
class Scene;
}

namespace Aetherion::Scripting {

/// @brief Manages scripting for a scene
class ScriptingSystem {
public:
  explicit ScriptingSystem(ScriptEngine *engine);
  ~ScriptingSystem();

  void BindScene(Scene::Scene *scene);
  void UnbindScene();

  void Update(float deltaTime);

  void SetEnabled(bool enabled) noexcept { m_enabled = enabled; }
  [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }
  void ResetInstances();

private:
  ScriptEngine *m_engine = nullptr;
  Scene::Scene *m_scene = nullptr;
  bool m_enabled = true;

  struct EntityScript {
    std::unique_ptr<ScriptInstance> instance;
    std::string scriptSource;
    bool isFileSource{false};
    std::filesystem::path sourcePath;
    std::filesystem::file_time_type lastWriteTime{};
  };

  std::unordered_map<Core::EntityId, std::vector<EntityScript>> m_entityScripts;
};

} // namespace Aetherion::Scripting
