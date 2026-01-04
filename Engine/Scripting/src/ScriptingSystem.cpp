#include "Aetherion/Scripting/ScriptingSystem.h"
#include "Aetherion/Scripting/ScriptInstance.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/ScriptComponent.h"

namespace Aetherion::Scripting {

ScriptingSystem::ScriptingSystem(ScriptEngine* engine) : m_engine(engine) {}

ScriptingSystem::~ScriptingSystem() {
    UnbindScene();
}

void ScriptingSystem::BindScene(Scene::Scene* scene) {
    if (m_scene == scene) return;
    UnbindScene();
    m_scene = scene;
}

void ScriptingSystem::UnbindScene() {
    for (auto& [entityId, scripts] : m_entityScripts) {
        for (auto& script : scripts) {
            script.instance->OnDestroy();
        }
    }
    m_entityScripts.clear();
    m_scene = nullptr;
}

void ScriptingSystem::Update(float deltaTime) {
    if (!m_scene || !m_engine || !m_enabled) return;

    m_engine->OnUpdate(deltaTime);

    const auto& entities = m_scene->GetEntities();
    for (const auto& entity : entities) {
        // Here we would check for a ScriptComponent and manage its instances.
        // For now, this is scaffolding.
        
        // Example logic:
        // if (auto scriptComp = entity->GetComponent<Scene::ScriptComponent>()) {
        //     auto entityId = entity->GetId();
        //     if (m_entityScripts.find(entityId) == m_entityScripts.end()) {
        //         auto instance = m_engine->CreateInstance(scriptComp->GetScriptSource());
        //         instance->SetEntity(entity.get());
        //         instance->OnCreate();
        //         m_entityScripts[entityId].push_back({ std::move(instance) });
        //     }
        // }
    }

    // Update existing instances
    for (auto& [entityId, scripts] : m_entityScripts) {
        for (auto& script : scripts) {
            script.instance->OnUpdate(deltaTime);
        }
    }
}

} // namespace Aetherion::Scripting
