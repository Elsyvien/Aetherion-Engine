#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Component;

class Entity : public std::enable_shared_from_this<Entity>
{
public:
    explicit Entity(Core::EntityId id, std::string name = {});
    virtual ~Entity() = default;

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    [[nodiscard]] Core::EntityId GetId() const noexcept;

    [[nodiscard]] const std::string& GetName() const noexcept;
    void SetName(std::string name);

    void AddComponent(std::shared_ptr<Component> component);
    void RemoveComponent(const std::shared_ptr<Component>& component);
    [[nodiscard]] const std::vector<std::shared_ptr<Component>>& GetComponents() const noexcept;

    template <typename T>
    [[nodiscard]] std::shared_ptr<T> GetComponent() const
    {
        for (const auto& component : m_components)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(component))
            {
                return casted;
            }
        }
        return nullptr;
    }

    // TODO: Replace with ECS-backed component storage and queries.
private:
    Core::EntityId m_id;
    std::string m_name;
    std::vector<std::shared_ptr<Component>> m_components;
};
} // namespace Aetherion::Scene
