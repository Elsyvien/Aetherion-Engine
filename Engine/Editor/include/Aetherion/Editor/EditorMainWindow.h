#pragma once

#include <filesystem>
#include <memory>
#include <cstdint>

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>
#include <QSize>

#include "Aetherion/Core/Types.h"
#include "Aetherion/Editor/EditorSettings.h"
#include "Aetherion/Editor/CommandHistory.h"

class QAction;
class QActionGroup;
class QLabel;
class QDockWidget;

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
class EditorAuxPanel;

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
    std::filesystem::path m_scenePath;
    bool m_sceneDirty{false};
    EditorSelection* m_selection = nullptr;
    QActionGroup* m_modeActionGroup = nullptr;
    QAction* m_modeEditAction = nullptr;
    QAction* m_modePlaytestAction = nullptr;
    QAction* m_modeUILayoutAction = nullptr;
    enum class GizmoMode
    {
        Translate,
        Rotate,
        Scale
    };
    GizmoMode m_gizmoMode{GizmoMode::Translate};
    QActionGroup* m_gizmoActionGroup = nullptr;
    QAction* m_gizmoTranslateAction = nullptr;
    QAction* m_gizmoRotateAction = nullptr;
    QAction* m_gizmoScaleAction = nullptr;
    QAction* m_snapToggleAction = nullptr;
    float m_snapTranslateStep{0.25f};
    float m_snapRotateStep{15.0f};
    float m_snapScaleStep{0.05f};
    EditorSettings m_settings{};
    bool m_validationEnabled{true};
    bool m_renderLoggingEnabled{true};
    int m_targetFrameIntervalMs{16};
    int m_headlessSleepMs{50};
    bool m_isPlaying{false};
    bool m_isPaused{false};

    std::unique_ptr<Rendering::VulkanViewport> m_vulkanViewport;
    WId m_surfaceHandle{0};
    QSize m_surfaceSize{};
    bool m_surfaceInitialized{false};
    class QTimer* m_renderTimer = nullptr;
    class QTimer* m_assetWatchTimer = nullptr;
    QElapsedTimer m_frameTimer;
    QLabel* m_fpsLabel = nullptr;
    QElapsedTimer m_fpsTimer;
    int m_fpsFrameCounter{0};
    QAction* m_validationMenuAction = nullptr;
    QAction* m_loggingMenuAction = nullptr;
    QAction* m_showHierarchyAction = nullptr;
    QAction* m_showInspectorAction = nullptr;
    QAction* m_showAssetBrowserAction = nullptr;
    QAction* m_showConsoleAction = nullptr;
    QAction* m_showMeshPreviewAction = nullptr;
    QAction* m_playAction = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_stepAction = nullptr;

    EditorViewport* m_viewport = nullptr;
    EditorHierarchyPanel* m_hierarchyPanel = nullptr;
    EditorInspectorPanel* m_inspectorPanel = nullptr;
    class EditorMeshPreview* m_meshPreview = nullptr;
    QDockWidget* m_hierarchyDock = nullptr;
    QDockWidget* m_inspectorDock = nullptr;
    QDockWidget* m_assetBrowserDock = nullptr;
    QDockWidget* m_consoleDock = nullptr;
    QDockWidget* m_meshPreviewDock = nullptr;
    EditorAssetBrowser* m_assetBrowser = nullptr;
    EditorConsole* m_console = nullptr;
    QByteArray m_defaultLayoutState;
    QByteArray m_defaultLayoutGeometry;
    EditorAuxPanel* m_auxPanel = nullptr;
    QString m_selectedAssetId;
    std::uint64_t m_assetChangeSerial{0};

    std::unique_ptr<CommandHistory> m_commandHistory;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;

    void CreateMenuBarContent();
    void CreateToolBarContent();
    void CreateDockPanels();
    void ConfigureStatusBar();
    void UpdateWindowTitle();
    void SetSceneDirty(bool dirty);
    std::filesystem::path GetAssetsRootPath() const;
    std::filesystem::path GetDefaultScenePath() const;
    void ApplySettings(const EditorSettings& settings, bool persist);
    void UpdateRenderTimerInterval(bool viewportReady);
    void OpenSettingsDialog();
    void RefreshAssetBrowser();
    void RescanAssets();
    void PollAssetChanges();
    void ImportGltfAsset();
    void AddAssetToScene(const QString& assetId);
    void DeleteAsset(const QString& assetId);
    void RenameAsset(const QString& assetId);
    void ShowAssetInExplorer(const QString& assetId);
    void DeleteEntity(Aetherion::Core::EntityId id);
    void DuplicateEntity(Aetherion::Core::EntityId id);
    void RenameEntity(Aetherion::Core::EntityId id);
    void CreateEmptyEntity(Aetherion::Core::EntityId parentId);
    void CreateLightEntity(Aetherion::Core::EntityId parentId);
    void SaveScene();
    void ReloadScene();
    bool ConfirmSaveIfDirty();
    bool SaveSceneToPath(const std::filesystem::path& path);
    bool LoadSceneFromPath(const std::filesystem::path& path);
    void RecreateRuntimeAndRenderer(bool enableValidation);
    void DestroyViewportRenderer();
    void AttachVulkanLogSink();
    void DetachVulkanLogSink();
    void LoadLayout();
    void SaveLayout() const;
    void UpdateRuntimeControlStates();
    void StartOrStopPlaySession();
    void TogglePauseSession();
    void StepSimulationOnce();
    void ActivateModeTab(int index);
    void ApplyTranslationDelta(float dx, float dy, float dz);
    void ApplyRotationDelta(float deltaDeg);
    void ApplyScaleDelta(float deltaUniform);
    void FocusCameraOnSelection();
    void RefreshSelectedEntityUi();
    
    void ExecuteCommand(std::unique_ptr<class Command> cmd);
    void Undo();
    void Redo();
    void UpdateUndoRedoState();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
};
} // namespace Aetherion::Editor
