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
#include <utility>

#include "Aetherion/Editor/EditorAssetBrowser.h"
#include "Aetherion/Editor/EditorConsole.h"
#include "Aetherion/Editor/EditorHierarchyPanel.h"
#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Editor/EditorInspectorPanel.h"
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
EditorMainWindow::EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp, QWidget* parent)
    : QMainWindow(parent)
    , m_runtimeApp(std::move(runtimeApp))
{
    m_validationEnabled = m_runtimeApp ? m_runtimeApp->IsValidationEnabled() : true;
    m_selection = new EditorSelection(this);

    setWindowTitle("Aetherion Editor");
    resize(1440, 900);

    auto* centerSplit = new QSplitter(Qt::Horizontal, this);
    centerSplit->setChildrenCollapsible(false);

    m_viewport = new EditorViewport(centerSplit);
    centerSplit->addWidget(m_viewport);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(16);
    connect(m_renderTimer, &QTimer::timeout, this, [this] {
        if (!m_vulkanViewport || !m_vulkanViewport->IsReady())
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
        m_vulkanViewport->Initialize(reinterpret_cast<void*>(nativeHandle), width, height);

        m_frameTimer.start();
        m_renderTimer->start();
        statusBar()->showMessage(tr("Viewport Vulkan renderer active"));
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
    editMenu->addAction(tr("Preferences"), [] {});
    auto* validationAction = editMenu->addAction(tr("Enable Vulkan Validation Layers"));
    validationAction->setCheckable(true);
    validationAction->setChecked(m_validationEnabled);
    connect(validationAction, &QAction::toggled, this, [this](bool enabled) {
        if (enabled == m_validationEnabled)
        {
            return;
        }
        RecreateRuntimeAndRenderer(enabled);
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

void EditorMainWindow::RefreshRenderView()
{
    m_renderView.instances.clear();

    if (!m_scene)
    {
        return;
    }

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
    }
}

void EditorMainWindow::RecreateRuntimeAndRenderer(bool enableValidation)
{
    m_validationEnabled = enableValidation;

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
    m_runtimeApp->Initialize(m_validationEnabled);

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
            m_vulkanViewport->Initialize(reinterpret_cast<void*>(m_surfaceHandle),
                                         m_surfaceSize.width(),
                                         m_surfaceSize.height());
            m_frameTimer.restart();
            m_renderTimer->start();
        }
    }

    RefreshRenderView();
    statusBar()->showMessage(tr("Renderer reset (%1 validation)")
                                 .arg(m_validationEnabled ? tr("with") : tr("without")));
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
