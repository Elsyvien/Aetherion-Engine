#pragma once

#include "Aetherion/Scene/System.h"
#include <vector>

namespace Aetherion::Scene
{
class Scene;
class Entity;

class AIBehaviorSystem : public System
{
public:
    AIBehaviorSystem() = default;
    ~AIBehaviorSystem() override = default;

    [[nodiscard]] std::string GetName() const override { return "AIBehaviorSystem"; }

    void Configure(Runtime::EngineContext& context) override;
    void Update(Scene& scene, float deltaTime) override;

private:
    void ProcessEntity(Scene& scene, Entity* entity, float deltaTime);
};
} // namespace Aetherion::Scene
