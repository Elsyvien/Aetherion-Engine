#pragma once

#include <memory>
#include <string>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Rendering
{
class VulkanContext;
}

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

    void SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context);
    [[nodiscard]] std::shared_ptr<Rendering::VulkanContext> GetVulkanContext() const noexcept;

    // TODO: Expose service locators (rendering, physics, audio, scripting, assets).
    // TODO: Provide lifetime management rules for subsystems.
private:
    std::string m_projectName;
    std::shared_ptr<Rendering::VulkanContext> m_vulkanContext;
    // TODO: Add registries and service references once implemented.
};
} // namespace Aetherion::Runtime
