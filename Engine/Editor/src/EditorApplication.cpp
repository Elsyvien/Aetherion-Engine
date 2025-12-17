#include "Aetherion/Editor/EditorApplication.h"

#include <QApplication>

#include "Aetherion/Editor/EditorMainWindow.h"
#include "Aetherion/Runtime/EngineApplication.h"

namespace Aetherion::Editor
{
EditorApplication::EditorApplication(int& argc, char** argv)
    : m_qtApp(std::make_unique<QApplication>(argc, argv))
{
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
    runtimeApp->Initialize();

    m_mainWindow = std::make_unique<EditorMainWindow>(runtimeApp);
    m_mainWindow->show();

    // TODO: Bridge editor commands to runtime (play, pause, step).
}
} // namespace Aetherion::Editor
