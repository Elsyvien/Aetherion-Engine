#include "Aetherion/Editor/EditorSettings.h"

#include <QSettings>
#include <QString>

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
    
    // LLM Settings
    out.llm.provider = static_cast<LLMProviderType>(
        settings.value("llm/provider", 0).toInt());
    out.llm.apiKey = settings.value("llm/apiKey", "").toString().toStdString();
    out.llm.endpoint = settings.value("llm/endpoint", "").toString().toStdString();
    out.llm.model = settings.value("llm/model", "").toString().toStdString();
    out.llm.imageModel = settings.value("llm/imageModel", "").toString().toStdString();
    out.llm.timeoutMs = settings.value("llm/timeoutMs", 60000).toInt();
    out.llm.enableLogging = settings.value("llm/enableLogging", false).toBool();
    
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
    
    // LLM Settings
    settings.setValue("llm/provider", static_cast<int>(llm.provider));
    settings.setValue("llm/apiKey", QString::fromStdString(llm.apiKey));
    settings.setValue("llm/endpoint", QString::fromStdString(llm.endpoint));
    settings.setValue("llm/model", QString::fromStdString(llm.model));
    settings.setValue("llm/imageModel", QString::fromStdString(llm.imageModel));
    settings.setValue("llm/timeoutMs", llm.timeoutMs);
    settings.setValue("llm/enableLogging", llm.enableLogging);
}
} // namespace Aetherion::Editor
