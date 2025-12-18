#include "Aetherion/Scene/Scene.h"

#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/System.h"
#include <utility>

namespace Aetherion::Scene
{
Scene::Scene() = default;

Scene::Scene(std::string name)
    : m_name(std::move(name))
{
}

Scene::~Scene() = default;

void Scene::AddEntity(std::shared_ptr<Entity> entity)
{
    // TODO: Route through ECS world to ensure deterministic ordering and ownership.
    m_entities.push_back(std::move(entity));
}

const std::vector<std::shared_ptr<Entity>>& Scene::GetEntities() const noexcept
{
    return m_entities;
}

std::shared_ptr<Entity> Scene::FindEntityById(Core::EntityId id) const noexcept
{
    for (const auto& entity : m_entities)
    {
        if (entity && entity->GetId() == id)
        {
            return entity;
        }
    }
    return nullptr;
}

void Scene::AddSystem(std::shared_ptr<System> system)
{
    // TODO: Integrate with scheduler and dependency graph once available.
    m_systems.push_back(std::move(system));
}

const std::vector<std::shared_ptr<System>>& Scene::GetSystems() const noexcept
{
    return m_systems;
}

const std::string& Scene::GetName() const noexcept
{
    return m_name;
}

void Scene::SetName(std::string name)
{
    m_name = std::move(name);
    // TODO: Propagate name change to editor and runtime observers.
}

void Scene::BindContext(Runtime::EngineContext& context)
{
    m_context = &context;
    // TODO: Use context to resolve services for systems and entities.
}
} // namespace Aetherion::Scene
