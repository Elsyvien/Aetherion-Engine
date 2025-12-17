#pragma once

#include <string>

namespace Aetherion::Platform
{
struct WindowDescriptor
{
    int width = 1280;
    int height = 720;
    std::string title = "Aetherion";

    // TODO: Extend with fullscreen, vsync, DPI awareness, and surface handles.
};

class PlatformAbstractionLayer
{
public:
    PlatformAbstractionLayer() = default;
    ~PlatformAbstractionLayer() = default;

    void Initialize(const WindowDescriptor& descriptor);
    void Shutdown();

    // TODO: Expose window handles, input devices, and OS services.
private:
    WindowDescriptor m_descriptor;
};
} // namespace Aetherion::Platform
