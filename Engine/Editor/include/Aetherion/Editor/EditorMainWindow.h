#pragma once

#include <filesystem>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <array>
#include <deque>

#include <QByteArray>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QString>
#include <QSize>

#include "Aetherion/Core/Types.h"
#include "Aetherion/Editor/EditorSettings.h"
#include "Aetherion/Editor/CommandHistory.h"
#include "Aetherion/Editor/Commands/TransformCommand.h"

class QAction;
class QActionGroup;
class QLabel;
class QFileSystemWatcher;
class QListWidget;
class QPushButton;

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

namespace Aetherion::Scripting
{
class LogicCopilot;
} // namespace Aetherion::Scripting

namespace Aetherion::Editor
{
class EditorViewport;
class EditorHierarchyPanel;
class EditorInspectorPanel;
class EditorAssetBrowser;
class EditorCameraPreview;
class EditorConsole;
class EditorSelection;
class EditorAuxPanel;
class EditorCommandPalette;
class EditorAssetGenerationPanel;
class EditorStatisticsPanel;
class AICopilotPanel;
class AICopilotProcessor;
class EditorAnimationPanel;
class EditorLogicCopilotPanel;
class TabPanelManager;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp,
                              const EditorSettings& settings,
                              QWidget* parent = nullptr);
    ~EditorMainWindow() override;

    // TODO: Add menu actions for projects.
private:
    std::shared_ptr<Runtime::EngineApplication> m_runtimeApp;

    std::shared_ptr<Scene::Scene> m_scene;
    std::filesystem::path m_scenePath;
    bool m_sceneDirty{false};
    bool m_ignoreNextSceneChange{false};
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

    enum class GizmoAxis
    {
        None,
        X,
        Y,
        Z
    };
    GizmoAxis m_activeGizmoAxis{GizmoAxis::None};
    int m_dragStartMouseX{0};
    int m_dragStartMouseY{0};
    bool m_requestPickOnRelease{false};

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
    bool m_playSessionSnapshotValid{false};
    std::unordered_map<Core::EntityId, TransformData> m_playSessionSnapshot;

    std::unique_ptr<Rendering::VulkanViewport> m_vulkanViewport;
    WId m_surfaceHandle{0};
    QSize m_surfaceSize{};
    bool m_surfaceInitialized{false};
    class QTimer* m_renderTimer = nullptr;
    class QTimer* m_assetWatchTimer = nullptr;
    QFileSystemWatcher* m_assetFileWatcher = nullptr;
    QElapsedTimer m_frameTimer;
    QLabel* m_fpsLabel = nullptr;
    QElapsedTimer m_fpsTimer;
    int m_fpsFrameCounter{0};
    QAction* m_validationMenuAction = nullptr;
    QAction* m_loggingMenuAction = nullptr;
    QAction* m_commandPaletteAction = nullptr;
    QAction* m_focusAssetFilterAction = nullptr;
    QAction* m_playAction = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_stepAction = nullptr;
    QAction* m_resetAction = nullptr;
    QAction* m_showHierarchyAction = nullptr;
    QAction* m_showInspectorAction = nullptr;
    QAction* m_showAssetBrowserAction = nullptr;
    QAction* m_showConsoleAction = nullptr;
    QAction* m_showMeshPreviewAction = nullptr;
    QAction* m_showCameraPreviewAction = nullptr;
    QAction* m_showBookmarksAction = nullptr;
    QAction* m_showAICopilotAction = nullptr;
    QAction* m_showAssetGenAction = nullptr;
    QAction* m_showStatsAction = nullptr;
    QAction* m_showBottomPanelAction = nullptr;
    QAction* m_showAiHudAction = nullptr;
    QAction* m_showPerfHudAction = nullptr;

    class TabPanelManager* m_panelManager = nullptr;
    EditorViewport* m_viewport = nullptr;
    EditorHierarchyPanel* m_hierarchyPanel = nullptr;
    EditorInspectorPanel* m_inspectorPanel = nullptr;
    class EditorMeshPreview* m_meshPreview = nullptr;
    EditorCameraPreview* m_cameraPreview = nullptr;
    AICopilotPanel* m_copilotPanel = nullptr;
    EditorStatisticsPanel* m_statsPanel = nullptr;
    EditorAnimationPanel* m_animationPanel = nullptr;
    EditorLogicCopilotPanel* m_logicCopilotPanel = nullptr;
    EditorAssetBrowser* m_assetBrowser = nullptr;
    EditorAssetGenerationPanel* m_assetGenPanel = nullptr;
    EditorConsole* m_console = nullptr;
    QByteArray m_defaultLayoutState;
    QByteArray m_defaultLayoutGeometry;
    QByteArray m_defaultPanelVerticalState;
    QByteArray m_defaultPanelHorizontalState;
    int m_defaultLeftTabIndex{0};
    int m_defaultRightTabIndex{0};
    int m_defaultBottomTabIndex{0};
    bool m_defaultBottomVisible{true};
    EditorAuxPanel* m_auxPanel = nullptr;
    EditorCommandPalette* m_commandPalette = nullptr;
    QString m_selectedAssetId;
    std::uint64_t m_assetChangeSerial{0};
    bool m_assetWatcherDirty{true};

    std::unique_ptr<CommandHistory> m_commandHistory;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;

    std::unique_ptr<AICopilotProcessor> m_copilotProcessor;
    std::unique_ptr<Scripting::LogicCopilot> m_logicCopilot;

    // Interactive transform state for smooth dragging
    bool m_interactiveTransformActive = false;
    std::shared_ptr<Scene::Entity> m_interactiveEntity;
    TransformData m_interactiveOldData;
    TransformData m_interactiveTargetData;
    TransformData m_interactiveCurrentData;
    bool m_aiHudVisible{true};
    std::deque<QString> m_aiHudHistory;
    struct CameraBookmark {
        QString name;
        float posX{0.0f};
        float posY{0.0f};
        float posZ{0.0f};
        float rotY{0.0f};
        float rotX{0.0f};
        float zoom{1.0f};
    };
    std::vector<CameraBookmark> m_bookmarks;
    QListWidget* m_bookmarkList = nullptr;
    QPushButton* m_addBookmarkBtn = nullptr;
    QPushButton* m_renameBookmarkBtn = nullptr;
    QPushButton* m_deleteBookmarkBtn = nullptr;

    void BeginInteractiveTransform();
    void UpdateInteractiveTransformTarget(float dx, float dy, float dz);        
    void EndInteractiveTransform();
    void UpdateInteractiveTransform(float deltaTime);

    void CreateMenuBarContent();
    void CreateToolBarContent();
    void CreateTabPanels();
    void InitializeCommandPalette();
    void RegisterCommandAction(QAction* action,
                               const QString& category,
                               const QString& description = QString());
    void OpenCommandPalette();
    void ConfigureStatusBar();
    void UpdateWindowTitle();
    void SetSceneDirty(bool dirty);
    std::filesystem::path GetAssetsRootPath() const;
    std::filesystem::path GetDefaultScenePath() const;
    void ApplySettings(const EditorSettings& settings, bool persist);
    void UpdateRenderTimerInterval(bool viewportReady);
    void ApplyRuntimeAISettings();
    void OpenSettingsDialog();
    void RefreshAssetBrowser();
    void RescanAssets();
    void PollAssetChanges();
    void RefreshAssetWatchPaths();
    void ImportGltfAsset();
    void AddAssetToScene(const QString& assetId);
    void DeleteAsset(const QString& assetId);
    void RenameAsset(const QString& assetId);
    void ShowAssetInExplorer(const QString& assetId);
    void DeleteEntity(Aetherion::Core::EntityId id);
    void DuplicateEntity(Aetherion::Core::EntityId id);
    void RenameEntity(Aetherion::Core::EntityId id);
    Core::EntityId AllocateEntityId() const;
    void HandleCopilotPrompt(const QString& prompt);
    void UpdateAiHudFromSelection();
    void CreateEmptyEntity(Aetherion::Core::EntityId parentId);
    void CreateLightEntity(Aetherion::Core::EntityId parentId);
    void CreateCameraEntity(Aetherion::Core::EntityId parentId);
    void CreateMeshEntity(Aetherion::Core::EntityId parentId,
                          const QString& meshAssetId,
                          const QString& displayName);
    void CreateBookmarksDock();
    void RefreshBookmarksList();
    void AddBookmarkFromCamera();
    void RenameBookmark();
    void DeleteBookmark();
    void ApplyBookmark(int row);
    std::filesystem::path GetBookmarksPath() const;
    void LoadBookmarks();
    void SaveBookmarks() const;
    void OpenScene();
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
    void ResetPlaySession();
    void CapturePlaySessionSnapshot();
    void ClearPlaySessionSnapshot();
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
