#include "Aetherion/Runtime/EngineApplication.h"

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
    RegisterPlaceholderSystems();
    // TODO: Bootstrap runtime subsystems and load initial scenes.
}

void EngineApplication::Shutdown()
{
    // TODO: Flush pending tasks, persist state, and tear down systems.
    m_context.reset();
}

std::shared_ptr<EngineContext> EngineApplication::GetContext() const noexcept
{
    return m_context;
}

void EngineApplication::RegisterPlaceholderSystems()
{
    // TODO: Register systems with the engine once rendering/physics/audio exist.
}
} // namespace Aetherion::Runtime
