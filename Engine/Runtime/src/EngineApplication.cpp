#include "Aetherion/Runtime/EngineApplication.h"

#include <stdexcept>

#include "Aetherion/Rendering/VulkanContext.h"

namespace Aetherion::Runtime
{
EngineApplication::EngineApplication()
    : m_context(std::make_shared<EngineContext>())
{
    // TODO: Load project metadata and configure context.
}

EngineApplication::~EngineApplication() = default;

void EngineApplication::Initialize()
{
    auto vulkanContext = std::make_shared<Rendering::VulkanContext>();
    try
    {
        vulkanContext->Initialize(true);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("Failed to initialize Vulkan: ") + ex.what());
    }

    m_context->SetVulkanContext(vulkanContext);

    RegisterPlaceholderSystems();
    // TODO: Bootstrap runtime subsystems and load initial scenes.
}

void EngineApplication::Shutdown()
{
    if (auto ctx = m_context->GetVulkanContext())
    {
        ctx->Shutdown();
        m_context->SetVulkanContext(nullptr);
    }

    // TODO: Flush pending tasks, persist state, and tear down systems.
    m_context.reset();
}

std::shared_ptr<EngineContext> EngineApplication::GetContext() const noexcept
{
    return m_context;
}

void EngineApplication::RegisterPlaceholderSystems()
{
    // TODO: Register systems with the engine once rendering/physics/audio exist.
}
} // namespace Aetherion::Runtime
