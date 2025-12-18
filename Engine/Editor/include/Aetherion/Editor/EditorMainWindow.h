#pragma once

#include <memory>

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QSize>

namespace Aetherion::Rendering
{
class VulkanViewport;
struct RenderView;
} // namespace Aetherion::Rendering

namespace Aetherion::Runtime
{
class EngineApplication;
} // namespace Aetherion::Runtime

namespace Aetherion::Scene
{
class Scene;
class Entity;
} // namespace Aetherion::Scene

namespace Aetherion::Editor
{
class EditorViewport;
class EditorHierarchyPanel;
class EditorInspectorPanel;
class EditorAssetBrowser;
class EditorConsole;
class EditorSelection;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp, QWidget* parent = nullptr);
    ~EditorMainWindow() override;

    // TODO: Add menu actions for projects, play/pause, and layout management.
private:
    std::shared_ptr<Runtime::EngineApplication> m_runtimeApp;

    std::shared_ptr<Scene::Scene> m_scene;
    EditorSelection* m_selection = nullptr;
    Rendering::RenderView m_renderView{};
    bool m_validationEnabled{true};

    std::unique_ptr<Rendering::VulkanViewport> m_vulkanViewport;
    WId m_surfaceHandle{0};
    QSize m_surfaceSize{};
    bool m_surfaceInitialized{false};
    class QTimer* m_renderTimer = nullptr;
    QElapsedTimer m_frameTimer;

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
    void RefreshRenderView();
    void RecreateRuntimeAndRenderer(bool enableValidation);
};
} // namespace Aetherion::Editor
