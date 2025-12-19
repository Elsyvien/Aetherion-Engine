#pragma once

#include <algorithm>

namespace Aetherion::Editor
{
struct EditorSettings
{
    bool validationEnabled{true};
    bool verboseLogging{true};
    int targetFps{60};
    int headlessSleepMs{50};

    void Clamp()
    {
        targetFps = std::clamp(targetFps, 1, 240);
        headlessSleepMs = std::clamp(headlessSleepMs, 0, 1000);
    }

    void Save() const;
    static EditorSettings Load();
};
} // namespace Aetherion::Editor
