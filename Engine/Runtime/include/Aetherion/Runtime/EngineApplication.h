#pragma once

#include <memory>

#include "Aetherion/Runtime/EngineContext.h"

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

    void Initialize();
    void Shutdown();

    [[nodiscard]] std::shared_ptr<EngineContext> GetContext() const noexcept;

    [[nodiscard]] std::shared_ptr<Scene::Scene> GetActiveScene() const noexcept;

    // TODO: Add scene management and runtime loop orchestration.
private:
    std::shared_ptr<EngineContext> m_context;
    std::shared_ptr<Scene::Scene> m_activeScene;

    void RegisterPlaceholderSystems();
    // TODO: Register systems when subsystems become available.
};
} // namespace Aetherion::Runtime
