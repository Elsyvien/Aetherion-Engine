#include "Aetherion/Editor/EditorSettings.h"

#include <QSettings>

namespace Aetherion::Editor
{
EditorSettings EditorSettings::Load()
{
    QSettings settings("Aetherion", "Editor");

    EditorSettings out{};
    out.validationEnabled = settings.value("rendering/validationEnabled", true).toBool();
    out.verboseLogging = settings.value("rendering/verboseLogging", true).toBool();
    out.targetFps = settings.value("rendering/targetFps", 60).toInt();
    out.headlessSleepMs = settings.value("rendering/headlessSleepMs", 50).toInt();
    out.Clamp();
    return out;
}

void EditorSettings::Save() const
{
    QSettings settings("Aetherion", "Editor");
    settings.setValue("rendering/validationEnabled", validationEnabled);
    settings.setValue("rendering/verboseLogging", verboseLogging);
    settings.setValue("rendering/targetFps", targetFps);
    settings.setValue("rendering/headlessSleepMs", headlessSleepMs);
}
} // namespace Aetherion::Editor
