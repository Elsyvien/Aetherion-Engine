#include "Aetherion/Runtime/EngineContext.h"

#include <utility>

namespace Aetherion::Runtime
{
EngineContext::EngineContext() = default;
EngineContext::~EngineContext() = default;

void EngineContext::SetProjectName(std::string name)
{
    m_projectName = std::move(name);
    // TODO: Notify interested systems about context changes.
}

const std::string& EngineContext::GetProjectName() const noexcept
{
    return m_projectName;
}
} // namespace Aetherion::Runtime
