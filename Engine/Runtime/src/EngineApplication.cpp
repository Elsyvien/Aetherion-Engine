#include "Aetherion/Runtime/EngineApplication.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __linux__
#include <unistd.h>
#endif

#include "Aetherion/Audio/AudioPlaceholder.h"
#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Physics/PhysicsPlaceholder.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/SceneSerializer.h"
#include "Aetherion/Scene/System.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Scripting/ScriptingPlaceholder.h"

namespace Aetherion::Runtime
{
namespace
{
std::filesystem::path FindAssetsRoot(std::filesystem::path start)
{
    if (start.empty())
    {
        return {};
    }

    std::error_code ec;
    auto probe = std::filesystem::absolute(start, ec);
    if (ec)
    {
        probe = std::move(start);
    }

    for (int i = 0; i < 8; ++i)
    {
        std::filesystem::path candidate = probe / "assets";
        if (std::filesystem::exists(candidate, ec))
        {
            return candidate;
        }
        if (!probe.has_parent_path())
        {
            break;
        }
        probe = probe.parent_path();
    }

    return {};
}

std::filesystem::path GetExecutableDir()
{
#ifdef _WIN32
    std::wstring buffer;
    buffer.resize(MAX_PATH);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0)
    {
        return {};
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0)
    {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
    std::array<char, 4096> buffer{};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0)
    {
        return {};
    }
    buffer[static_cast<size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path();
#else
    return {};
#endif
}

std::filesystem::path ResolveAssetsRoot()
{
    if (const char* env = std::getenv("AETHERION_ASSETS_DIR"))
    {
        std::filesystem::path envPath(env);
        std::error_code ec;
        if (!envPath.empty() && std::filesystem::exists(envPath, ec))
        {
            return std::filesystem::absolute(envPath, ec);
        }
    }

    std::error_code ec;
    auto fromCwd = FindAssetsRoot(std::filesystem::current_path(ec));
    if (!fromCwd.empty())
    {
        return fromCwd;
    }

    auto fromExe = FindAssetsRoot(GetExecutableDir());
    if (!fromExe.empty())
    {
        return fromExe;
    }

    return std::filesystem::path("assets");
}

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

class RenderViewSystem final : public IRuntimeSystem
{
public:
    explicit RenderViewSystem(std::weak_ptr<Scene::Scene> scene)
        : m_scene(std::move(scene))
    {
    }

    [[nodiscard]] std::string GetName() const override { return "RenderViewSystem"; }

    void Initialize(EngineContext& context) override
    {
        m_context = &context;
        EnsureRenderView();
        RebuildRenderView();
    }

    void Tick(EngineContext& context, float) override
    {
        m_context = &context;
        RebuildRenderView();
    }

    void Shutdown(EngineContext& context) override
    {
        (void)context;
        m_context = nullptr;
        m_scene.reset();
    }

private:
    void EnsureRenderView()
    {
        if (!m_context)
        {
            return;
        }

        if (!m_context->GetRenderView())
        {
            m_context->SetRenderView(std::make_shared<Rendering::RenderView>());
        }
    }

    void RebuildRenderView()
    {
        if (!m_context)
        {
            return;
        }

        auto view = m_context->GetRenderView();
        if (!view)
        {
            view = std::make_shared<Rendering::RenderView>();
            m_context->SetRenderView(view);
        }

        view->instances.clear();
        view->batches.clear();
        view->transforms.clear();
        view->meshes.clear();

        auto scene = m_scene.lock();
        if (!scene)
        {
            return;
        }

        std::unordered_map<const Scene::MeshRendererComponent*, size_t> batchLookup;
        const auto& entities = scene->GetEntities();
        view->instances.reserve(entities.size());

        for (const auto& entity : entities)
        {
            if (!entity)
            {
                continue;
            }

            auto transform = entity->GetComponent<Scene::TransformComponent>();
            if (transform)
            {
                view->transforms.emplace(entity->GetId(), transform.get());
            }

            auto mesh = entity->GetComponent<Scene::MeshRendererComponent>();
            if (mesh)
            {
                view->meshes.emplace(entity->GetId(), mesh.get());
            }

            if (!transform || !mesh || !mesh->IsVisible())
            {
                continue;
            }

            Rendering::RenderInstance instance{};
            instance.entityId = entity->GetId();
            instance.transform = transform.get();
            instance.mesh = mesh.get();
            instance.meshAssetId = mesh->GetMeshAssetId();
            instance.albedoTextureId = mesh->GetAlbedoTextureId();
            instance.hasModel = false;
            view->instances.push_back(instance);

            size_t batchIndex = 0;
            auto found = batchLookup.find(instance.mesh);
            if (found == batchLookup.end())
            {
                batchIndex = view->batches.size();
                batchLookup.emplace(instance.mesh, batchIndex);
                view->batches.emplace_back();
            }
            else
            {
                batchIndex = found->second;
            }

            view->batches[batchIndex].instances.push_back(instance);
        }
    }

    EngineContext* m_context{nullptr};
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

    const std::filesystem::path assetsRoot = ResolveAssetsRoot();
    if (const auto assets = m_context->GetAssetRegistry())
    {
        assets->Scan(assetsRoot.string());
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
    const auto scenePath = assetsRoot / "scenes" / "bootstrap_scene.json";
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

void EngineApplication::SetActiveScene(std::shared_ptr<Scene::Scene> scene)
{
    m_activeScene = std::move(scene);
    m_sceneSystemsConfigured = false;

    if (m_activeScene && m_context)
    {
        m_activeScene->BindContext(*m_context);
    }

    if (!m_context)
    {
        return;
    }

    for (const auto& system : m_runtimeSystems)
    {
        if (system)
        {
            system->Shutdown(*m_context);
        }
    }
    m_runtimeSystems.clear();
    RegisterPlaceholderSystems();
}

void EngineApplication::RegisterPlaceholderSystems()
{
    RegisterSystem(std::make_shared<SceneSystemDispatcher>(m_activeScene));
    RegisterSystem(std::make_shared<RenderViewSystem>(m_activeScene));
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
