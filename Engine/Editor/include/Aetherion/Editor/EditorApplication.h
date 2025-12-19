#pragma once

#include <memory>

class QApplication;

namespace Aetherion::Editor
{
struct EditorSettings;
}

namespace Aetherion::Editor
{
class EditorMainWindow;

class EditorApplication
{
public:
    EditorApplication(int& argc, char** argv);
    ~EditorApplication();

    EditorApplication(const EditorApplication&) = delete;
    EditorApplication& operator=(const EditorApplication&) = delete;

    int Run();

    // TODO: Expose hooks for project loading and runtime bootstrapping.
private:
    std::unique_ptr<QApplication> m_qtApp;
    std::unique_ptr<EditorMainWindow> m_mainWindow;
    std::unique_ptr<EditorSettings> m_settings;

    void InitializeUi();
};
} // namespace Aetherion::Editor
