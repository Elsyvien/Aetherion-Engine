#pragma once

#include <string>

namespace Aetherion::Runtime
{
class EngineContext;

class IRuntimeSystem
{
public:
    virtual ~IRuntimeSystem() = default;

    IRuntimeSystem(const IRuntimeSystem&) = delete;
    IRuntimeSystem& operator=(const IRuntimeSystem&) = delete;

    [[nodiscard]] virtual std::string GetName() const = 0;
    virtual void Initialize(EngineContext& context) = 0;
    virtual void Tick(EngineContext& context, float deltaTime) = 0;
    virtual void Shutdown(EngineContext& context) = 0;
};
} // namespace Aetherion::Runtime
