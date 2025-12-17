#pragma once

#include <memory>
#include <string>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Runtime
{
class EngineContext
{
public:
    EngineContext();
    ~EngineContext();

    EngineContext(const EngineContext&) = delete;
    EngineContext& operator=(const EngineContext&) = delete;

    void SetProjectName(std::string name);
    [[nodiscard]] const std::string& GetProjectName() const noexcept;

    // TODO: Expose service locators (rendering, physics, audio, scripting, assets).
    // TODO: Provide lifetime management rules for subsystems.
private:
    std::string m_projectName;
    // TODO: Add registries and service references once implemented.
};
} // namespace Aetherion::Runtime
