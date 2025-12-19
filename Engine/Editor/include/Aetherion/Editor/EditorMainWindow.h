#pragma once

#include <memory>

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QSize>

#include "Aetherion/Editor/EditorSettings.h"
#include "Aetherion/Rendering/RenderView.h"

class QAction;
class QLabel;

namespace Aetherion::Rendering
{
class VulkanViewport;
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
    explicit EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp,
                              const EditorSettings& settings,
                              QWidget* parent = nullptr);
    ~EditorMainWindow() override;

    // TODO: Add menu actions for projects, play/pause, and layout management.
private:
    std::shared_ptr<Runtime::EngineApplication> m_runtimeApp;

    std::shared_ptr<Scene::Scene> m_scene;
    EditorSelection* m_selection = nullptr;
    Rendering::RenderView m_renderView{};
    EditorSettings m_settings{};
    bool m_validationEnabled{true};
    bool m_renderLoggingEnabled{true};
    int m_targetFrameIntervalMs{16};
    int m_headlessSleepMs{50};

    std::unique_ptr<Rendering::VulkanViewport> m_vulkanViewport;
    WId m_surfaceHandle{0};
    QSize m_surfaceSize{};
    bool m_surfaceInitialized{false};
    class QTimer* m_renderTimer = nullptr;
    QElapsedTimer m_frameTimer;
    QLabel* m_fpsLabel = nullptr;
    QElapsedTimer m_fpsTimer;
    int m_fpsFrameCounter{0};
    QAction* m_validationMenuAction = nullptr;
    QAction* m_loggingMenuAction = nullptr;

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
    void ApplySettings(const EditorSettings& settings, bool persist);
    void UpdateRenderTimerInterval(bool viewportReady);
    void OpenSettingsDialog();
    void RefreshRenderView();
    void RecreateRuntimeAndRenderer(bool enableValidation);
    void DestroyViewportRenderer();
    void AttachVulkanLogSink();
    void DetachVulkanLogSink();
};
} // namespace Aetherion::Editor
