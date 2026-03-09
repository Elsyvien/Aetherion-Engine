#include "Aetherion/Scripting/ScriptingSystem.h"

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/ScriptComponent.h"
#include "Aetherion/Scripting/ScriptInstance.h"
#include "Aetherion/Scripting/LuaScriptEngine.h"
#include "Aetherion/Runtime/EngineContext.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <unordered_set>

namespace Aetherion::Scripting {
namespace {

struct ResolvedScriptSource {
  std::string source;
  bool isFile{false};
  std::filesystem::path path;
  std::filesystem::file_time_type lastWriteTime{};
};

std::filesystem::path ResolvePathCandidate(
    const std::filesystem::path &candidate,
    const std::filesystem::path &assetsRoot) {
  if (candidate.empty()) {
    return {};
  }

  std::error_code ec;
  if (candidate.is_absolute()) {
    if (std::filesystem::exists(candidate, ec)) {
      return std::filesystem::weakly_canonical(candidate, ec);
    }
    return {};
  }

  const auto tryExisting = [&](const std::filesystem::path &path) {
    std::error_code localEc;
    if (path.empty() || !std::filesystem::exists(path, localEc)) {
      return std::filesystem::path{};
    }
    return std::filesystem::weakly_canonical(path, localEc);
  };

  if (!assetsRoot.empty()) {
    if (auto resolved = tryExisting(assetsRoot / candidate); !resolved.empty()) {
      return resolved;
    }

    if (!candidate.empty() && candidate.begin()->string() == "assets") {
      if (auto resolved =
              tryExisting(assetsRoot.parent_path() / candidate);
          !resolved.empty()) {
        return resolved;
      }
    }
  }

  if (auto resolved = tryExisting(candidate); !resolved.empty()) {
    return resolved;
  }

  return {};
}

ResolvedScriptSource ResolveScriptSource(
    const Scene::ScriptComponent &component,
    const Assets::AssetRegistry *assetRegistry) {
  ResolvedScriptSource resolved;

  std::filesystem::path assetsRoot;
  if (assetRegistry) {
    assetsRoot = assetRegistry->GetRootPath();
  }

  const std::string &inlineSource = component.GetScriptSource();
  if (component.GetSourceMode() ==
          Scene::ScriptComponent::SourceMode::InlineCode &&
      !inlineSource.empty()) {
    resolved.source = inlineSource;
    return resolved;
  }

  const auto finalizeFileResolution =
      [&](const std::filesystem::path &sourcePath) {
    std::error_code existsEc;
    if (sourcePath.empty() || !std::filesystem::exists(sourcePath, existsEc)) {
      return false;
    }

    std::error_code ec;
    resolved.source = sourcePath.string();
    resolved.isFile = true;
    resolved.path = sourcePath;
    resolved.lastWriteTime = std::filesystem::last_write_time(sourcePath, ec);
    return true;
  };

  const std::string &assetId = component.GetScriptAssetId();
  if (!assetId.empty()) {
    if (assetRegistry) {
      if (const auto *entry = assetRegistry->FindEntry(assetId)) {
        if (auto sourcePath = ResolvePathCandidate(entry->path, assetsRoot);
            !sourcePath.empty()) {
          if (finalizeFileResolution(sourcePath)) {
            return resolved;
          }
        }
      }
    }

    if (auto sourcePath =
            ResolvePathCandidate(std::filesystem::path(assetId), assetsRoot);
        !sourcePath.empty()) {
      if (finalizeFileResolution(sourcePath)) {
        return resolved;
      }
    }
  }

  const std::string &pathSource = component.GetScriptSource();
  if (!pathSource.empty()) {
    if (auto sourcePath =
            ResolvePathCandidate(std::filesystem::path(pathSource), assetsRoot);
        !sourcePath.empty()) {
      (void)finalizeFileResolution(sourcePath);
    }
  }

  return resolved;
}

} // namespace

ScriptingSystem::ScriptingSystem(ScriptEngine *engine) : m_engine(engine) {}

ScriptingSystem::~ScriptingSystem() { UnbindScene(); }

void ScriptingSystem::BindScene(Scene::Scene *scene) {
  if (m_scene == scene)
    return;
  UnbindScene();
  m_scene = scene;

  if (auto *luaEngine = dynamic_cast<LuaScriptEngine *>(m_engine)) {
    luaEngine->SetScene(scene);
  }
}

void ScriptingSystem::UnbindScene() {
  ResetInstances();

  if (auto *luaEngine = dynamic_cast<LuaScriptEngine *>(m_engine)) {
    luaEngine->SetScene(nullptr);
  }
  m_scene = nullptr;
}

void ScriptingSystem::ResetInstances() {
  for (auto &[entityId, scripts] : m_entityScripts) {
    (void)entityId;
    for (auto &script : scripts) {
      if (script.instance) {
        script.instance->OnDestroy();
      }
    }
  }
  m_entityScripts.clear();
}

void ScriptingSystem::Update(float deltaTime) {
  if (!m_scene || !m_engine || !m_enabled)
    return;

  // Update engine-level scripting tasks (e.g., set deltaTime in Lua)
  m_engine->OnUpdate(deltaTime);

  Assets::AssetRegistry *assetRegistry = nullptr;
  if (auto *context = m_scene->GetContext()) {
    auto registryRef = context->GetAssetRegistry();
    assetRegistry = registryRef.get();
  }

  const auto &entities = m_scene->GetEntities();

  // Track which entities still exist
  std::vector<Core::EntityId> activeEntityIds;
  std::unordered_set<Core::EntityId> configuredEntityIds;

  for (const auto &entity : entities) {
    if (!entity)
      continue;

    auto entityId = entity->GetId();
    activeEntityIds.push_back(entityId);

    // Check if entity has a ScriptComponent
    auto scriptComp = entity->GetComponent<Scene::ScriptComponent>();
    if (!scriptComp) {
      auto existing = m_entityScripts.find(entityId);
      if (existing != m_entityScripts.end()) {
        for (auto &script : existing->second) {
          if (script.instance) {
            script.instance->OnDestroy();
          }
        }
        m_entityScripts.erase(existing);
      }
      continue;
    }

    const ResolvedScriptSource resolved =
        ResolveScriptSource(*scriptComp, assetRegistry);
    if (resolved.source.empty()) {
      auto existing = m_entityScripts.find(entityId);
      if (existing != m_entityScripts.end()) {
        for (auto &script : existing->second) {
          if (script.instance) {
            script.instance->OnDestroy();
          }
        }
        m_entityScripts.erase(existing);
      }
      continue;
    }

    configuredEntityIds.insert(entityId);

    // Check if we already have instances for this entity
    auto it = m_entityScripts.find(entityId);
    if (it == m_entityScripts.end()) {
      auto instance = m_engine->CreateInstance(
          resolved.source, resolved.isFile ? ScriptEngine::SourceKind::FilePath
                                           : ScriptEngine::SourceKind::InlineCode);
      if (instance) {
        instance->SetEntity(entity.get());
        instance->OnCreate();

        EntityScript es;
        es.instance = std::move(instance);
        es.scriptSource = resolved.source;
        es.isFileSource = resolved.isFile;
        es.sourcePath = resolved.path;
        es.lastWriteTime = resolved.lastWriteTime;

        m_entityScripts[entityId].push_back(std::move(es));
      }
    } else {
      bool scriptChanged = it->second.empty();
      if (!scriptChanged) {
        const auto &existing = it->second.front();
        scriptChanged = existing.scriptSource != resolved.source ||
                        existing.isFileSource != resolved.isFile;
        if (!scriptChanged && resolved.isFile) {
          scriptChanged = existing.sourcePath != resolved.path ||
                          existing.lastWriteTime != resolved.lastWriteTime;
        }
      }

      if (scriptChanged) {
        for (auto &existing : it->second) {
          if (existing.instance) {
            existing.instance->OnDestroy();
          }
        }
        it->second.clear();

        auto instance = m_engine->CreateInstance(
            resolved.source,
            resolved.isFile ? ScriptEngine::SourceKind::FilePath
                            : ScriptEngine::SourceKind::InlineCode);
        if (instance) {
          instance->SetEntity(entity.get());
          instance->OnCreate();

          EntityScript es;
          es.instance = std::move(instance);
          es.scriptSource = resolved.source;
          es.isFileSource = resolved.isFile;
          es.sourcePath = resolved.path;
          es.lastWriteTime = resolved.lastWriteTime;

          it->second.push_back(std::move(es));
        }
      }
    }
  }

  // Clean up script instances for entities that no longer exist
  for (auto it = m_entityScripts.begin(); it != m_entityScripts.end();) {
    const bool entityExists =
        std::find(activeEntityIds.begin(), activeEntityIds.end(), it->first) !=
        activeEntityIds.end();
    const bool shouldRemainConfigured =
        configuredEntityIds.find(it->first) != configuredEntityIds.end();
    if (!entityExists || !shouldRemainConfigured) {
      for (auto &script : it->second) {
        if (script.instance) {
          if (!entityExists) {
            script.instance->SetEntity(nullptr);
          }
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
