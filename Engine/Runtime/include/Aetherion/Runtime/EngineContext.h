#pragma once

#include <memory>
#include <string>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Rendering
{
class RenderView;
class VulkanContext;
}

namespace Aetherion::Assets
{
class AssetRegistry;
}

namespace Aetherion::Physics
{
class PhysicsWorldStub;
}

namespace Aetherion::Audio
{
class AudioEngineStub;
}

namespace Aetherion::Scripting
{
class ScriptingRuntimeStub;
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

    void SetRenderView(std::shared_ptr<Rendering::RenderView> view);
    [[nodiscard]] std::shared_ptr<Rendering::RenderView> GetRenderView() const noexcept;

    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);
    [[nodiscard]] std::shared_ptr<Assets::AssetRegistry> GetAssetRegistry() const noexcept;

    void SetPhysicsSystem(std::shared_ptr<Physics::PhysicsWorldStub> physics);
    [[nodiscard]] std::shared_ptr<Physics::PhysicsWorldStub> GetPhysicsSystem() const noexcept;

    void SetAudioSystem(std::shared_ptr<Audio::AudioEngineStub> audio);
    [[nodiscard]] std::shared_ptr<Audio::AudioEngineStub> GetAudioSystem() const noexcept;

    void SetScriptingRuntime(std::shared_ptr<Scripting::ScriptingRuntimeStub> scripting);
    [[nodiscard]] std::shared_ptr<Scripting::ScriptingRuntimeStub> GetScriptingRuntime() const noexcept;

    // EngineContext owns shared references to service singletons. Providers remain alive until
    // replaced or cleared by Set* methods or during EngineApplication::Shutdown().
private:
    std::string m_projectName;
    std::shared_ptr<Rendering::VulkanContext> m_vulkanContext;
    std::shared_ptr<Rendering::RenderView> m_renderView;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    std::shared_ptr<Physics::PhysicsWorldStub> m_physicsSystem;
    std::shared_ptr<Audio::AudioEngineStub> m_audioSystem;
    std::shared_ptr<Scripting::ScriptingRuntimeStub> m_scriptingRuntime;
};
} // namespace Aetherion::Runtime
