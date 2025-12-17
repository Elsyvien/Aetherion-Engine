#pragma once

#include <memory>

#include <QByteArray>
#include <QMainWindow>

namespace Aetherion::Runtime
{
class EngineApplication;
} // namespace Aetherion::Runtime

namespace Aetherion::Editor
{
class EditorViewport;
class EditorHierarchyPanel;
class EditorInspectorPanel;
class EditorAssetBrowser;
class EditorConsole;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp, QWidget* parent = nullptr);
    ~EditorMainWindow() override;

    // TODO: Add menu actions for projects, play/pause, and layout management.
private:
    std::shared_ptr<Runtime::EngineApplication> m_runtimeApp;

    EditorViewport* m_viewport = nullptr;
    EditorHierarchyPanel* m_hierarchyPanel = nullptr;
    EditorInspectorPanel* m_inspectorPanel = nullptr;
    EditorAssetBrowser* m_assetBrowser = nullptr;
    EditorConsole* m_console = nullptr;
    QByteArray m_defaultLayoutState;

    void CreateMenuBarContent();
    void CreateToolBarContent();
    void CreateDockPanels();
    void ConfigureStatusBar();
};
} // namespace Aetherion::Editor
