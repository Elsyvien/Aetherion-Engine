#include "Aetherion/Scripting/ScriptingSystem.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/ScriptComponent.h"
#include "Aetherion/Scripting/ScriptInstance.h"

#include <algorithm>
#include <iostream>

namespace Aetherion::Scripting {

ScriptingSystem::ScriptingSystem(ScriptEngine *engine) : m_engine(engine) {}

ScriptingSystem::~ScriptingSystem() { UnbindScene(); }

void ScriptingSystem::BindScene(Scene::Scene *scene) {
  if (m_scene == scene)
    return;
  UnbindScene();
  m_scene = scene;
}

void ScriptingSystem::UnbindScene() {
  // Destroy all script instances
  for (auto &[entityId, scripts] : m_entityScripts) {
    for (auto &script : scripts) {
      if (script.instance) {
        script.instance->OnDestroy();
      }
    }
  }
  m_entityScripts.clear();
  m_scene = nullptr;
}

void ScriptingSystem::Update(float deltaTime) {
  if (!m_scene || !m_engine || !m_enabled)
    return;

  // Update engine-level scripting tasks (e.g., set deltaTime in Lua)
  m_engine->OnUpdate(deltaTime);

  const auto &entities = m_scene->GetEntities();

  // Track which entities still exist
  std::vector<Core::EntityId> activeEntityIds;

  for (const auto &entity : entities) {
    if (!entity)
      continue;

    auto entityId = entity->GetId();
    activeEntityIds.push_back(entityId);

    // Check if entity has a ScriptComponent
    auto scriptComp = entity->GetComponent<Scene::ScriptComponent>();
    if (!scriptComp)
      continue;

    const std::string &scriptSource = scriptComp->GetScriptSource();
    if (scriptSource.empty())
      continue;

    // Check if we already have instances for this entity
    auto it = m_entityScripts.find(entityId);
    if (it == m_entityScripts.end()) {
      // Create new script instance for this entity
      auto instance = m_engine->CreateInstance(scriptSource);
      if (instance) {
        instance->SetEntity(entity.get());
        instance->OnCreate();

        EntityScript es;
        es.instance = std::move(instance);
        es.scriptSource = scriptSource;

        m_entityScripts[entityId].push_back(std::move(es));
      }
    } else {
      // Check if script source changed (hot reload scenario)
      bool scriptChanged = true;
      for (const auto &es : it->second) {
        if (es.scriptSource == scriptSource) {
          scriptChanged = false;
          break;
        }
      }

      if (scriptChanged && !it->second.empty()) {
        // Destroy old instances
        for (auto &es : it->second) {
          if (es.instance) {
            es.instance->OnDestroy();
          }
        }
        it->second.clear();

        // Create new instance with updated script
        auto instance = m_engine->CreateInstance(scriptSource);
        if (instance) {
          instance->SetEntity(entity.get());
          instance->OnCreate();

          EntityScript es;
          es.instance = std::move(instance);
          es.scriptSource = scriptSource;

          it->second.push_back(std::move(es));
        }
      }
    }
  }

  // Clean up script instances for entities that no longer exist
  for (auto it = m_entityScripts.begin(); it != m_entityScripts.end();) {
    bool entityExists =
        std::find(activeEntityIds.begin(), activeEntityIds.end(), it->first) !=
        activeEntityIds.end();
    if (!entityExists) {
      for (auto &script : it->second) {
        if (script.instance) {
          script.instance->OnDestroy();
        }
      }
      it = m_entityScripts.erase(it);
    } else {
      ++it;
    }
  }

  // Update all active script instances
  for (auto &[entityId, scripts] : m_entityScripts) {
    for (auto &script : scripts) {
      if (script.instance) {
        script.instance->OnUpdate(deltaTime);
      }
    }
  }
}

} // namespace Aetherion::Scripting
