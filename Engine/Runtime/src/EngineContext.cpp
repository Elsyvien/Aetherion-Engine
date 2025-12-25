#include "Aetherion/Runtime/EngineContext.h"

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Audio/AudioPlaceholder.h"
#include "Aetherion/Physics/PhysicsWorld.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Scripting/ScriptingPlaceholder.h"
#include <utility>

namespace Aetherion::Runtime {
EngineContext::EngineContext() = default;
EngineContext::~EngineContext() = default;

void EngineContext::SetProjectName(std::string name) {
  m_projectName = std::move(name);
  // TODO: Notify interested systems about context changes.
}

const std::string &EngineContext::GetProjectName() const noexcept {
  return m_projectName;
}

void EngineContext::SetVulkanContext(
    std::shared_ptr<Rendering::VulkanContext> context) {
  m_vulkanContext = std::move(context);
}

std::shared_ptr<Rendering::VulkanContext>
EngineContext::GetVulkanContext() const noexcept {
  return m_vulkanContext;
}

void EngineContext::SetRenderView(std::shared_ptr<Rendering::RenderView> view) {
  m_renderView = std::move(view);
}

std::shared_ptr<Rendering::RenderView>
EngineContext::GetRenderView() const noexcept {
  return m_renderView;
}

void EngineContext::SetAssetRegistry(
    std::shared_ptr<Assets::AssetRegistry> registry) {
  m_assetRegistry = std::move(registry);
}

std::shared_ptr<Assets::AssetRegistry>
EngineContext::GetAssetRegistry() const noexcept {
  return m_assetRegistry;
}

void EngineContext::SetPhysicsSystem(
    std::shared_ptr<Physics::PhysicsWorld> physics) {
  m_physicsSystem = std::move(physics);
}

std::shared_ptr<Physics::PhysicsWorld>
EngineContext::GetPhysicsSystem() const noexcept {
  return m_physicsSystem;
}

void EngineContext::SetAudioSystem(
    std::shared_ptr<Audio::AudioEngineStub> audio) {
  m_audioSystem = std::move(audio);
}

std::shared_ptr<Audio::AudioEngineStub>
EngineContext::GetAudioSystem() const noexcept {
  return m_audioSystem;
}

void EngineContext::SetScriptingRuntime(
    std::shared_ptr<Scripting::ScriptingRuntimeStub> scripting) {
  m_scriptingRuntime = std::move(scripting);
}

std::shared_ptr<Scripting::ScriptingRuntimeStub>
EngineContext::GetScriptingRuntime() const noexcept {
  return m_scriptingRuntime;
}

void EngineContext::SetSimulationState(bool playing, bool paused) noexcept {
  m_simulationPlaying = playing;
  m_simulationPaused = paused;
}
} // namespace Aetherion::Runtime
