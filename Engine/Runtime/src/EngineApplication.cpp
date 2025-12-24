#include "Aetherion/Runtime/EngineApplication.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
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
#include "Aetherion/Scene/LightComponent.h"
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

std::string BoolToOnOff(bool value)
{
    return value ? "on" : "off";
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
        view->directionalLight = Rendering::RenderDirectionalLight{};

        auto scene = m_scene.lock();
        if (!scene)
        {
            return;
        }

        std::unordered_map<const Scene::MeshRendererComponent*, size_t> batchLookup;
        const auto& entities = scene->GetEntities();
        view->instances.reserve(entities.size());

        bool foundLight = false;
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

            auto light = entity->GetComponent<Scene::LightComponent>();
            if (light && !foundLight && light->IsEnabled())
            {
                float yawDeg = 0.0f;
                float pitchDeg = -45.0f;
                float posX = 0.0f;
                float posY = 3.0f;
                float posZ = 0.0f;
                if (transform)
                {
                    yawDeg = transform->GetRotationYDegrees();
                    pitchDeg = transform->GetRotationXDegrees();
                    posX = transform->GetPositionX();
                    posY = transform->GetPositionY();
                    posZ = transform->GetPositionZ();
                }

                const float yawRad = yawDeg * (3.14159265358979323846f / 180.0f);
                const float pitchRad = pitchDeg * (3.14159265358979323846f / 180.0f);
                float dir[3] = {std::cos(pitchRad) * std::sin(yawRad),
                                std::sin(pitchRad),
                                std::cos(pitchRad) * std::cos(yawRad)};
                const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
                if (len > 0.0001f)
                {
                    dir[0] /= len;
                    dir[1] /= len;
                    dir[2] /= len;
                }

                view->directionalLight.enabled = true;
                view->directionalLight.direction[0] = dir[0];
                view->directionalLight.direction[1] = dir[1];
                view->directionalLight.direction[2] = dir[2];

                // Store position and entity ID for gizmo rendering
                view->directionalLight.position[0] = posX;
                view->directionalLight.position[1] = posY;
                view->directionalLight.position[2] = posZ;
                view->directionalLight.entityId = entity->GetId();

                const auto lightColor = light->GetColor();
                view->directionalLight.color[0] = lightColor[0];
                view->directionalLight.color[1] = lightColor[1];
                view->directionalLight.color[2] = lightColor[2];
                view->directionalLight.intensity = light->GetIntensity();

                const auto ambient = light->GetAmbientColor();
                view->directionalLight.ambientColor[0] = ambient[0];
                view->directionalLight.ambientColor[1] = ambient[1];
                view->directionalLight.ambientColor[2] = ambient[2];
                foundLight = true;
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
        DebugPrint("Engine already initialized. Restarting...");
        Shutdown();
    }

    if (!m_context)
    {
        m_context = std::make_shared<EngineContext>();
    }

    m_enableValidationLayers = enableValidationLayers;
    m_enableVerboseLogging = enableVerboseLogging;
    DebugPrint("Initializing engine (validation=" + BoolToOnOff(m_enableValidationLayers) +
                   ", verbose logging=" + BoolToOnOff(m_enableVerboseLogging) + ")");

    auto vulkanContext = std::make_shared<Rendering::VulkanContext>();
    try
    {
        vulkanContext->Initialize(m_enableValidationLayers, m_enableVerboseLogging);
    }
    catch (const std::exception& ex)
    {
        DebugPrint(std::string("Vulkan initialization failed: ") + ex.what(), true);
        throw std::runtime_error(std::string("Failed to initialize Vulkan: ") + ex.what());
    }
    DebugPrint("Vulkan context initialized.");

    m_context->SetVulkanContext(vulkanContext);
    m_context->SetRenderView(std::make_shared<Rendering::RenderView>());
    m_context->SetAssetRegistry(std::make_shared<Assets::AssetRegistry>());
    m_context->SetPhysicsSystem(std::make_shared<Physics::PhysicsWorldStub>());
    m_context->SetAudioSystem(std::make_shared<Audio::AudioEngineStub>());
    m_context->SetScriptingRuntime(std::make_shared<Scripting::ScriptingRuntimeStub>());
    m_context->SetProjectName("Aetherion");

    const std::filesystem::path assetsRoot = ResolveAssetsRoot();
    DebugPrint("Resolved assets root: " + assetsRoot.string());
    if (const auto assets = m_context->GetAssetRegistry())
    {
        assets->Scan(assetsRoot.string());
        DebugPrint("Asset scan complete: " + assets->GetRootPath().string() + " (" +
                       std::to_string(assets->GetEntries().size()) + " assets)");
    }
    if (const auto physics = m_context->GetPhysicsSystem())
    {
        physics->Initialize();
        DebugPrint("Physics placeholder initialized.");
    }
    if (const auto audio = m_context->GetAudioSystem())
    {
        audio->Initialize();
        DebugPrint("Audio placeholder initialized.");
    }
    if (const auto scripting = m_context->GetScriptingRuntime())
    {
        scripting->Initialize();
        DebugPrint("Scripting placeholder initialized.");
    }

    Scene::SceneSerializer serializer(*m_context);
    const auto scenePath = assetsRoot / "scenes" / "bootstrap_scene.json";
    DebugPrint("Loading bootstrap scene: " + scenePath.string());
    m_activeScene = serializer.Load(scenePath);
    if (!m_activeScene)
    {
        DebugPrint("Bootstrap scene missing. Creating default scene...");
        m_activeScene = serializer.CreateDefaultScene();
        serializer.Save(*m_activeScene, scenePath);
        DebugPrint("Default scene saved to: " + scenePath.string());
    }
    else
    {
        DebugPrint("Bootstrap scene loaded successfully.");
    }

    m_sceneSystemsConfigured = false;
    RegisterPlaceholderSystems();

    m_running = true;
    m_lastFrameTime = std::chrono::steady_clock::now();
    m_initialized = true;
    DebugPrint("Engine initialized. Entering main loop.");
}

void EngineApplication::Shutdown()
{
    DebugPrint("Shutting down engine...");
    m_running = false;

    if (auto ctx = m_context ? m_context->GetVulkanContext() : nullptr)
    {
        ctx->Shutdown();
        m_context->SetVulkanContext(nullptr);
        DebugPrint("Vulkan context shut down.");
    }

    for (const auto& system : m_runtimeSystems)
    {
        if (system && m_context)
        {
            system->Shutdown(*m_context);
        }
    }
    m_runtimeSystems.clear();
    DebugPrint("Runtime systems cleared.");

    if (m_context)
    {
        if (const auto scripting = m_context->GetScriptingRuntime())
        {
            scripting->Shutdown();
            DebugPrint("Scripting placeholder shut down.");
        }
        if (const auto audio = m_context->GetAudioSystem())
        {
            audio->Shutdown();
            DebugPrint("Audio placeholder shut down.");
        }
        if (const auto physics = m_context->GetPhysicsSystem())
        {
            physics->Shutdown();
            DebugPrint("Physics placeholder shut down.");
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

    static bool s_loggedFirstTick = false;
    if (!s_loggedFirstTick)
    {
        const size_t systemsCount = m_runtimeSystems.size();
        DebugPrint("Main loop started. Registered runtime systems: " + std::to_string(systemsCount) +
                       (m_activeScene ? " (scene bound)" : " (no active scene)"));
        s_loggedFirstTick = true;
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
    const std::string name = m_runtimeSystems.back() ? m_runtimeSystems.back()->GetName() : "UnknownSystem";
    DebugPrint("Registering runtime system: " + name);
    if (m_context)
    {
        m_runtimeSystems.back()->Initialize(*m_context);
        DebugPrint("Runtime system initialized: " + name);
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
    DebugPrint("Placeholder systems registered.");
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

void EngineApplication::DebugPrint(const std::string& message, bool isError) const
{
    auto& stream = isError ? std::cerr : std::cout;
    stream << "[Engine] " << message << std::endl;
}
} // namespace Aetherion::Runtime
