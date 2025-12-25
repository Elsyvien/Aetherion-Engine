#include "Aetherion/Runtime/EngineApplication.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>

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
#include "Aetherion/Scene/CameraComponent.h"
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

void Mat4Identity(float out[16])
{
    std::memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void Mat4Mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    for (int c = 0; c < 4; ++c)
    {
        for (int rIdx = 0; rIdx < 4; ++rIdx)
        {
            r[c * 4 + rIdx] = a[0 * 4 + rIdx] * b[c * 4 + 0] + a[1 * 4 + rIdx] *
                              b[c * 4 + 1] + a[2 * 4 + rIdx] * b[c * 4 + 2] +
                              a[3 * 4 + rIdx] * b[c * 4 + 3];
        }
    }
    std::memcpy(out, r, sizeof(r));
}

void Mat4RotationX(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[5] = c;
    out[9] = -s;
    out[6] = s;
    out[10] = c;
}

void Mat4RotationY(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[8] = s;
    out[2] = -s;
    out[10] = c;
}

void Mat4RotationZ(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[4] = -s;
    out[1] = s;
    out[5] = c;
}

void Mat4Translation(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void Mat4Scale(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}

std::array<float, 16> BuildLocalMatrix(const Scene::TransformComponent& transform)
{
    float t[16];
    float rx[16];
    float ry[16];
    float rz[16];
    float rzy[16];
    float r[16];
    float s[16];
    float tr[16];
    float local[16];
    Mat4Translation(t, transform.GetPositionX(), transform.GetPositionY(), transform.GetPositionZ());
    Mat4RotationX(rx, transform.GetRotationXDegrees() * (3.14159265358979323846f / 180.0f));
    Mat4RotationY(ry, transform.GetRotationYDegrees() * (3.14159265358979323846f / 180.0f));
    Mat4RotationZ(rz, transform.GetRotationZDegrees() * (3.14159265358979323846f / 180.0f));
    Mat4Mul(rzy, rz, ry);
    Mat4Mul(r, rzy, rx);
    Mat4Scale(s, transform.GetScaleX(), transform.GetScaleY(), transform.GetScaleZ());
    Mat4Mul(tr, t, r);
    Mat4Mul(local, tr, s);
    std::array<float, 16> out{};
    std::memcpy(out.data(), local, sizeof(local));
    return out;
}

std::array<float, 16> GetWorldMatrix(const Scene::Scene& scene, Core::EntityId id)
{
    auto entity = scene.FindEntityById(id);
    if (!entity)
    {
        std::array<float, 16> identity{};
        Mat4Identity(identity.data());
        return identity;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        std::array<float, 16> identity{};
        Mat4Identity(identity.data());
        return identity;
    }

    auto local = BuildLocalMatrix(*transform);
    if (!transform->HasParent())
    {
        return local;
    }

    auto parent = GetWorldMatrix(scene, transform->GetParentId());
    float world[16];
    Mat4Mul(world, parent.data(), local.data());
    std::array<float, 16> out{};
    std::memcpy(out.data(), world, sizeof(world));
    return out;
}

void Vec3Normalize(float v[3])
{
    const float lenSq = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (lenSq <= 0.0f)
    {
        return;
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    v[0] *= invLen;
    v[1] *= invLen;
    v[2] *= invLen;
}

bool ContainsTokenCaseInsensitive(const std::string& value, const char* token)
{
    if (value.empty() || token == nullptr || token[0] == '\0')
    {
        return false;
    }
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string loweredToken(token);
    std::transform(loweredToken.begin(), loweredToken.end(), loweredToken.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered.find(loweredToken) != std::string::npos;
}

bool IsMovingLightName(const std::string& name)
{
    return ContainsTokenCaseInsensitive(name, "moving") ||
           ContainsTokenCaseInsensitive(name, "orbit") ||
           ContainsTokenCaseInsensitive(name, "bob");
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

    void Tick(EngineContext& context, float deltaTime) override
    {
        m_context = &context;
        m_timeSeconds += std::max(0.0f, deltaTime);
        if (m_timeSeconds > 10000.0f)
        {
            m_timeSeconds = std::fmod(m_timeSeconds, 10000.0f);
        }
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

        auto registry = m_context ? m_context->GetAssetRegistry() : nullptr;

        view->instances.clear();
        view->batches.clear();
        view->transforms.clear();
        view->meshes.clear();
        view->directionalLight = Rendering::RenderDirectionalLight{};
        view->lights.clear();
        view->camera = Rendering::RenderCamera{};
        view->cameras.clear();

        auto scene = m_scene.lock();
        if (!scene)
        {
            return;
        }

        std::unordered_map<const Scene::MeshRendererComponent*, size_t> batchLookup;
        const auto& entities = scene->GetEntities();
        view->instances.reserve(entities.size());

        bool foundDirectional = false;
        bool foundPrimaryDirectional = false;
        bool foundCamera = false;
        bool foundPrimaryCamera = false;
        std::unordered_set<Core::EntityId> movingLightIds;
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

            bool hasWorld = false;
            std::array<float, 16> world{};

            auto light = entity->GetComponent<Scene::LightComponent>();
            if (light && transform)
            {
                if (!hasWorld)
                {
                    world = GetWorldMatrix(*scene, entity->GetId());
                    hasWorld = true;
                }

                Rendering::RenderLight renderLight{};
                renderLight.entityId = entity->GetId();
                renderLight.enabled = light->IsEnabled();
                const auto lightColor = light->GetColor();
                renderLight.color[0] = lightColor[0];
                renderLight.color[1] = lightColor[1];
                renderLight.color[2] = lightColor[2];
                renderLight.intensity = light->GetIntensity();
                renderLight.range = light->GetRange();
                renderLight.innerConeAngle = light->GetInnerConeAngle();
                renderLight.outerConeAngle = light->GetOuterConeAngle();
                renderLight.isPrimary = light->IsPrimary();
                renderLight.position[0] = world[12];
                renderLight.position[1] = world[13];
                renderLight.position[2] = world[14];

                float dir[3] = {-world[8], -world[9], -world[10]};
                Vec3Normalize(dir);
                renderLight.direction[0] = dir[0];
                renderLight.direction[1] = dir[1];
                renderLight.direction[2] = dir[2];

                switch (light->GetType())
                {
                case Scene::LightComponent::LightType::Point:
                    renderLight.type = Rendering::RenderLightType::Point;
                    break;
                case Scene::LightComponent::LightType::Spot:
                    renderLight.type = Rendering::RenderLightType::Spot;
                    break;
                default:
                    renderLight.type = Rendering::RenderLightType::Directional;
                    break;
                }

                const bool moving =
                    (renderLight.type != Rendering::RenderLightType::Directional) &&
                    IsMovingLightName(entity->GetName());
                if (moving)
                {
                    movingLightIds.insert(entity->GetId());
                    auto baseIt = m_movingLightBases.find(entity->GetId());
                    if (baseIt == m_movingLightBases.end())
                    {
                        std::array<float, 3> base = {renderLight.position[0],
                                                     renderLight.position[1],
                                                     renderLight.position[2]};
                        baseIt = m_movingLightBases.emplace(entity->GetId(), base).first;
                    }

                    const float speed =
                        0.6f + 0.15f * static_cast<float>(entity->GetId() % 7);
                    const float radius =
                        0.7f + 0.2f * static_cast<float>(entity->GetId() % 5);
                    const float height =
                        0.25f + 0.1f * static_cast<float>(entity->GetId() % 3);
                    const float angle = m_timeSeconds * speed;

                    renderLight.position[0] =
                        baseIt->second[0] + std::cos(angle) * radius;
                    renderLight.position[2] =
                        baseIt->second[2] + std::sin(angle) * radius;
                    renderLight.position[1] =
                        baseIt->second[1] + std::sin(angle * 1.7f) * height;
                }

                view->lights.push_back(renderLight);

                if (renderLight.type == Rendering::RenderLightType::Directional && renderLight.enabled)
                {
                    const bool choose =
                        !foundDirectional || (renderLight.isPrimary && !foundPrimaryDirectional);
                    if (choose)
                    {
                        view->directionalLight.enabled = true;
                        view->directionalLight.direction[0] = renderLight.direction[0];
                        view->directionalLight.direction[1] = renderLight.direction[1];
                        view->directionalLight.direction[2] = renderLight.direction[2];
                        view->directionalLight.position[0] = renderLight.position[0];
                        view->directionalLight.position[1] = renderLight.position[1];
                        view->directionalLight.position[2] = renderLight.position[2];
                        view->directionalLight.entityId = renderLight.entityId;
                        view->directionalLight.color[0] = renderLight.color[0];
                        view->directionalLight.color[1] = renderLight.color[1];
                        view->directionalLight.color[2] = renderLight.color[2];
                        view->directionalLight.intensity = renderLight.intensity;

                        const auto ambient = light->GetAmbientColor();
                        view->directionalLight.ambientColor[0] = ambient[0];
                        view->directionalLight.ambientColor[1] = ambient[1];
                        view->directionalLight.ambientColor[2] = ambient[2];
                        foundDirectional = true;
                        if (renderLight.isPrimary)
                        {
                            foundPrimaryDirectional = true;
                        }
                    }
                }
            }

            auto camera = entity->GetComponent<Scene::CameraComponent>();       
            if (camera && transform)
            {
                if (!hasWorld)
                {
                    world = GetWorldMatrix(*scene, entity->GetId());
                    hasWorld = true;
                }

                Rendering::RenderCamera candidate{};
                candidate.enabled = true;
                candidate.position[0] = world[12];
                candidate.position[1] = world[13];
                candidate.position[2] = world[14];
                candidate.forward[0] = -world[8];
                candidate.forward[1] = -world[9];
                candidate.forward[2] = -world[10];
                Vec3Normalize(candidate.forward);
                candidate.up[0] = world[4];
                candidate.up[1] = world[5];
                candidate.up[2] = world[6];
                Vec3Normalize(candidate.up);

                float fov = camera->GetVerticalFov();
                if (fov < 1.0f) fov = 1.0f;
                if (fov > 179.0f) fov = 179.0f;
                candidate.verticalFov = fov;

                float nearClip = std::max(0.001f, camera->GetNearClip());
                float farClip = std::max(nearClip + 0.001f, camera->GetFarClip());
                candidate.nearClip = nearClip;
                candidate.farClip = farClip;
                candidate.orthographicSize = std::max(0.01f, camera->GetOrthographicSize());
                candidate.projectionType = static_cast<uint32_t>(camera->GetProjectionType());
                candidate.entityId = entity->GetId();
                view->cameras.push_back(candidate);

                const bool isPrimary = camera->IsPrimary();
                if (!foundCamera || (isPrimary && !foundPrimaryCamera))
                {
                    view->camera = candidate;
                    foundCamera = true;
                    if (isPrimary)
                    {
                        foundPrimaryCamera = true;
                    }
                }
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
            if (registry && !instance.meshAssetId.empty())
            {
                if (const auto* entry = registry->FindEntry(instance.meshAssetId))
                {
                    instance.meshAssetId = entry->id;
                }
            }

            instance.albedoTextureId = mesh->GetAlbedoTextureId();
            if (registry && !instance.albedoTextureId.empty())
            {
                if (const auto* entry = registry->FindEntry(instance.albedoTextureId))
                {
                    instance.albedoTextureId = entry->id;
                }
            }
            if (instance.albedoTextureId.empty() && registry && !instance.meshAssetId.empty())
            {
                if (const auto* cachedMesh = registry->GetMesh(instance.meshAssetId))
                {
                    for (const auto& materialId : cachedMesh->materialIds)
                    {
                        if (const auto* material = registry->GetMaterial(materialId);
                            material && !material->albedoTextureId.empty())
                        {
                            instance.albedoTextureId = material->albedoTextureId;
                            break;
                        }
                    }
                    if (instance.albedoTextureId.empty() && !cachedMesh->textureIds.empty())
                    {
                        instance.albedoTextureId = cachedMesh->textureIds.front();
                    }
                }
            }
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

        if (movingLightIds.empty())
        {
            m_movingLightBases.clear();
        }
        else
        {
            for (auto it = m_movingLightBases.begin(); it != m_movingLightBases.end();)
            {
                if (movingLightIds.find(it->first) == movingLightIds.end())
                {
                    it = m_movingLightBases.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    EngineContext* m_context{nullptr};
    std::weak_ptr<Scene::Scene> m_scene;
    float m_timeSeconds{0.0f};
    std::unordered_map<Core::EntityId, std::array<float, 3>> m_movingLightBases;
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
