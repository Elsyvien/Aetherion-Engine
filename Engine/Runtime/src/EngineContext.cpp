#include "Aetherion/Runtime/EngineContext.h"

#include "Aetherion/Rendering/VulkanContext.h"

#include <utility>

namespace Aetherion::Runtime
{
EngineContext::EngineContext() = default;
EngineContext::~EngineContext() = default;

void EngineContext::SetProjectName(std::string name)
{
    m_projectName = std::move(name);
    // TODO: Notify interested systems about context changes.
}

const std::string& EngineContext::GetProjectName() const noexcept
{
    return m_projectName;
}

void EngineContext::SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context)
{
    m_vulkanContext = std::move(context);
}

std::shared_ptr<Rendering::VulkanContext> EngineContext::GetVulkanContext() const noexcept
{
    return m_vulkanContext;
}
} // namespace Aetherion::Runtime
