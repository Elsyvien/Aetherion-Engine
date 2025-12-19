#include "Aetherion/Editor/EditorMainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QSysInfo>
#include <QCloseEvent>
#include <QActionGroup>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <string>
#include <utility>
#include <cstring>
#include <cmath>
#include <functional>

#include "Aetherion/Editor/EditorAssetBrowser.h"
#include "Aetherion/Editor/EditorConsole.h"
#include "Aetherion/Editor/EditorHierarchyPanel.h"
#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Editor/EditorInspectorPanel.h"
#include "Aetherion/Editor/EditorSettingsDialog.h"
#include "Aetherion/Editor/EditorViewport.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Rendering/VulkanViewport.h"
#include "Aetherion/Runtime/EngineApplication.h"

#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Editor
{
namespace
{
ConsoleSeverity ToConsoleSeverity(Rendering::LogSeverity severity)
{
    switch (severity)
    {
    case Rendering::LogSeverity::Error:
        return ConsoleSeverity::Error;
    case Rendering::LogSeverity::Warning:
        return ConsoleSeverity::Warning;
    default:
        return ConsoleSeverity::Info;
    }
}

void AppendConsole(EditorConsole* console, const QString& message, ConsoleSeverity severity)
{
    if (console)
    {
        console->AppendMessage(message, severity);
    }
}

void Mat4Identity(float out[16])
{
    std::memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void Mat4Mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    for (int c = 0; c < 4; ++c)
    {
        for (int rIdx = 0; rIdx < 4; ++rIdx)
        {
            r[c * 4 + rIdx] = a[0 * 4 + rIdx] * b[c * 4 + 0] + a[1 * 4 + rIdx] * b[c * 4 + 1] +
                              a[2 * 4 + rIdx] * b[c * 4 + 2] + a[3 * 4 + rIdx] * b[c * 4 + 3];
        }
    }
    std::memcpy(out, r, sizeof(r));
}

void Mat4RotationZ(float out[16], float radians)
{
    Mat4Identity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[4] = -s;
    out[1] = s;
    out[5] = c;
}

void Mat4Translation(float out[16], float x, float y, float z)

{
    Mat4Identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

void Mat4Scale(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}
} // namespace

EditorMainWindow::EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp,
                                   const EditorSettings& settings,
                                   QWidget* parent)
    : QMainWindow(parent)
    , m_runtimeApp(std::move(runtimeApp))
    , m_settings(settings)
{
    m_settings.Clamp();
    m_validationEnabled = m_settings.validationEnabled;
    m_renderLoggingEnabled = m_settings.verboseLogging;
    m_targetFrameIntervalMs = std::max(1, 1000 / std::max(1, m_settings.targetFps));
    m_headlessSleepMs = m_settings.headlessSleepMs;
    m_selection = new EditorSelection(this);

    setWindowTitle("Aetherion Editor");
    setWindowIcon(QApplication::windowIcon());
    resize(1440, 900);

    auto* centerSplit = new QSplitter(Qt::Horizontal, this);
    centerSplit->setChildrenCollapsible(false);

    m_viewport = new EditorViewport(centerSplit);
    m_viewport->setFocusPolicy(Qt::StrongFocus);
    m_viewport->installEventFilter(this);
    centerSplit->addWidget(m_viewport);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(m_targetFrameIntervalMs);
    connect(m_renderTimer, &QTimer::timeout, this, [this] {
        const bool viewportReady = m_vulkanViewport && m_vulkanViewport->IsReady();
        UpdateRenderTimerInterval(viewportReady);
        if (!viewportReady)
        {
            if (m_fpsLabel)
            {
                m_fpsLabel->setText(tr("FPS: --"));
            }
            m_fpsFrameCounter = 0;
            m_fpsTimer.invalidate();
            return;
        }
        if (isMinimized())
        {
            if (m_fpsLabel)
            {
                m_fpsLabel->setText(tr("FPS: --"));
            }
            m_fpsFrameCounter = 0;
            m_fpsTimer.invalidate();
            return;
        }

        const qint64 nanos = m_frameTimer.isValid() ? m_frameTimer.nsecsElapsed() : 0;
        const float dt = static_cast<float>(nanos) / 1'000'000'000.0f;
        m_frameTimer.restart();
        RefreshRenderView();
        try
        {
            m_vulkanViewport->RenderFrame(dt, m_renderView);
        }
        catch (const std::exception& ex)
        {
            AppendConsole(m_console, QString::fromStdString(ex.what()), ConsoleSeverity::Error);
            fprintf(stderr, "Render failed: %s\n", ex.what());
            m_renderTimer->stop();
            statusBar()->showMessage(tr("Renderer error: %1").arg(QString::fromStdString(ex.what())));
            return;
        }

        if (m_fpsLabel)
        {
            if (!m_fpsTimer.isValid())
            {
                m_fpsTimer.start();
                m_fpsFrameCounter = 0;
            }

            ++m_fpsFrameCounter;
            const qint64 elapsedMs = m_fpsTimer.elapsed();
            if (elapsedMs >= 1000)
            {
                const double fps = (elapsedMs > 0) ? (static_cast<double>(m_fpsFrameCounter) * 1000.0 / static_cast<double>(elapsedMs))
                                                   : 0.0;
                m_fpsLabel->setText(tr("FPS: %1").arg(QString::number(fps, 'f', 1)));
                m_fpsFrameCounter = 0;
                m_fpsTimer.restart();
            }
        }
    });

    connect(m_viewport, &EditorViewport::surfaceReady, this, [this](WId nativeHandle, int width, int height) {
        m_surfaceHandle = nativeHandle;
        m_surfaceSize = QSize(width, height);
        m_surfaceInitialized = true;

        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
        if (!vk)
        {
            return;
        }

        DestroyViewportRenderer();

        try
        {
            m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk);
            m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
            m_vulkanViewport->Initialize(reinterpret_cast<void*>(nativeHandle), width, height);
        }
        catch (const std::exception& ex)
        {
            AppendConsole(m_console, QString::fromStdString(ex.what()), ConsoleSeverity::Error);
            statusBar()->showMessage(tr("Viewport renderer unavailable: %1").arg(QString::fromStdString(ex.what())));
            return;
        }

        if (m_vulkanViewport->IsReady())
        {
            m_frameTimer.start();
            m_renderTimer->start();
            m_fpsFrameCounter = 0;
            m_fpsTimer.start();
            statusBar()->showMessage(tr("Viewport Vulkan renderer active"));
        }
        else
        {
            statusBar()->showMessage(tr("Viewport renderer unavailable: swapchain not supported on this adapter"));
        }
    });

    connect(m_viewport, &EditorViewport::surfaceResized, this, [this](int width, int height) {
        m_surfaceSize = QSize(width, height);
        if (m_vulkanViewport && m_vulkanViewport->IsReady())
        {
            try
            {
                m_vulkanViewport->Resize(width, height);
            }
            catch (const std::exception& ex)
            {
                AppendConsole(m_console, QString::fromStdString(ex.what()), ConsoleSeverity::Error);
                statusBar()->showMessage(tr("Viewport resize failed: %1").arg(QString::fromStdString(ex.what())));
            }
        }
    });

    auto* secondaryPlaceholder = new QWidget(centerSplit);
    secondaryPlaceholder->setMinimumWidth(220);
    auto* secondaryLayout = new QVBoxLayout(secondaryPlaceholder);
    secondaryLayout->setContentsMargins(0, 0, 0, 0);
    auto* secondaryLabel = new QLabel(tr("Auxiliary View (placeholder)"), secondaryPlaceholder);
    secondaryLabel->setAlignment(Qt::AlignCenter);
    secondaryLayout->addWidget(secondaryLabel);
    centerSplit->addWidget(secondaryPlaceholder);
    centerSplit->setStretchFactor(0, 1);
    centerSplit->setStretchFactor(1, 0);

    setCentralWidget(centerSplit);

    CreateDockPanels();
    CreateMenuBarContent();
    CreateToolBarContent();
    ConfigureStatusBar();
    LoadLayout();

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->SetSelectionModel(m_selection);
    }

    m_scene = m_runtimeApp ? m_runtimeApp->GetActiveScene() : nullptr;
    if (m_selection)
    {
        m_selection->SetActiveScene(m_scene);
    }
    if (m_scene && m_hierarchyPanel)
    {
        m_hierarchyPanel->BindScene(m_scene);
    }

    AttachVulkanLogSink();

    connect(m_selection, &EditorSelection::SelectionChanged, this, [this](Aetherion::Core::EntityId) {
        AppendConsole(m_console, tr("Selection: entity changed"), ConsoleSeverity::Info);
        if (m_assetBrowser)
        {
            m_assetBrowser->ClearSelection();
        }
        if (m_inspectorPanel)
        {
            m_inspectorPanel->SetSelectedEntity(m_selection->GetSelectedEntity());
        }
    });
    connect(m_selection, &EditorSelection::SelectionCleared, this, [this]() {
        AppendConsole(m_console, tr("Selection: entity cleared"), ConsoleSeverity::Info);
        if (m_inspectorPanel)
        {
            m_inspectorPanel->SetSelectedEntity(nullptr);
        }
    });

    if (m_hierarchyPanel)
    {
        connect(m_hierarchyPanel, &EditorHierarchyPanel::entityActivated, this, [this](Aetherion::Core::EntityId) {
            // Re-open the Properties/Inspector panel if the user closed it.
            if (m_inspectorDock)
            {
                m_inspectorDock->show();
                m_inspectorDock->raise();
            }
        });

        connect(m_hierarchyPanel,
                &EditorHierarchyPanel::entityReparentRequested,
                this,
                [this](Aetherion::Core::EntityId childId, Aetherion::Core::EntityId newParentId) {
                    if (!m_scene)
                    {
                        return;
                    }

                    const bool success = m_scene->SetParent(childId, newParentId);
                    if (!success)
                    {
                        if (m_console)
                        {
                            m_console->AppendMessage(tr("Reparent blocked (invalid target)"), ConsoleSeverity::Warning);
                        }
                        if (m_hierarchyPanel)
                        {
                            m_hierarchyPanel->BindScene(m_scene);
                        }
                        return;
                    }

                    if (m_console)
                    {
                        const QString msg = (newParentId == 0)
                                                ? tr("Entity %1 unparented").arg(childId)
                                                : tr("Entity %1 parented to %2").arg(childId).arg(newParentId);
                        m_console->AppendMessage(msg, ConsoleSeverity::Info);
                    }
                    if (m_hierarchyPanel)
                    {
                        m_hierarchyPanel->BindScene(m_scene);
                        m_hierarchyPanel->SetSelectedEntity(childId);
                    }
                });
    }

    if (m_inspectorPanel)
    {
        connect(m_inspectorPanel,
                &EditorInspectorPanel::transformChanged,
                this,
                [this](Aetherion::Core::EntityId,
                       float,
                       float,
                       float,
                       float,
                       float) { RefreshRenderView(); });
    }

    if (m_assetBrowser)
    {
        connect(m_assetBrowser, &EditorAssetBrowser::AssetSelected, this, [this](const QString& assetId) {
            AppendConsole(m_console, tr("AssetBrowser: selected '%1'").arg(assetId), ConsoleSeverity::Info);
            if (m_inspectorPanel)
            {
                m_inspectorPanel->SetSelectedAsset(assetId);
                AppendConsole(m_console, tr("Inspector: showing asset '%1'").arg(assetId), ConsoleSeverity::Info);
            }
        });
        connect(m_assetBrowser, &EditorAssetBrowser::AssetSelectionCleared, this, [this] {
            AppendConsole(m_console, tr("AssetBrowser: selection cleared"), ConsoleSeverity::Info);
            // Keep current entity selection (if any) as source of truth.
            if (m_inspectorPanel)
            {
                m_inspectorPanel->SetSelectedEntity(m_selection ? m_selection->GetSelectedEntity() : nullptr);
                AppendConsole(m_console, tr("Inspector: reverted to entity selection"), ConsoleSeverity::Info);
            }
        });
        connect(m_assetBrowser, &EditorAssetBrowser::AssetActivated, this, [this] {
            if (m_inspectorDock)
            {
                m_inspectorDock->show();
                m_inspectorDock->raise();
            }
        });
    }
}

EditorMainWindow::~EditorMainWindow()
{
    DestroyViewportRenderer();
    DetachVulkanLogSink();

    if (m_runtimeApp)
    {
        m_runtimeApp->Shutdown();
    }
}

void EditorMainWindow::CreateMenuBarContent()
{
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("New Project"), [] {});
    fileMenu->addAction(tr("Open Project..."), [] {});
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Exit"), this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    auto* preferences = editMenu->addAction(tr("Preferences"));
    connect(preferences, &QAction::triggered, this, &EditorMainWindow::OpenSettingsDialog);
    m_validationMenuAction = editMenu->addAction(tr("Enable Vulkan Validation Layers"));
    m_validationMenuAction->setCheckable(true);
    m_validationMenuAction->setChecked(m_validationEnabled);
    connect(m_validationMenuAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled == m_validationEnabled)
        {
            return;
        }
        m_settings.validationEnabled = enabled;
        ApplySettings(m_settings, true);
    });

    m_loggingMenuAction = editMenu->addAction(tr("Verbose Rendering Logs"));
    m_loggingMenuAction->setCheckable(true);
    m_loggingMenuAction->setChecked(m_renderLoggingEnabled);
    connect(m_loggingMenuAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled == m_renderLoggingEnabled)
        {
            return;
        }
        m_settings.verboseLogging = enabled;
        ApplySettings(m_settings, true);
    });

    auto* viewMenu = menuBar()->addMenu(tr("&View"));

    m_showHierarchyAction = viewMenu->addAction(tr("Show Hierarchy"));
    m_showHierarchyAction->setCheckable(true);
    m_showHierarchyAction->setChecked(true);
    connect(m_showHierarchyAction, &QAction::triggered, this, [this](bool checked) {
        if (m_hierarchyDock)
        {
            m_hierarchyDock->setVisible(checked);
            if (checked)
            {
                m_hierarchyDock->raise();
            }
        }
    });

    m_showInspectorAction = viewMenu->addAction(tr("Show Inspector"));
    m_showInspectorAction->setCheckable(true);
    m_showInspectorAction->setChecked(true);
    connect(m_showInspectorAction, &QAction::triggered, this, [this](bool checked) {
        if (m_inspectorDock)
        {
            m_inspectorDock->setVisible(checked);
            if (checked)
            {
                m_inspectorDock->raise();
            }
        }
    });

    m_showAssetBrowserAction = viewMenu->addAction(tr("Show Asset Browser"));
    m_showAssetBrowserAction->setCheckable(true);
    m_showAssetBrowserAction->setChecked(true);
    connect(m_showAssetBrowserAction, &QAction::triggered, this, [this](bool checked) {
        if (m_assetBrowserDock)
        {
            m_assetBrowserDock->setVisible(checked);
            if (checked)
            {
                m_assetBrowserDock->raise();
            }
        }
    });

    m_showConsoleAction = viewMenu->addAction(tr("Show Console"));
    m_showConsoleAction->setCheckable(true);
    m_showConsoleAction->setChecked(true);
    connect(m_showConsoleAction, &QAction::triggered, this, [this](bool checked) {
        if (m_consoleDock)
        {
            m_consoleDock->setVisible(checked);
            if (checked)
            {
                m_consoleDock->raise();
            }
        }
    });

    viewMenu->addSeparator();
    viewMenu->addAction(tr("Reset Layout"), [this] {
        // TODO: Support named layout presets and user-defined layouts.
        restoreState(m_defaultLayoutState);
        m_defaultLayoutState = saveState();
        SaveLayout();
    });

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* aboutAction = helpMenu->addAction(tr("About Aetherion"));
    connect(aboutAction, &QAction::triggered, this, [this] {
        const QString version = QCoreApplication::applicationVersion().isEmpty()
                                    ? tr("dev")
                                    : QCoreApplication::applicationVersion();
        const QString text = tr("Aetherion Editor using Aetherion-Engine.\n© Max Staneker 2026\nVersion: %1\nUI: Qt 6\nRenderer: Vulkan\n\nBuild: %2 %3")
                                 .arg(version)
                                 .arg(QString::fromLatin1(__DATE__))
                                 .arg(QString::fromLatin1(__TIME__))
                             + tr("\nRunning on: %1").arg(QSysInfo::prettyProductName());
        QMessageBox::about(this, tr("About Aetherion"), text);
    });
}

void EditorMainWindow::CreateToolBarContent()
{
    auto* toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName("MainToolBar");
    toolBar->setMovable(false);
    toolBar->setAllowedAreas(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, toolBar);

    m_playAction = toolBar->addAction(tr("Play"));
    m_pauseAction = toolBar->addAction(tr("Pause"));
    m_stepAction = toolBar->addAction(tr("Step"));

    m_playAction->setCheckable(true);
    m_pauseAction->setCheckable(true);

    connect(m_playAction, &QAction::triggered, this, [this] { StartOrStopPlaySession(); });
    connect(m_pauseAction, &QAction::triggered, this, [this] { TogglePauseSession(); });
    connect(m_stepAction, &QAction::triggered, this, [this] { StepSimulationOnce(); });

    toolBar->addSeparator();

    m_modeActionGroup = new QActionGroup(toolBar);
    m_modeActionGroup->setExclusive(true);

    m_modeEditAction = toolBar->addAction(tr("Edit"));
    m_modePlaytestAction = toolBar->addAction(tr("Playtest"));
    m_modeUILayoutAction = toolBar->addAction(tr("UI Layout"));

    m_modeEditAction->setCheckable(true);
    m_modePlaytestAction->setCheckable(true);
    m_modeUILayoutAction->setCheckable(true);

    m_modeEditAction->setActionGroup(m_modeActionGroup);
    m_modePlaytestAction->setActionGroup(m_modeActionGroup);
    m_modeUILayoutAction->setActionGroup(m_modeActionGroup);

    connect(m_modeEditAction, &QAction::triggered, this, [this] { ActivateModeTab(0); });
    connect(m_modePlaytestAction, &QAction::triggered, this, [this] { ActivateModeTab(1); });
    connect(m_modeUILayoutAction, &QAction::triggered, this, [this] { ActivateModeTab(2); });

    toolBar->addSeparator();

    m_gizmoActionGroup = new QActionGroup(toolBar);
    m_gizmoActionGroup->setExclusive(true);

    m_gizmoTranslateAction = toolBar->addAction(tr("Move"));
    m_gizmoRotateAction = toolBar->addAction(tr("Rotate"));
    m_gizmoScaleAction = toolBar->addAction(tr("Scale"));

    m_gizmoTranslateAction->setShortcut(QKeySequence(Qt::Key_W));
    m_gizmoRotateAction->setShortcut(QKeySequence(Qt::Key_E));
    m_gizmoScaleAction->setShortcut(QKeySequence(Qt::Key_R));

    m_gizmoTranslateAction->setCheckable(true);
    m_gizmoRotateAction->setCheckable(true);
    m_gizmoScaleAction->setCheckable(true);

    m_gizmoTranslateAction->setActionGroup(m_gizmoActionGroup);
    m_gizmoRotateAction->setActionGroup(m_gizmoActionGroup);
    m_gizmoScaleAction->setActionGroup(m_gizmoActionGroup);

    connect(m_gizmoTranslateAction, &QAction::triggered, this, [this] { m_gizmoMode = GizmoMode::Translate; });
    connect(m_gizmoRotateAction, &QAction::triggered, this, [this] { m_gizmoMode = GizmoMode::Rotate; });
    connect(m_gizmoScaleAction, &QAction::triggered, this, [this] { m_gizmoMode = GizmoMode::Scale; });

    m_snapToggleAction = toolBar->addAction(tr("Snap"));
    m_snapToggleAction->setCheckable(true);
    m_snapToggleAction->setChecked(true);
    m_snapToggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));

    ActivateModeTab(0);
    m_gizmoTranslateAction->setChecked(true);
    UpdateRuntimeControlStates();
}

void EditorMainWindow::ApplySettings(const EditorSettings& settings, bool persist)
{
    const bool validationChanged = settings.validationEnabled != m_validationEnabled;
    const bool loggingChanged = settings.verboseLogging != m_renderLoggingEnabled;

    m_settings = settings;
    m_settings.Clamp();
    m_validationEnabled = m_settings.validationEnabled;
    m_renderLoggingEnabled = m_settings.verboseLogging;
    m_targetFrameIntervalMs = std::max(1, 1000 / std::max(1, m_settings.targetFps));
    m_headlessSleepMs = m_settings.headlessSleepMs;

    if (persist)
    {
        m_settings.Save();
    }

    if (m_validationMenuAction)
    {
        m_validationMenuAction->blockSignals(true);
        m_validationMenuAction->setChecked(m_validationEnabled);
        m_validationMenuAction->blockSignals(false);
    }
    if (m_loggingMenuAction)
    {
        m_loggingMenuAction->blockSignals(true);
        m_loggingMenuAction->setChecked(m_renderLoggingEnabled);
        m_loggingMenuAction->blockSignals(false);
    }

    if (auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr)
    {
        if (auto vk = ctx->GetVulkanContext())
        {
            vk->SetLoggingEnabled(m_renderLoggingEnabled);
        }
    }

    if (validationChanged)
    {
        RecreateRuntimeAndRenderer(m_validationEnabled);
    }
    else if (m_vulkanViewport)
    {
        m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
    }

    UpdateRenderTimerInterval(m_vulkanViewport && m_vulkanViewport->IsReady());
}

void EditorMainWindow::UpdateRenderTimerInterval(bool viewportReady)
{
    if (!m_renderTimer)
    {
        return;
    }

    const bool headless = !viewportReady || !m_surfaceInitialized || isMinimized();
    const int desiredInterval = (headless && m_headlessSleepMs > 0) ? m_headlessSleepMs : m_targetFrameIntervalMs;
    if (desiredInterval > 0 && m_renderTimer->interval() != desiredInterval)
    {
        m_renderTimer->setInterval(desiredInterval);
    }
}

void EditorMainWindow::OpenSettingsDialog()
{
    EditorSettingsDialog dialog(m_settings, this);
    if (dialog.exec() == QDialog::Accepted)
    {
        ApplySettings(dialog.GetSettings(), true);
    }
}

void EditorMainWindow::RefreshRenderView()
{
    m_renderView.instances.clear();
    m_renderView.batches.clear();

    if (!m_scene)
    {
        return;
    }

    std::unordered_map<const Scene::MeshRendererComponent*, size_t> batchLookup;

    std::unordered_map<Core::EntityId, const Scene::TransformComponent*> transformLookup;
    std::unordered_map<Core::EntityId, const Scene::MeshRendererComponent*> meshLookup;
    for (const auto& entity : m_scene->GetEntities())
    {
        if (!entity)
        {
            continue;
        }

        if (auto transform = entity->GetComponent<Scene::TransformComponent>())
        {
            transformLookup.emplace(entity->GetId(), transform.get());
        }

        if (auto mesh = entity->GetComponent<Scene::MeshRendererComponent>())
        {
            meshLookup.emplace(entity->GetId(), mesh.get());
        }
    }

    std::unordered_map<Core::EntityId, std::array<float, 16>> worldCache;
    std::function<const std::array<float, 16>&(Core::EntityId)> modelFor = [&](Core::EntityId id)
    {
        auto cached = worldCache.find(id);
        if (cached != worldCache.end())
        {
            return cached->second;
        }

        std::array<float, 16> identity{};
        Mat4Identity(identity.data());

        auto it = transformLookup.find(id);
        if (it == transformLookup.end() || it->second == nullptr)
        {
            return worldCache.emplace(id, identity).first->second;
        }

        const auto* transform = it->second;
        const float radians = transform->GetRotationZDegrees() * (3.14159265358979323846f / 180.0f);

        float t[16];
        Mat4Translation(t, transform->GetPositionX(), transform->GetPositionY(), 0.0f);

        float r[16];
        Mat4RotationZ(r, radians);

        float s[16];
        Mat4Scale(s, transform->GetScaleX(), transform->GetScaleY(), 1.0f);

        float tr[16];
        Mat4Mul(tr, t, r);

        float localModel[16];
        Mat4Mul(localModel, tr, s);

        if (transform->HasParent())
        {
            const auto& parentModel = modelFor(transform->GetParentId());
            float world[16];
            Mat4Mul(world, parentModel.data(), localModel);
            std::array<float, 16> stored{};
            std::memcpy(stored.data(), world, sizeof(world));
            return worldCache.emplace(id, stored).first->second;
        }

        std::array<float, 16> stored{};
        std::memcpy(stored.data(), localModel, sizeof(localModel));
        return worldCache.emplace(id, stored).first->second;
    };

    for (const auto& entity : m_scene->GetEntities())
    {
        if (!entity)
        {
            continue;
        }

        auto transform = entity->GetComponent<Scene::TransformComponent>();
        auto mesh = entity->GetComponent<Scene::MeshRendererComponent>();
        if (!transform || !mesh || !mesh->IsVisible())
        {
            continue;
        }

        Rendering::RenderInstance instance{};
        instance.entityId = entity->GetId();
        instance.transform = transform.get();
        instance.mesh = mesh.get();
        const auto& model = modelFor(instance.entityId);
        std::memcpy(instance.model, model.data(), sizeof(instance.model));
        instance.hasModel = true;
        m_renderView.instances.push_back(instance);

        size_t batchIndex = 0;
        auto found = batchLookup.find(instance.mesh);
        if (found == batchLookup.end())
        {
            batchIndex = m_renderView.batches.size();
            batchLookup[instance.mesh] = batchIndex;
            m_renderView.batches.emplace_back();
        }
        else
        {
            batchIndex = found->second;
        }

        m_renderView.batches[batchIndex].instances.push_back(instance);
    }

    m_renderView.transforms = transformLookup;
    m_renderView.meshes = meshLookup;
}

void EditorMainWindow::RecreateRuntimeAndRenderer(bool enableValidation)
{
    m_validationEnabled = enableValidation;
    m_settings.validationEnabled = enableValidation;

    DestroyViewportRenderer();
    DetachVulkanLogSink();

    if (m_runtimeApp)
    {
        m_runtimeApp->Shutdown();
    }

    m_runtimeApp = std::make_shared<Runtime::EngineApplication>();
    try
    {
        m_runtimeApp->Initialize(m_validationEnabled, m_renderLoggingEnabled);
    }
    catch (const std::exception& ex)
    {
        AppendConsole(m_console, QString::fromStdString(ex.what()), ConsoleSeverity::Error);
        statusBar()->showMessage(tr("Renderer reset failed: %1").arg(QString::fromStdString(ex.what())));
        m_runtimeApp.reset();
        m_scene.reset();
        return;
    }

    m_scene = m_runtimeApp->GetActiveScene();
    if (m_selection)
    {
        m_selection->SetActiveScene(m_scene);
    }

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->SetSelectionModel(m_selection);
        m_hierarchyPanel->BindScene(m_scene);
    }

    if (m_selection)
    {
        if (!m_scene)
        {
            m_selection->Clear();
        }
    }

    if (m_inspectorPanel)
    {
        m_inspectorPanel->SetSelectedEntity(m_selection ? m_selection->GetSelectedEntity() : nullptr);
    }

    AttachVulkanLogSink();

    if (m_surfaceInitialized && m_surfaceHandle != 0)
    {
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
        if (vk)
        {
            try
            {
                DestroyViewportRenderer();
                m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk);
                m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
                m_vulkanViewport->Initialize(reinterpret_cast<void*>(m_surfaceHandle),
                                             m_surfaceSize.width(),
                                             m_surfaceSize.height());
                if (m_vulkanViewport->IsReady())
                {
                    m_frameTimer.restart();
                    m_renderTimer->start();
                    m_fpsFrameCounter = 0;
                    m_fpsTimer.start();
                }
            }
            catch (const std::exception& ex)
            {
                AppendConsole(m_console, QString::fromStdString(ex.what()), ConsoleSeverity::Error);
                statusBar()->showMessage(tr("Renderer reset failed: %1").arg(QString::fromStdString(ex.what())));
            }
        }
    }

    RefreshRenderView();
    UpdateRenderTimerInterval(m_vulkanViewport && m_vulkanViewport->IsReady());
    statusBar()->showMessage(tr("Renderer reset (%1 validation, %2 logging)")
                                 .arg(m_validationEnabled ? tr("with") : tr("without"))
                                 .arg(m_renderLoggingEnabled ? tr("verbose") : tr("minimal")));
}

void EditorMainWindow::DestroyViewportRenderer()
{
    if (m_renderTimer)
    {
        m_renderTimer->stop();
    }

    if (m_vulkanViewport)
    {
        m_vulkanViewport->Shutdown();
        m_vulkanViewport.reset();
    }

    m_frameTimer.invalidate();
    m_fpsTimer.invalidate();
    m_fpsFrameCounter = 0;
}

void EditorMainWindow::AttachVulkanLogSink()
{
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
    if (!vk)
    {
        return;
    }

    vk->SetLoggingEnabled(m_renderLoggingEnabled);
    vk->SetLogCallback([this](Rendering::LogSeverity severity, const std::string& message) {
        AppendConsole(m_console, QString::fromStdString(message), ToConsoleSeverity(severity));
    });
}

void EditorMainWindow::DetachVulkanLogSink()
{
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
    if (vk)
    {
        vk->SetLogCallback(nullptr);
    }
}

void EditorMainWindow::LoadLayout()
{
    QSettings settings("Aetherion", "Editor");
    const QByteArray saved = settings.value("layout/mainWindow").toByteArray();
    if (!saved.isEmpty())
    {
        restoreState(saved);
        m_defaultLayoutState = saved;
    }
    else
    {
        m_defaultLayoutState = saveState();
        settings.setValue("layout/mainWindow", m_defaultLayoutState);
    }
}

void EditorMainWindow::SaveLayout() const
{
    QSettings settings("Aetherion", "Editor");
    settings.setValue("layout/mainWindow", saveState());
}

void EditorMainWindow::closeEvent(QCloseEvent* event)
{
    SaveLayout();
    QMainWindow::closeEvent(event);
}

void EditorMainWindow::CreateDockPanels()
{
    auto* hierarchyDock = new QDockWidget(tr("Hierarchy"), this);
    hierarchyDock->setObjectName("HierarchyDock");
    hierarchyDock->setAttribute(Qt::WA_NativeWindow, true);
    m_hierarchyDock = hierarchyDock;
    m_hierarchyPanel = new EditorHierarchyPanel(hierarchyDock);
    hierarchyDock->setWidget(m_hierarchyPanel);
    hierarchyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
    connect(hierarchyDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_showHierarchyAction)
        {
            m_showHierarchyAction->blockSignals(true);
            m_showHierarchyAction->setChecked(visible);
            m_showHierarchyAction->blockSignals(false);
        }
    });

    auto* inspectorDock = new QDockWidget(tr("Inspector"), this);
    inspectorDock->setObjectName("InspectorDock");
    inspectorDock->setAttribute(Qt::WA_NativeWindow, true);
    m_inspectorDock = inspectorDock;
    m_inspectorPanel = new EditorInspectorPanel(inspectorDock);
    inspectorDock->setWidget(m_inspectorPanel);
    inspectorDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    connect(inspectorDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_showInspectorAction)
        {
            m_showInspectorAction->blockSignals(true);
            m_showInspectorAction->setChecked(visible);
            m_showInspectorAction->blockSignals(false);
        }
    });

    auto* assetDock = new QDockWidget(tr("Asset Browser"), this);
    assetDock->setObjectName("AssetBrowserDock");
    assetDock->setAttribute(Qt::WA_NativeWindow, true);
    m_assetBrowserDock = assetDock;
    m_assetBrowser = new EditorAssetBrowser(assetDock);
    assetDock->setWidget(m_assetBrowser);
    assetDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, assetDock);
    connect(assetDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_showAssetBrowserAction)
        {
            m_showAssetBrowserAction->blockSignals(true);
            m_showAssetBrowserAction->setChecked(visible);
            m_showAssetBrowserAction->blockSignals(false);
        }
    });

    auto* consoleDock = new QDockWidget(tr("Console"), this);
    consoleDock->setObjectName("ConsoleDock");
    consoleDock->setAttribute(Qt::WA_NativeWindow, true);
    m_consoleDock = consoleDock;
    m_console = new EditorConsole(consoleDock);
    consoleDock->setWidget(m_console);
    consoleDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);
    connect(consoleDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_showConsoleAction)
        {
            m_showConsoleAction->blockSignals(true);
            m_showConsoleAction->setChecked(visible);
            m_showConsoleAction->blockSignals(false);
        }
    });

    tabifyDockWidget(assetDock, consoleDock);
    consoleDock->raise();

    // TODO: Persist dock layout between sessions.
}

void EditorMainWindow::ConfigureStatusBar()
{
    statusBar()->showMessage(tr("Aetherion scaffolding - runtime disconnected"));
    if (!m_fpsLabel)
    {
        m_fpsLabel = new QLabel(tr("FPS: --"), this);
        m_fpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusBar()->addPermanentWidget(m_fpsLabel);
    }
}

void EditorMainWindow::UpdateRuntimeControlStates()
{
    if (m_playAction)
    {
        const bool showStop = m_isPlaying && !m_isPaused;
        m_playAction->blockSignals(true);
        m_playAction->setChecked(showStop);
        m_playAction->setText(showStop ? tr("Stop") : tr("Play"));
        m_playAction->blockSignals(false);
    }

    if (m_pauseAction)
    {
        m_pauseAction->blockSignals(true);
        m_pauseAction->setEnabled(m_isPlaying);
        m_pauseAction->setChecked(m_isPaused);
        m_pauseAction->setText(m_isPaused ? tr("Resume") : tr("Pause"));
        m_pauseAction->blockSignals(false);
    }

    if (m_stepAction)
    {
        m_stepAction->setEnabled(m_isPlaying && m_isPaused);
    }
}

void EditorMainWindow::StartOrStopPlaySession()
{
    if (m_isPlaying && !m_isPaused)
    {
        m_isPlaying = false;
        m_isPaused = false;
        AppendConsole(m_console, tr("Stopped play session (runtime stub)"), ConsoleSeverity::Info);
        statusBar()->showMessage(tr("Stopped play session"), 2000);
        UpdateRuntimeControlStates();
        return;
    }

    m_isPlaying = true;
    m_isPaused = false;
    AppendConsole(m_console, tr("Started play session (runtime stub)"), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Play session started"), 2000);
    UpdateRuntimeControlStates();
}

void EditorMainWindow::TogglePauseSession()
{
    if (!m_isPlaying)
    {
        statusBar()->showMessage(tr("No active play session"), 2000);
        return;
    }

    m_isPaused = !m_isPaused;
    AppendConsole(m_console, m_isPaused ? tr("Paused session (runtime stub)") : tr("Resumed session (runtime stub)"), ConsoleSeverity::Info);
    statusBar()->showMessage(m_isPaused ? tr("Session paused") : tr("Session resumed"), 2000);
    UpdateRuntimeControlStates();
}

void EditorMainWindow::StepSimulationOnce()
{
    if (!m_isPlaying || !m_isPaused)
    {
        statusBar()->showMessage(tr("Step is available while paused"), 2500);
        return;
    }

    AppendConsole(m_console, tr("Stepped simulation once (placeholder)"), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Stepped simulation (placeholder)"), 2000);
}

void EditorMainWindow::ActivateModeTab(int index)
{
    if (index < 0 || index > 2)
    {
        return;
    }

    // Reflect checked state on actions
    if (m_modeEditAction && m_modePlaytestAction && m_modeUILayoutAction)
    {
        m_modeEditAction->blockSignals(true);
        m_modePlaytestAction->blockSignals(true);
        m_modeUILayoutAction->blockSignals(true);
        m_modeEditAction->setChecked(index == 0);
        m_modePlaytestAction->setChecked(index == 1);
        m_modeUILayoutAction->setChecked(index == 2);
        m_modeEditAction->blockSignals(false);
        m_modePlaytestAction->blockSignals(false);
        m_modeUILayoutAction->blockSignals(false);
    }

    const QString label = (index == 0) ? tr("Edit") : (index == 1) ? tr("Playtest") : tr("UI Layout");
    QString detail;
    if (label == tr("Edit"))
    {
        detail = tr("Edit workspace (placeholder) ready for scene tools");
    }
    else if (label == tr("Playtest"))
    {
        detail = tr("Future in-editor runtime preview will appear here");
    }
    else
    {
        detail = tr("UI layout customization placeholder");
    }

    AppendConsole(m_console, tr("Switched to '%1' tab: %2").arg(label, detail), ConsoleSeverity::Info);
    statusBar()->showMessage(detail, 2500);
}

bool EditorMainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_viewport && event && event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->isAutoRepeat())
        {
            return false;
        }

        const bool snapping = m_snapToggleAction ? m_snapToggleAction->isChecked() : false;
        const float moveStep = snapping ? m_snapTranslateStep : 0.05f;
        const float rotateStep = snapping ? m_snapRotateStep : 5.0f;
        const float scaleStep = snapping ? m_snapScaleStep : 0.01f;

        switch (keyEvent->key())
        {
        case Qt::Key_W:
            if (m_gizmoTranslateAction)
            {
                m_gizmoTranslateAction->trigger();
            }
            return true;
        case Qt::Key_E:
            if (m_gizmoRotateAction)
            {
                m_gizmoRotateAction->trigger();
            }
            return true;
        case Qt::Key_R:
            if (m_gizmoScaleAction)
            {
                m_gizmoScaleAction->trigger();
            }
            return true;
        case Qt::Key_Left:
            if (m_gizmoMode == GizmoMode::Translate)
            {
                ApplyTranslationDelta(-moveStep, 0.0f);
            }
            else if (m_gizmoMode == GizmoMode::Rotate)
            {
                ApplyRotationDelta(-rotateStep);
            }
            else if (m_gizmoMode == GizmoMode::Scale)
            {
                ApplyScaleDelta(-scaleStep);
            }
            return true;
        case Qt::Key_Right:
            if (m_gizmoMode == GizmoMode::Translate)
            {
                ApplyTranslationDelta(moveStep, 0.0f);
            }
            else if (m_gizmoMode == GizmoMode::Rotate)
            {
                ApplyRotationDelta(rotateStep);
            }
            else if (m_gizmoMode == GizmoMode::Scale)
            {
                ApplyScaleDelta(scaleStep);
            }
            return true;
        case Qt::Key_Up:
            if (m_gizmoMode == GizmoMode::Translate)
            {
                ApplyTranslationDelta(0.0f, moveStep);
            }
            else if (m_gizmoMode == GizmoMode::Scale)
            {
                ApplyScaleDelta(scaleStep);
            }
            return true;
        case Qt::Key_Down:
            if (m_gizmoMode == GizmoMode::Translate)
            {
                ApplyTranslationDelta(0.0f, -moveStep);
            }
            else if (m_gizmoMode == GizmoMode::Scale)
            {
                ApplyScaleDelta(-scaleStep);
            }
            return true;
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void EditorMainWindow::ApplyTranslationDelta(float dx, float dy)
{
    if (!m_selection)
    {
        return;
    }

    auto entity = m_selection->GetSelectedEntity();
    if (!entity)
    {
        return;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        return;
    }

    transform->SetPosition(transform->GetPositionX() + dx, transform->GetPositionY() + dy);
    RefreshRenderView();
    RefreshSelectedEntityUi();
}

void EditorMainWindow::ApplyRotationDelta(float deltaDeg)
{
    if (!m_selection)
    {
        return;
    }

    auto entity = m_selection->GetSelectedEntity();
    if (!entity)
    {
        return;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        return;
    }

    transform->SetRotationZDegrees(transform->GetRotationZDegrees() + deltaDeg);
    RefreshRenderView();
    RefreshSelectedEntityUi();
}

void EditorMainWindow::ApplyScaleDelta(float deltaUniform)
{
    if (!m_selection)
    {
        return;
    }

    auto entity = m_selection->GetSelectedEntity();
    if (!entity)
    {
        return;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        return;
    }

    const float newScaleX = std::max(0.001f, transform->GetScaleX() + deltaUniform);
    const float newScaleY = std::max(0.001f, transform->GetScaleY() + deltaUniform);
    transform->SetScale(newScaleX, newScaleY);
    RefreshRenderView();
    RefreshSelectedEntityUi();
}

void EditorMainWindow::RefreshSelectedEntityUi()
{
    if (m_inspectorPanel)
    {
        m_inspectorPanel->SetSelectedEntity(m_selection ? m_selection->GetSelectedEntity() : nullptr);
    }
    if (m_hierarchyPanel && m_selection)
    {
        auto entity = m_selection->GetSelectedEntity();
        if (entity)
        {
            m_hierarchyPanel->SetSelectedEntity(entity->GetId());
        }
    }
}
} // namespace Aetherion::Editor
