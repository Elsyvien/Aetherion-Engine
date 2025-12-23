#include "Aetherion/Scene/Scene.h"

#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/System.h"
#include "Aetherion/Scene/TransformComponent.h"

#include <algorithm>
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

void Scene::RemoveEntity(Core::EntityId id)
{
    if (id == 0)
    {
        return;
    }

    // First, unparent any children of this entity
    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }
        auto transform = entity->GetComponent<TransformComponent>();
        if (transform && transform->GetParentId() == id)
        {
            transform->ClearParent();
        }
    }

    // Remove the entity from the list
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
                       [id](const std::shared_ptr<Entity>& e) {
                           return e && e->GetId() == id;
                       }),
        m_entities.end());
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

bool Scene::SetParent(Core::EntityId childId, Core::EntityId newParentId)
{
    if (childId == 0 || childId == newParentId)
    {
        return false;
    }

    auto child = FindEntityById(childId);
    if (!child)
    {
        return false;
    }

    auto childTransform = child->GetComponent<TransformComponent>();
    if (!childTransform)
    {
        return false;
    }

    // Prevent cycles by walking the ancestry chain of the prospective parent.
    Core::EntityId cursor = newParentId;
    while (cursor != 0)
    {
        if (cursor == childId)
        {
            return false;
        }

        auto ancestor = FindEntityById(cursor);
        auto ancestorTransform = ancestor ? ancestor->GetComponent<TransformComponent>() : nullptr;
        cursor = ancestorTransform ? ancestorTransform->GetParentId() : 0;
    }

    const Core::EntityId oldParentId = childTransform->GetParentId();
    if (oldParentId == newParentId)
    {
        return true;
    }

    // Detach from old parent list.
    if (oldParentId != 0)
    {
        if (auto oldParent = FindEntityById(oldParentId))
        {
            if (auto oldParentTransform = oldParent->GetComponent<TransformComponent>())
            {
                oldParentTransform->RemoveChild(childId);
            }
        }
    }

    // Attach to new parent if provided.
    if (newParentId != 0)
    {
        auto newParent = FindEntityById(newParentId);
        if (!newParent)
        {
            childTransform->ClearParent();
            return false;
        }

        auto parentTransform = newParent->GetComponent<TransformComponent>();
        if (!parentTransform)
        {
            childTransform->ClearParent();
            return false;
        }

        childTransform->SetParent(newParentId);
        parentTransform->AddChild(childId);
        return true;
    }

    // Root-level entity (no parent).
    childTransform->ClearParent();
    return true;
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
