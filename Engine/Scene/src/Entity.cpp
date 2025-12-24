#include "Aetherion/Scene/Entity.h"

#include "Aetherion/Scene/Component.h"
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
    // TODO: Delegate to ECS storage and component lifecycle management.
    m_components.push_back(std::move(component));
}

void Entity::RemoveComponent(const std::shared_ptr<Component>& component)
{
    std::erase(m_components, component);
}

const std::vector<std::shared_ptr<Component>>& Entity::GetComponents() const noexcept
{
    return m_components;
}
} // namespace Aetherion::Scene
