#pragma once

#include <memory>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Component;

class Entity : public std::enable_shared_from_this<Entity>
{
public:
    explicit Entity(Core::EntityId id);
    virtual ~Entity() = default;

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    [[nodiscard]] Core::EntityId GetId() const noexcept;

    void AddComponent(std::shared_ptr<Component> component);
    [[nodiscard]] const std::vector<std::shared_ptr<Component>>& GetComponents() const noexcept;

    // TODO: Replace with ECS-backed component storage and queries.
private:
    Core::EntityId m_id;
    std::vector<std::shared_ptr<Component>> m_components;
};
} // namespace Aetherion::Scene
