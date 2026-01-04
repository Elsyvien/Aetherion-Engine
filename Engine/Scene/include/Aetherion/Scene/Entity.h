#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Component;
class Scene;

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
    template <typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args)
    {
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        AddComponent(component);
        return component;
    }
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

    template <typename T>
    [[nodiscard]] bool HasComponent() const
    {
        return static_cast<bool>(GetComponent<T>());
    }

    [[nodiscard]] Scene* GetScene() const noexcept { return m_scene; }

    // TODO: Replace with ECS-backed component storage and queries.
private:
    void BindScene(Scene* scene);

    Core::EntityId m_id;
    std::string m_name;
    std::vector<std::shared_ptr<Component>> m_components;
    Scene* m_scene{nullptr};

    friend class Scene;
};
} // namespace Aetherion::Scene
