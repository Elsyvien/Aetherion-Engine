#include "Aetherion/Scene/Entity.h"

#include "Aetherion/Scene/Component.h"
#include <utility>

namespace Aetherion::Scene
{
Entity::Entity(Core::EntityId id)
    : m_id(id)
{
    // TODO: Integrate with centralized entity registry.
}

Core::EntityId Entity::GetId() const noexcept
{
    return m_id;
}

void Entity::AddComponent(std::shared_ptr<Component> component)
{
    // TODO: Delegate to ECS storage and component lifecycle management.
    m_components.push_back(std::move(component));
}

const std::vector<std::shared_ptr<Component>>& Entity::GetComponents() const noexcept
{
    return m_components;
}
} // namespace Aetherion::Scene
