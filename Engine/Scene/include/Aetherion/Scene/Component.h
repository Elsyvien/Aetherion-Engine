#pragma once

#include <string>

namespace Aetherion::Scene
{
class Entity;
class Scene;
}

namespace Aetherion::Scene
{
class Component
{
public:
    Component() = default;
    virtual ~Component() = default;

    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;

    [[nodiscard]] virtual std::string GetDisplayName() const = 0;
    [[nodiscard]] Entity* GetEntity() const noexcept { return m_entity; }
    [[nodiscard]] Scene* GetScene() const noexcept { return m_scene; }
    [[nodiscard]] bool HasBegunPlay() const noexcept { return m_started; }

protected:
    virtual void OnAdded() {}
    virtual void OnRemoved() {}
    virtual void OnBeginPlay() {}
    virtual void OnEndPlay() {}
    virtual void OnUpdate(float deltaTime) { (void)deltaTime; }

private:
    void Bind(Entity* entity, Scene* scene);
    void Unbind();
    void BeginPlay();
    void EndPlay();
    void Update(float deltaTime);

    Entity* m_entity{nullptr};
    Scene* m_scene{nullptr};
    bool m_started{false};

    friend class Entity;
    friend class Scene;
};
} // namespace Aetherion::Scene
