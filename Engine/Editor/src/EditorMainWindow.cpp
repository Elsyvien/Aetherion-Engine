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
#include "Aetherion/Editor/EditorInspectorPanel.h"
#include "Aetherion/Editor/EditorViewport.h"
#include "Aetherion/Rendering/VulkanViewport.h"
#include "Aetherion/Runtime/EngineApplication.h"

namespace Aetherion::Editor
{
EditorMainWindow::EditorMainWindow(std::shared_ptr<Runtime::EngineApplication> runtimeApp, QWidget* parent)
    : QMainWindow(parent)
    , m_runtimeApp(std::move(runtimeApp))
{
    setWindowTitle("Aetherion Editor");
    resize(1440, 900);

    auto* centerSplit = new QSplitter(Qt::Horizontal, this);
    centerSplit->setChildrenCollapsible(false);

    m_viewport = new EditorViewport(centerSplit);
    centerSplit->addWidget(m_viewport);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(16);
    connect(m_renderTimer, &QTimer::timeout, this, [this] {
        if (m_vulkanViewport && m_vulkanViewport->IsReady())
        {
            m_vulkanViewport->RenderFrame();
        }
    });

    connect(m_viewport, &EditorViewport::surfaceReady, this, [this](WId nativeHandle, int width, int height) {
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
        if (!vk)
        {
            return;
        }

        m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk);
        m_vulkanViewport->Initialize(reinterpret_cast<void*>(nativeHandle), width, height);
        m_renderTimer->start();
        statusBar()->showMessage(tr("Viewport Vulkan renderer active"));
    });

    connect(m_viewport, &EditorViewport::surfaceResized, this, [this](int width, int height) {
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
