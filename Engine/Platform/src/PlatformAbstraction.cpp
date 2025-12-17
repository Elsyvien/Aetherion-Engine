#include "Aetherion/Platform/PlatformAbstraction.h"

namespace Aetherion::Platform
{
void PlatformAbstractionLayer::Initialize(const WindowDescriptor& descriptor)
{
    m_descriptor = descriptor;
    // TODO: Create OS windows and hook into rendering surfaces.
}

void PlatformAbstractionLayer::Shutdown()
{
    // TODO: Destroy OS resources and detach from platform services.
}
} // namespace Aetherion::Platform
