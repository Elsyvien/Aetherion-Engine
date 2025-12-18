#include "Aetherion/Runtime/EngineApplication.h"

#include <stdexcept>

#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

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

    // Minimal bootstrap scene so the editor has something real to select/inspect.
    m_activeScene = std::make_shared<Scene::Scene>("Main Scene");
    m_activeScene->BindContext(*m_context);

    auto viewportEntity = std::make_shared<Scene::Entity>(1, "Viewport Quad");
    viewportEntity->AddComponent(std::make_shared<Scene::TransformComponent>());
    m_activeScene->AddEntity(viewportEntity);

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

std::shared_ptr<Scene::Scene> EngineApplication::GetActiveScene() const noexcept
{
    return m_activeScene;
}

void EngineApplication::RegisterPlaceholderSystems()
{
    // TODO: Register systems with the engine once rendering/physics/audio exist.
}
} // namespace Aetherion::Runtime
