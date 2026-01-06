#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "Aetherion/Assets/LLMClient.h"
#include "Aetherion/Core/EventBus.h"
#include "Aetherion/Core/Types.h"

namespace Aetherion::Rendering {
class RenderView;
class VulkanContext;
} // namespace Aetherion::Rendering

namespace Aetherion::Assets {
class AssetRegistry;
}

namespace Aetherion::Physics {
class PhysicsWorld;
}

namespace Aetherion::Audio {
class AudioEngine;
}

namespace Aetherion::Scripting {
class ScriptingRuntime;
class ScriptEngine;
}

namespace Aetherion::Runtime {
class EngineContext {
public:
  EngineContext();
  ~EngineContext();

  EngineContext(const EngineContext &) = delete;
  EngineContext &operator=(const EngineContext &) = delete;

  void SetProjectName(std::string name);
  [[nodiscard]] const std::string &GetProjectName() const noexcept;

  void SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context);
  [[nodiscard]] std::shared_ptr<Rendering::VulkanContext>
  GetVulkanContext() const noexcept;

  void SetRenderView(std::shared_ptr<Rendering::RenderView> view);
  [[nodiscard]] std::shared_ptr<Rendering::RenderView>
  GetRenderView() const noexcept;

  void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);
  [[nodiscard]] std::shared_ptr<Assets::AssetRegistry>
  GetAssetRegistry() const noexcept;

  void SetPhysicsSystem(std::shared_ptr<Physics::PhysicsWorld> physics);
  [[nodiscard]] std::shared_ptr<Physics::PhysicsWorld>
  GetPhysicsSystem() const noexcept;

  void SetAudioSystem(std::shared_ptr<Audio::AudioEngine> audio);
  [[nodiscard]] std::shared_ptr<Audio::AudioEngine>
  GetAudioSystem() const noexcept;

  void SetScriptingRuntime(
      std::shared_ptr<Scripting::ScriptingRuntime> scripting);
  [[nodiscard]] std::shared_ptr<Scripting::ScriptingRuntime>
  GetScriptingRuntime() const noexcept;

  void SetScriptEngine(std::shared_ptr<Scripting::ScriptEngine> engine);        
  [[nodiscard]] std::shared_ptr<Scripting::ScriptEngine>
  GetScriptEngine() const noexcept;

  void SetAIConfig(Assets::LLMConfig config, bool enabled = true);
  void ClearAIConfig() noexcept;
  [[nodiscard]] bool HasAIConfig() const noexcept { return m_aiConfigEnabled; }
  [[nodiscard]] const Assets::LLMConfig& GetAIConfig() const noexcept {
    return m_aiConfig;
  }
  Assets::ILLMClient* GetAIClient();

  /// @brief Get the global event bus for inter-system communication
  [[nodiscard]] Core::EventBus &GetEventBus() noexcept { return m_eventBus; }   
  [[nodiscard]] const Core::EventBus &GetEventBus() const noexcept {
    return m_eventBus;
  }

  // Simulation state (play/pause/step) shared with runtime systems
  void SetSimulationState(bool playing, bool paused) noexcept;
  [[nodiscard]] bool IsSimulationPlaying() const noexcept {
    return m_simulationPlaying;
  }
  [[nodiscard]] bool IsSimulationPaused() const noexcept {
    return m_simulationPaused;
  }
  [[nodiscard]] bool IsSimulationStepRequested() const noexcept {
    return m_stepOnceRequested;
  }
  void RequestSimulationStep() noexcept { m_stepOnceRequested = true; }
  [[nodiscard]] bool ConsumeSimulationStepRequest() noexcept {
    const bool requested = m_stepOnceRequested;
    m_stepOnceRequested = false;
    return requested;
  }
  void ClearSimulationStepRequest() noexcept { m_stepOnceRequested = false; }   
  [[nodiscard]] std::uint64_t GetFrameIndex() const noexcept {
    return m_frameIndex;
  }
  void AdvanceFrame() noexcept { ++m_frameIndex; }

  // EngineContext owns shared references to service singletons. Providers      
  // remain alive until replaced or cleared by Set* methods or during
  // EngineApplication::Shutdown().
private:
  std::string m_projectName;
  std::shared_ptr<Rendering::VulkanContext> m_vulkanContext;
  std::shared_ptr<Rendering::RenderView> m_renderView;
  std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
  std::shared_ptr<Physics::PhysicsWorld> m_physicsSystem;
  std::shared_ptr<Audio::AudioEngine> m_audioSystem;
  std::shared_ptr<Scripting::ScriptingRuntime> m_scriptingRuntime;
  std::shared_ptr<Scripting::ScriptEngine> m_scriptEngine;
  Assets::LLMConfig m_aiConfig{};
  bool m_aiConfigEnabled{false};
  std::unique_ptr<Assets::ILLMClient> m_aiClient;
  Core::EventBus m_eventBus;
  bool m_simulationPlaying{false};
  bool m_simulationPaused{false};
  bool m_stepOnceRequested{false};
  std::uint64_t m_frameIndex{0};
};
} // namespace Aetherion::Runtime
