#include "Aetherion/Editor/EditorApplication.h"

#include <QApplication>
#include <cstdlib>
#include <string_view>

#include "Aetherion/Editor/EditorMainWindow.h"
#include "Aetherion/Editor/EditorSettings.h"
#include "Aetherion/Runtime/EngineApplication.h"

namespace Aetherion::Editor
{
EditorApplication::EditorApplication(int& argc, char** argv)
    : m_qtApp(std::make_unique<QApplication>(argc, argv))
    , m_settings(std::make_unique<EditorSettings>(EditorSettings::Load()))
{
    if (const char* env = std::getenv("AETHERION_ENABLE_VK_VALIDATION"))
    {
        std::string_view value(env);
        const bool enableValidation = !(value == "0" || value == "false" || value == "False");
        if (m_settings)
        {
            m_settings->validationEnabled = enableValidation;
            m_settings->Save();
        }
    }
}

EditorApplication::~EditorApplication() = default;

int EditorApplication::Run()
{
    InitializeUi();
    return m_qtApp->exec();
}

void EditorApplication::InitializeUi()
{
    auto runtimeApp = std::make_shared<Runtime::EngineApplication>();
    const EditorSettings activeSettings = m_settings ? *m_settings : EditorSettings{};
    runtimeApp->Initialize(activeSettings.validationEnabled, activeSettings.verboseLogging);

    m_mainWindow = std::make_unique<EditorMainWindow>(runtimeApp, activeSettings);
    m_mainWindow->show();

    // TODO: Bridge editor commands to runtime (play, pause, step).
}
} // namespace Aetherion::Editor
