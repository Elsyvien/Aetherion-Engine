#include "Aetherion/Runtime/EngineApplication.h"

#include <filesystem>
#include <stdexcept>
#include <thread>

#include "Aetherion/Audio/AudioPlaceholder.h"
#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Physics/PhysicsPlaceholder.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/SceneSerializer.h"
#include "Aetherion/Scene/System.h"
#include "Aetherion/Scripting/ScriptingPlaceholder.h"

namespace Aetherion::Runtime
{
namespace
{
class SceneSystemDispatcher final : public IRuntimeSystem
{
public:
    explicit SceneSystemDispatcher(std::weak_ptr<Scene::Scene> scene)
        : m_scene(std::move(scene))
    {
    }

    [[nodiscard]] std::string GetName() const override { return "SceneSystemDispatcher"; }

    void Initialize(EngineContext& context) override
    {
        m_context = &context;
        ConfigureSceneSystems();
    }

    void Tick(EngineContext& context, float deltaTime) override
    {
        m_context = &context;
        ConfigureSceneSystems();

        if (auto scene = m_scene.lock())
        {
            for (const auto& system : scene->GetSystems())
            {
                if (system)
                {
                    system->Update(*scene, deltaTime);
                }
            }
        }
    }

    void Shutdown(EngineContext& context) override
    {
        (void)context;
        m_context = nullptr;
        m_scene.reset();
    }

private:
    void ConfigureSceneSystems()
    {
        if (m_sceneConfigured || !m_context)
        {
            return;
        }

        if (auto scene = m_scene.lock())
        {
            for (const auto& system : scene->GetSystems())
            {
                if (system)
                {
                    system->Configure(*m_context);
                }
            }
        }
        m_sceneConfigured = true;
    }

    EngineContext* m_context{nullptr};
    bool m_sceneConfigured{false};
    std::weak_ptr<Scene::Scene> m_scene;
};
} // namespace

EngineApplication::EngineApplication()
    : m_context(std::make_shared<EngineContext>())
{
    // TODO: Load project metadata and configure context.
}

EngineApplication::~EngineApplication() = default;

void EngineApplication::Initialize(bool enableValidationLayers, bool enableVerboseLogging)
{
    if (m_initialized)
    {
        Shutdown();
    }

    if (!m_context)
    {
        m_context = std::make_shared<EngineContext>();
    }

    m_enableValidationLayers = enableValidationLayers;
    m_enableVerboseLogging = enableVerboseLogging;

    auto vulkanContext = std::make_shared<Rendering::VulkanContext>();
    try
    {
        vulkanContext->Initialize(m_enableValidationLayers, m_enableVerboseLogging);
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("Failed to initialize Vulkan: ") + ex.what());
    }

    m_context->SetVulkanContext(vulkanContext);
    m_context->SetRenderView(std::make_shared<Rendering::RenderView>());
    m_context->SetAssetRegistry(std::make_shared<Assets::AssetRegistry>());
    m_context->SetPhysicsSystem(std::make_shared<Physics::PhysicsWorldStub>());
    m_context->SetAudioSystem(std::make_shared<Audio::AudioEngineStub>());
    m_context->SetScriptingRuntime(std::make_shared<Scripting::ScriptingRuntimeStub>());
    m_context->SetProjectName("Aetherion");

    if (const auto assets = m_context->GetAssetRegistry())
    {
        assets->Scan("assets");
    }
    if (const auto physics = m_context->GetPhysicsSystem())
    {
        physics->Initialize();
    }
    if (const auto audio = m_context->GetAudioSystem())
    {
        audio->Initialize();
    }
    if (const auto scripting = m_context->GetScriptingRuntime())
    {
        scripting->Initialize();
    }

    Scene::SceneSerializer serializer(*m_context);
    const auto scenePath = std::filesystem::path("assets") / "scenes" / "bootstrap_scene.json";
    m_activeScene = serializer.Load(scenePath);
    if (!m_activeScene)
    {
        m_activeScene = serializer.CreateDefaultScene();
        serializer.Save(*m_activeScene, scenePath);
    }

    m_sceneSystemsConfigured = false;
    RegisterPlaceholderSystems();

    m_running = true;
    m_lastFrameTime = std::chrono::steady_clock::now();
    m_initialized = true;
}

void EngineApplication::Shutdown()
{
    m_running = false;

    if (auto ctx = m_context ? m_context->GetVulkanContext() : nullptr)
    {
        ctx->Shutdown();
        m_context->SetVulkanContext(nullptr);
    }

    for (const auto& system : m_runtimeSystems)
    {
        if (system && m_context)
        {
            system->Shutdown(*m_context);
        }
    }
    m_runtimeSystems.clear();

    if (m_context)
    {
        if (const auto scripting = m_context->GetScriptingRuntime())
        {
            scripting->Shutdown();
        }
        if (const auto audio = m_context->GetAudioSystem())
        {
            audio->Shutdown();
        }
        if (const auto physics = m_context->GetPhysicsSystem())
        {
            physics->Shutdown();
        }
        m_context->SetAssetRegistry(nullptr);
        m_context->SetPhysicsSystem(nullptr);
        m_context->SetAudioSystem(nullptr);
        m_context->SetScriptingRuntime(nullptr);
        m_context->SetRenderView(nullptr);
    }

    m_activeScene.reset();
    m_context.reset();
    m_lastFrameTime = {};
    m_sceneSystemsConfigured = false;
    m_initialized = false;
}

void EngineApplication::Run()
{
    while (m_running)
    {
        Tick();
        std::this_thread::yield();
    }
}

void EngineApplication::Tick()
{
    if (!m_running || !m_context)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;

    ProcessInput();
    PumpEvents();

    UpdateRuntimeSystems(deltaTime);
    if (m_runtimeSystems.empty())
    {
        UpdateSceneSystems(deltaTime);
    }
}

void EngineApplication::RegisterSystem(std::shared_ptr<IRuntimeSystem> system)
{
    if (!system)
    {
        return;
    }

    m_runtimeSystems.push_back(std::move(system));
    if (m_context)
    {
        m_runtimeSystems.back()->Initialize(*m_context);
    }
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
    RegisterSystem(std::make_shared<SceneSystemDispatcher>(m_activeScene));
    // TODO: Register systems with the engine once rendering/physics/audio exist.
}

void EngineApplication::UpdateRuntimeSystems(float deltaTime)
{
    for (const auto& system : m_runtimeSystems)
    {
        if (system)
        {
            system->Tick(*m_context, deltaTime);
        }
    }
}

void EngineApplication::UpdateSceneSystems(float deltaTime)
{
    if (!m_activeScene)
    {
        return;
    }

    if (!m_sceneSystemsConfigured && m_context)
    {
        for (const auto& system : m_activeScene->GetSystems())
        {
            if (system)
            {
                system->Configure(*m_context);
            }
        }
        m_sceneSystemsConfigured = true;
    }

    for (const auto& system : m_activeScene->GetSystems())
    {
        if (system)
        {
            system->Update(*m_activeScene, deltaTime);
        }
    }
}

void EngineApplication::ProcessInput()
{
    // TODO: Bridge platform input, window events, and editor commands.
}

void EngineApplication::PumpEvents()
{
    // TODO: Connect to platform message pump for windowing/input.
}
} // namespace Aetherion::Runtime
