#include "Aetherion/Editor/EditorMainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>
#include <unordered_map>
#include <utility>

#include "Aetherion/Editor/EditorAssetBrowser.h"
#include "Aetherion/Editor/EditorConsole.h"
#include "Aetherion/Editor/EditorHierarchyPanel.h"
#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Editor/EditorInspectorPanel.h"
#include "Aetherion/Editor/EditorSettingsDialog.h"
#include "Aetherion/Editor/EditorViewport.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanViewport.h"
#include "Aetherion/Runtime/EngineApplication.h"

#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Editor
{
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
    resize(1440, 900);

    auto* centerSplit = new QSplitter(Qt::Horizontal, this);
    centerSplit->setChildrenCollapsible(false);

    m_viewport = new EditorViewport(centerSplit);
    centerSplit->addWidget(m_viewport);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(m_targetFrameIntervalMs);
    connect(m_renderTimer, &QTimer::timeout, this, [this] {
        const bool viewportReady = m_vulkanViewport && m_vulkanViewport->IsReady();
        UpdateRenderTimerInterval(viewportReady);
        if (!viewportReady)
        {
            return;
        }
        if (isMinimized())
        {
            return;
        }

        const qint64 nanos = m_frameTimer.isValid() ? m_frameTimer.nsecsElapsed() : 0;
        const float dt = static_cast<float>(nanos) / 1'000'000'000.0f;
        m_frameTimer.restart();
        RefreshRenderView();
        m_vulkanViewport->RenderFrame(dt, m_renderView);
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

        m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk);
        m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
        m_vulkanViewport->Initialize(reinterpret_cast<void*>(nativeHandle), width, height);

        if (m_vulkanViewport->IsReady())
        {
            m_frameTimer.start();
            m_renderTimer->start();
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
            m_vulkanViewport->Resize(width, height);
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
    m_defaultLayoutState = saveState();

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

    connect(m_selection, &EditorSelection::SelectionChanged, this, [this](Aetherion::Core::EntityId) {
        if (m_inspectorPanel)
        {
            m_inspectorPanel->SetSelectedEntity(m_selection->GetSelectedEntity());
        }
    });
    connect(m_selection, &EditorSelection::SelectionCleared, this, [this]() {
        if (m_inspectorPanel)
        {
            m_inspectorPanel->SetSelectedEntity(nullptr);
        }
    });
}

EditorMainWindow::~EditorMainWindow() = default;

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
    viewMenu->addAction(tr("Reset Layout"), [this] {
        // TODO: Support named layout presets and user-defined layouts.
        restoreState(m_defaultLayoutState);
    });

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("About Aetherion"), [] {});
}

void EditorMainWindow::CreateToolBarContent()
{
    auto* toolBar = addToolBar(tr("Main"));
    toolBar->setMovable(true);

    auto* playAction = toolBar->addAction(tr("Play"));
    auto* pauseAction = toolBar->addAction(tr("Pause"));
    auto* stepAction = toolBar->addAction(tr("Step"));

    playAction->setEnabled(false);
    pauseAction->setEnabled(false);
    stepAction->setEnabled(false);

    // TODO: Wire actions to runtime controls.
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
}

void EditorMainWindow::RecreateRuntimeAndRenderer(bool enableValidation)
{
    m_validationEnabled = enableValidation;
    m_settings.validationEnabled = enableValidation;

    if (m_renderTimer)
    {
        m_renderTimer->stop();
    }

    if (m_vulkanViewport)
    {
        m_vulkanViewport->Shutdown();
        m_vulkanViewport.reset();
    }

    if (m_runtimeApp)
    {
        m_runtimeApp->Shutdown();
    }

    m_runtimeApp = std::make_shared<Runtime::EngineApplication>();
    m_runtimeApp->Initialize(m_validationEnabled, m_renderLoggingEnabled);

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

    if (m_surfaceInitialized && m_surfaceHandle != 0)
    {
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
        if (vk)
        {
            m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk);
            m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
            m_vulkanViewport->Initialize(reinterpret_cast<void*>(m_surfaceHandle),
                                         m_surfaceSize.width(),
                                         m_surfaceSize.height());
            if (m_vulkanViewport->IsReady())
            {
                m_frameTimer.restart();
                m_renderTimer->start();
            }
        }
    }

    RefreshRenderView();
    UpdateRenderTimerInterval(m_vulkanViewport && m_vulkanViewport->IsReady());
    statusBar()->showMessage(tr("Renderer reset (%1 validation, %2 logging)")
                                 .arg(m_validationEnabled ? tr("with") : tr("without"))
                                 .arg(m_renderLoggingEnabled ? tr("verbose") : tr("minimal")));
}

void EditorMainWindow::CreateDockPanels()
{
    auto* hierarchyDock = new QDockWidget(tr("Hierarchy"), this);
    m_hierarchyPanel = new EditorHierarchyPanel(hierarchyDock);
    hierarchyDock->setWidget(m_hierarchyPanel);
    hierarchyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

    auto* inspectorDock = new QDockWidget(tr("Inspector"), this);
    m_inspectorPanel = new EditorInspectorPanel(inspectorDock);
    inspectorDock->setWidget(m_inspectorPanel);
    inspectorDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    auto* assetDock = new QDockWidget(tr("Asset Browser"), this);
    m_assetBrowser = new EditorAssetBrowser(assetDock);
    assetDock->setWidget(m_assetBrowser);
    assetDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, assetDock);

    auto* consoleDock = new QDockWidget(tr("Console"), this);
    m_console = new EditorConsole(consoleDock);
    consoleDock->setWidget(m_console);
    consoleDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

    tabifyDockWidget(assetDock, consoleDock);
    consoleDock->raise();

    // TODO: Persist dock layout between sessions.
}

void EditorMainWindow::ConfigureStatusBar()
{
    statusBar()->showMessage(tr("Aetherion scaffolding - runtime disconnected"));
    // TODO: Display play state, selection info, and performance metrics.
}
} // namespace Aetherion::Editor
