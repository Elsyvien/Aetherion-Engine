#include "Aetherion/Scene/Entity.h"

#include "Aetherion/Scene/Component.h"
#include "Aetherion/Scene/Scene.h"

#include <algorithm>
#include <utility>

namespace Aetherion::Scene
{
Entity::Entity(Core::EntityId id, std::string name)
    : m_id(id)
    , m_name(std::move(name))
{
    // TODO: Integrate with centralized entity registry.
}

Core::EntityId Entity::GetId() const noexcept
{
    return m_id;
}

const std::string& Entity::GetName() const noexcept
{
    return m_name;
}

void Entity::SetName(std::string name)
{
    m_name = std::move(name);
}

void Entity::AddComponent(std::shared_ptr<Component> component)
{
    if (!component)
    {
        return;
    }

    auto existing = std::find(m_components.begin(), m_components.end(), component);
    if (existing != m_components.end())
    {
        return;
    }

    component->Bind(this, m_scene);
    if (m_scene && m_scene->IsPlaying())
    {
        component->BeginPlay();
    }

    // TODO: Delegate to ECS storage and component lifecycle management.
    m_components.push_back(std::move(component));
}

void Entity::RemoveComponent(const std::shared_ptr<Component>& component)
{
    if (!component)
    {
        return;
    }

    auto it = std::find(m_components.begin(), m_components.end(), component);
    if (it == m_components.end())
    {
        return;
    }

    (*it)->Unbind();
    m_components.erase(it);
}

const std::vector<std::shared_ptr<Component>>& Entity::GetComponents() const noexcept
{
    return m_components;
}

void Entity::BindScene(Scene* scene)
{
    if (m_scene == scene)
    {
        return;
    }

    m_scene = scene;
    for (const auto& component : m_components)
    {
        if (!component)
        {
            continue;
        }
        if (scene)
        {
            component->Bind(this, scene);
            if (scene->IsPlaying())
            {
                component->BeginPlay();
            }
        }
        else
        {
            component->Unbind();
        }
    }
}
} // namespace Aetherion::Scene
