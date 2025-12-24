#pragma once

#include <chrono>
#include <string>
#include <memory>
#include <vector>

#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Runtime/RuntimeSystem.h"

namespace Aetherion::Scene
{
class Scene;
} // namespace Aetherion::Scene

namespace Aetherion::Runtime
{
class EngineApplication
{
public:
    EngineApplication();
    ~EngineApplication();

    EngineApplication(const EngineApplication&) = delete;
    EngineApplication& operator=(const EngineApplication&) = delete;

    void Initialize(bool enableValidationLayers, bool enableVerboseLogging);
    void Shutdown();

    void Run();
    void Tick();
    void RequestShutdown() noexcept { m_running = false; }

    void RegisterSystem(std::shared_ptr<IRuntimeSystem> system);

    [[nodiscard]] std::shared_ptr<EngineContext> GetContext() const noexcept;

    [[nodiscard]] std::shared_ptr<Scene::Scene> GetActiveScene() const noexcept;
    void SetActiveScene(std::shared_ptr<Scene::Scene> scene);
    [[nodiscard]] bool IsValidationEnabled() const noexcept { return m_enableValidationLayers; }
    [[nodiscard]] bool IsVerboseLoggingEnabled() const noexcept { return m_enableVerboseLogging; }

    // TODO: Add scene management and runtime loop orchestration.
private:
    std::shared_ptr<EngineContext> m_context;
    std::shared_ptr<Scene::Scene> m_activeScene;
    std::vector<std::shared_ptr<IRuntimeSystem>> m_runtimeSystems;
    std::chrono::steady_clock::time_point m_lastFrameTime{};
    bool m_running{false};
    bool m_enableValidationLayers{true};
    bool m_enableVerboseLogging{true};
    bool m_initialized{false};
    bool m_sceneSystemsConfigured{false};

    void DebugPrint(const std::string& message, bool isError = false) const;
    void RegisterPlaceholderSystems();
    void UpdateRuntimeSystems(float deltaTime);
    void UpdateSceneSystems(float deltaTime);
    void ProcessInput();
    void PumpEvents();
};
} // namespace Aetherion::Runtime
