#pragma once

#include <string>

namespace Aetherion::Runtime
{
class EngineContext;
} // namespace Aetherion::Runtime

namespace Aetherion::Scene
{
class Scene;

class System
{
public:
    System() = default;
    virtual ~System() = default;

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    [[nodiscard]] virtual std::string GetName() const = 0;
    virtual void Configure(Runtime::EngineContext& context) = 0;
    virtual void Update(Scene& scene, float deltaTime) = 0;

    // TODO: Define system lifecycle and ordering constraints.
};
} // namespace Aetherion::Scene
