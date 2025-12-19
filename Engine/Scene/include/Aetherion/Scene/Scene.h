#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Runtime
{
class EngineContext;
} // namespace Aetherion::Runtime

namespace Aetherion::Scene
{
class Entity;
class System;

class Scene
{
public:
    Scene();
    explicit Scene(std::string name);
    ~Scene();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    void AddEntity(std::shared_ptr<Entity> entity);
    [[nodiscard]] const std::vector<std::shared_ptr<Entity>>& GetEntities() const noexcept;
    [[nodiscard]] std::shared_ptr<Entity> FindEntityById(Core::EntityId id) const noexcept;

    bool SetParent(Core::EntityId childId, Core::EntityId newParentId);

    void AddSystem(std::shared_ptr<System> system);
    [[nodiscard]] const std::vector<std::shared_ptr<System>>& GetSystems() const noexcept;

    [[nodiscard]] const std::string& GetName() const noexcept;
    void SetName(std::string name);

    void BindContext(Runtime::EngineContext& context);

    // TODO: Replace collections with ECS registries and scheduler integration.
private:
    std::string m_name;
    Runtime::EngineContext* m_context = nullptr;
    std::vector<std::shared_ptr<Entity>> m_entities;
    std::vector<std::shared_ptr<System>> m_systems;
};
} // namespace Aetherion::Scene
