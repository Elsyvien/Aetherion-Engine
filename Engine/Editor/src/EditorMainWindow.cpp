#include "Aetherion/Editor/EditorMainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QSysInfo>
#include <QCloseEvent>
#include <QActionGroup>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Aetherion/Editor/EditorAssetBrowser.h"
#include "Aetherion/Editor/EditorConsole.h"
#include "Aetherion/Editor/EditorHierarchyPanel.h"
#include "Aetherion/Editor/EditorMeshPreview.h"
#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Editor/EditorInspectorPanel.h"
#include "Aetherion/Editor/EditorSettingsDialog.h"
#include "Aetherion/Editor/EditorViewport.h"
#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Rendering/VulkanViewport.h"
#include "Aetherion/Runtime/EngineApplication.h"

#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/SceneSerializer.h"
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

void Mat4Translation(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
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

void Mat4Scale(float out[16], float x, float y, float z)
{
    Mat4Identity(out);
    out[0] = x;
    out[5] = y;
    out[10] = z;
}

std::array<float, 16> BuildLocalMatrix(const Scene::TransformComponent& transform)
{
    float t[16];
    float r[16];
    float s[16];
    float tr[16];
    float local[16];
    Mat4Translation(t, transform.GetPositionX(), transform.GetPositionY(), 0.0f);
    Mat4RotationZ(r, transform.GetRotationZDegrees() * (3.14159265358979323846f / 180.0f));
    Mat4Scale(s, transform.GetScaleX(), transform.GetScaleY(), 1.0f);
    Mat4Mul(tr, t, r);
    Mat4Mul(local, tr, s);
    std::array<float, 16> out{};
    std::memcpy(out.data(), local, sizeof(local));
    return out;
}

std::array<float, 16> GetWorldMatrix(const Scene::Scene& scene, Core::EntityId id)
{
    auto entity = scene.FindEntityById(id);
    if (!entity)
    {
        std::array<float, 16> identity{};
        Mat4Identity(identity.data());
        return identity;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        std::array<float, 16> identity{};
        Mat4Identity(identity.data());
        return identity;
    }

    auto local = BuildLocalMatrix(*transform);
    if (!transform->HasParent())
    {
        return local;
    }

    auto parent = GetWorldMatrix(scene, transform->GetParentId());
    float world[16];
    Mat4Mul(world, parent.data(), local.data());
    std::array<float, 16> out{};
    std::memcpy(out.data(), world, sizeof(world));
    return out;
}

std::array<float, 3> TransformPoint(const std::array<float, 16>& m, const std::array<float, 3>& p)
{
    std::array<float, 3> out{};
    out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
    return out;
}

float ExtractMaxScale(const std::array<float, 16>& m)
{
    const float sx = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
    const float sy = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
    const float sz = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
    return std::max(sx, std::max(sy, sz));
}


std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec)
    {
        return canonical;
    }
    return path.lexically_normal();
}

bool PathStartsWith(const std::filesystem::path& path, const std::filesystem::path& prefix)
{
    auto pathIt = path.begin();
    auto prefixIt = prefix.begin();
    for (; prefixIt != prefix.end(); ++prefixIt, ++pathIt)
    {
        if (pathIt == path.end() || *pathIt != *prefixIt)
        {
            return false;
        }
    }
    return true;
}

std::filesystem::path MakeUniquePath(const std::filesystem::path& base)
{
    std::error_code ec;
    if (!std::filesystem::exists(base, ec))
    {
        return base;
    }

    const auto dir = base.parent_path();
    const auto stem = base.stem().string();
    const auto ext = base.extension().string();
    for (int i = 1; i < 1000; ++i)
    {
        auto candidate = dir / (stem + "_" + std::to_string(i) + ext);
        if (!std::filesystem::exists(candidate, ec))
        {
            return candidate;
        }
    }

    return base;
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
        if (m_runtimeApp)
        {
            m_runtimeApp->Tick();
        }
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
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto renderView = ctx ? ctx->GetRenderView() : nullptr;
        const Rendering::RenderView* activeView = renderView.get();
        Rendering::RenderView emptyView{};
        if (!activeView)
        {
            activeView = &emptyView;
        }
        if (renderView)
        {
            renderView->selectedEntityId =
                (m_selection && m_selection->GetSelectedEntity()) ? m_selection->GetSelectedEntity()->GetId()
                                                                  : 0;
        }
        try
        {
            m_vulkanViewport->RenderFrame(dt, *activeView);
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
            auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
            m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk, registry);
            m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
            m_vulkanViewport->Initialize(reinterpret_cast<void*>(nativeHandle), width, height);
            
            // Sync camera from viewport widget
            if (m_viewport)
            {
                m_vulkanViewport->SetCameraPosition(
                    m_viewport->getCameraX(),
                    m_viewport->getCameraY(),
                    m_viewport->getCameraZ());
                m_vulkanViewport->SetCameraRotation(
                    m_viewport->getCameraRotationY(),
                    m_viewport->getCameraRotationX());
                m_vulkanViewport->SetCameraZoom(m_viewport->getCameraZoom());
            }
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

            // Initialize mesh preview with shared Vulkan context
            if (m_meshPreview)
            {
                auto previewCtx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
                auto previewVk = previewCtx ? previewCtx->GetVulkanContext() : nullptr;
                auto previewRegistry = previewCtx ? previewCtx->GetAssetRegistry() : nullptr;
                m_meshPreview->SetVulkanContext(previewVk);
                m_meshPreview->SetAssetRegistry(previewRegistry);
            }
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

    // Connect camera changes from viewport to renderer
    connect(m_viewport, &EditorViewport::cameraChanged, this, [this]() {
        if (m_vulkanViewport)
        {
            m_vulkanViewport->SetCameraPosition(
                m_viewport->getCameraX(),
                m_viewport->getCameraY(),
                m_viewport->getCameraZ());
            m_vulkanViewport->SetCameraRotation(
                m_viewport->getCameraRotationY(),
                m_viewport->getCameraRotationX());
            m_vulkanViewport->SetCameraZoom(m_viewport->getCameraZoom());
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
    m_scenePath = GetDefaultScenePath();
    UpdateWindowTitle();
    if (m_inspectorPanel)
    {
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        m_inspectorPanel->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
    }

    AttachVulkanLogSink();
    RefreshAssetBrowser();

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

                    SetSceneDirty(true);

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
        
        // Connect hierarchy context menu actions
        connect(m_hierarchyPanel, &EditorHierarchyPanel::entityDeleteRequested, this, &EditorMainWindow::DeleteEntity);
        connect(m_hierarchyPanel, &EditorHierarchyPanel::entityDuplicateRequested, this, &EditorMainWindow::DuplicateEntity);
        connect(m_hierarchyPanel, &EditorHierarchyPanel::entityRenameRequested, this, &EditorMainWindow::RenameEntity);
        connect(m_hierarchyPanel, &EditorHierarchyPanel::createEmptyEntityRequested, this, &EditorMainWindow::CreateEmptyEntity);
        connect(m_hierarchyPanel, &EditorHierarchyPanel::createEmptyEntityAtRootRequested, this, [this]() {
            CreateEmptyEntity(0);
        });
    }

    if (m_inspectorPanel)
    {
        connect(m_inspectorPanel, &EditorInspectorPanel::sceneModified, this, [this] {
            SetSceneDirty(true);
        });
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
            // Update mesh preview if it's a mesh asset
            if (m_meshPreview)
            {
                auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
                auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
                if (registry)
                {
                    const auto* entry = registry->FindEntry(assetId.toStdString());
                    if (entry && entry->type == Assets::AssetRegistry::AssetType::Mesh)
                    {
                        m_meshPreview->SetMeshAsset(assetId);
                    }
                    else
                    {
                        m_meshPreview->ClearPreview();
                    }
                }
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
            if (m_meshPreview)
            {
                m_meshPreview->ClearPreview();
            }
        });
        connect(m_assetBrowser, &EditorAssetBrowser::AssetActivated, this, [this] {
            if (m_inspectorDock)
            {
                m_inspectorDock->show();
                m_inspectorDock->raise();
            }
        });
        connect(m_assetBrowser, &EditorAssetBrowser::RescanRequested, this, &EditorMainWindow::RescanAssets);
        connect(m_assetBrowser, &EditorAssetBrowser::AssetDroppedOnScene, this, &EditorMainWindow::AddAssetToScene);
        connect(m_assetBrowser, &EditorAssetBrowser::AssetDeleteRequested, this, &EditorMainWindow::DeleteAsset);
        connect(m_assetBrowser, &EditorAssetBrowser::AssetRenameRequested, this, &EditorMainWindow::RenameAsset);
        connect(m_assetBrowser, &EditorAssetBrowser::AssetShowInExplorerRequested, this, &EditorMainWindow::ShowAssetInExplorer);
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
    auto* importGltf = fileMenu->addAction(tr("Import glTF..."));
    connect(importGltf, &QAction::triggered, this, &EditorMainWindow::ImportGltfAsset);
    auto* saveScene = fileMenu->addAction(tr("Save Scene"));
    saveScene->setShortcut(QKeySequence::Save);
    connect(saveScene, &QAction::triggered, this, &EditorMainWindow::SaveScene);
    auto* reloadScene = fileMenu->addAction(tr("Reload Scene"));
    reloadScene->setShortcut(QKeySequence::Refresh);
    connect(reloadScene, &QAction::triggered, this, &EditorMainWindow::ReloadScene);
    auto* rescanAssets = fileMenu->addAction(tr("Rescan Assets"));
    rescanAssets->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));
    connect(rescanAssets, &QAction::triggered, this, &EditorMainWindow::RescanAssets);
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

    m_showMeshPreviewAction = viewMenu->addAction(tr("Show Mesh Preview"));
    m_showMeshPreviewAction->setCheckable(true);
    m_showMeshPreviewAction->setChecked(true);
    connect(m_showMeshPreviewAction, &QAction::triggered, this, [this](bool checked) {
        if (m_meshPreviewDock)
        {
            m_meshPreviewDock->setVisible(checked);
            if (checked)
            {
                m_meshPreviewDock->raise();
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

    auto* resetCameraAction = viewMenu->addAction(tr("Reset Camera"));
    resetCameraAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(resetCameraAction, &QAction::triggered, this, [this] {
        if (m_viewport)
        {
            m_viewport->resetCamera();
        }
        if (m_vulkanViewport)
        {
            m_vulkanViewport->ResetCamera();
        }
        statusBar()->showMessage(tr("Camera reset"), 2000);
    });

    auto* focusOnSelectionAction = viewMenu->addAction(tr("Focus on Selection"));
    focusOnSelectionAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(focusOnSelectionAction, &QAction::triggered, this, [this] {
        FocusCameraOnSelection();
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

void EditorMainWindow::RefreshAssetBrowser()
{
    if (!m_assetBrowser)
    {
        return;
    }

    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;

    std::vector<EditorAssetBrowser::Item> items;
    if (!registry)
    {
        const QString label = tr("Assets unavailable");
        items.push_back({label, label, true});
        m_assetBrowser->SetItems(items);
        return;
    }

    const auto& entries = registry->GetEntries();
    struct Category
    {
        Assets::AssetRegistry::AssetType type;
        const char* label;
    };

    const Category categories[] = {
        {Assets::AssetRegistry::AssetType::Texture, "Textures/"},
        {Assets::AssetRegistry::AssetType::Mesh, "Meshes/"},
        {Assets::AssetRegistry::AssetType::Audio, "Audio/"},
        {Assets::AssetRegistry::AssetType::Script, "Scripts/"},
        {Assets::AssetRegistry::AssetType::Scene, "Scenes/"},
        {Assets::AssetRegistry::AssetType::Shader, "Shaders/"},
        {Assets::AssetRegistry::AssetType::Other, "Misc/"}
    };

    const size_t categoryCount = sizeof(categories) / sizeof(categories[0]);
    std::vector<std::vector<const Assets::AssetRegistry::AssetEntry*>> grouped(categoryCount);
    for (const auto& entry : entries)
    {
        for (size_t i = 0; i < categoryCount; ++i)
        {
            if (entry.type == categories[i].type)
            {
                grouped[i].push_back(&entry);
                break;
            }
        }
    }

    for (size_t i = 0; i < categoryCount; ++i)
    {
        const QString header = tr(categories[i].label);
        items.push_back({header, header, true});

        auto& list = grouped[i];
        std::sort(list.begin(), list.end(), [](const auto* lhs, const auto* rhs) {
            return lhs->id < rhs->id;
        });

        for (const auto* entry : list)
        {
            const QString id = QString::fromStdString(entry->id);
            items.push_back({QString("  %1").arg(id), id, false});
        }
    }

    m_assetBrowser->SetItems(items);
}

void EditorMainWindow::RescanAssets()
{
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    if (!registry)
    {
        statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
        return;
    }

    std::filesystem::path root = registry->GetRootPath();
    if (root.empty())
    {
        root = std::filesystem::path("assets");
    }
    registry->Scan(root.string());
    RefreshAssetBrowser();
    if (m_inspectorPanel)
    {
        m_inspectorPanel->SetAssetRegistry(registry);
    }
    statusBar()->showMessage(tr("Assets rescanned"), 2000);
}

void EditorMainWindow::ImportGltfAsset()
{
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    if (!registry)
    {
        statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
        return;
    }

    const QString filter = tr("glTF (*.gltf *.glb)");
    const QString selected = QFileDialog::getOpenFileName(this, tr("Import glTF"), QString(), filter);
    if (selected.isEmpty())
    {
        return;
    }

    std::filesystem::path sourcePath = selected.toStdString();
    std::filesystem::path rootPath = registry->GetRootPath();
    if (rootPath.empty())
    {
        rootPath = std::filesystem::path("assets");
    }

    std::error_code ec;
    std::filesystem::create_directories(rootPath, ec);
    if (ec)
    {
        statusBar()->showMessage(tr("Failed to prepare assets folder"), 2000);
        return;
    }

    std::filesystem::path normalizedSource = NormalizePath(sourcePath);
    std::filesystem::path normalizedRoot = NormalizePath(rootPath);
    bool insideRoot = PathStartsWith(normalizedSource, normalizedRoot);
    std::filesystem::path importPath = sourcePath;

    if (!insideRoot)
    {
        const auto choice = QMessageBox::question(
            this,
            tr("Import glTF"),
            tr("Selected file is outside the assets folder. Copy into assets/meshes and import?\n\nNote: external dependencies are not copied yet."),
            QMessageBox::Yes | QMessageBox::Cancel);
        if (choice != QMessageBox::Yes)
        {
            return;
        }

        std::filesystem::path destDir = normalizedRoot / "meshes";
        std::filesystem::create_directories(destDir, ec);
        if (ec)
        {
            statusBar()->showMessage(tr("Failed to create assets/meshes"), 2000);
            return;
        }

        importPath = MakeUniquePath(destDir / sourcePath.filename());
        std::filesystem::copy_file(sourcePath, importPath, std::filesystem::copy_options::none, ec);
        if (ec)
        {
            statusBar()->showMessage(tr("Failed to copy glTF"), 2000);
            return;
        }
    }

    const auto result = registry->ImportGltf(importPath.string());
    if (!result.success)
    {
        const QString message = tr("GLTF import failed: %1").arg(QString::fromStdString(result.message));
        AppendConsole(m_console, message, ConsoleSeverity::Error);
        statusBar()->showMessage(message, 3000);
        return;
    }

    registry->Scan(rootPath.string());
    RefreshAssetBrowser();
    if (m_inspectorPanel)
    {
        m_inspectorPanel->SetAssetRegistry(registry);
    }

    const QString success = tr("Imported glTF: %1").arg(QString::fromStdString(result.id));
    AppendConsole(m_console, success, ConsoleSeverity::Info);
    statusBar()->showMessage(success, 3000);
}

void EditorMainWindow::AddAssetToScene(const QString& assetId)
{
    if (assetId.isEmpty() || !m_scene)
    {
        statusBar()->showMessage(tr("Cannot add asset: no active scene"), 2000);
        return;
    }

    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    if (!registry)
    {
        statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
        return;
    }

    const std::string idStr = assetId.toStdString();
    const auto* entry = registry->FindEntry(idStr);
    if (!entry)
    {
        statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
        return;
    }

    // Only mesh assets can be added to the scene directly
    if (entry->type != Assets::AssetRegistry::AssetType::Mesh)
    {
        statusBar()->showMessage(tr("Only mesh assets can be added to scene"), 2000);
        return;
    }

    // Generate a unique entity ID
    Core::EntityId newId = 1;
    for (const auto& entity : m_scene->GetEntities())
    {
        if (entity && entity->GetId() >= newId)
        {
            newId = entity->GetId() + 1;
        }
    }

    // Create the entity name from asset path
    std::filesystem::path assetPath(idStr);
    std::string entityName = assetPath.stem().string();
    if (entityName.empty())
    {
        entityName = "New Entity";
    }

    // Create entity with transform and mesh renderer
    auto newEntity = std::make_shared<Scene::Entity>(newId, entityName);
    
    auto transform = std::make_shared<Scene::TransformComponent>();
    transform->SetPosition(0.0f, 0.0f);
    transform->SetScale(1.0f, 1.0f);
    
    auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
    meshRenderer->SetMeshAssetId(idStr);
    meshRenderer->SetColor(1.0f, 1.0f, 1.0f);
    {
        const std::string stem = std::filesystem::path(idStr).stem().string();
        if (const auto* cached = registry->GetMesh(stem); cached && !cached->textureIds.empty())
        {
            meshRenderer->SetAlbedoTextureId(cached->textureIds.front());
        }
    }
    
    newEntity->AddComponent(transform);
    newEntity->AddComponent(meshRenderer);
    
    m_scene->AddEntity(newEntity);

    // Update UI
    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->BindScene(m_scene);
        m_hierarchyPanel->SetSelectedEntity(newId);
    }
    
    if (m_selection)
    {
        m_selection->SelectEntity(newEntity);
    }

    SetSceneDirty(true);

    const QString msg = tr("Added '%1' to scene as entity '%2'").arg(assetId).arg(QString::fromStdString(entityName));
    AppendConsole(m_console, msg, ConsoleSeverity::Info);
    statusBar()->showMessage(msg, 3000);

    FocusCameraOnSelection();
}

void EditorMainWindow::DeleteAsset(const QString& assetId)
{
    if (assetId.isEmpty())
    {
        return;
    }

    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    if (!registry)
    {
        statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
        return;
    }

    const std::string idStr = assetId.toStdString();
    const auto* entry = registry->FindEntry(idStr);
    if (!entry)
    {
        statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
        return;
    }

    // Confirm deletion
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Delete Asset"),
        tr("Are you sure you want to delete '%1'?\n\nThis will remove the file from disk.").arg(assetId),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    // Delete the file
    std::error_code ec;
    if (std::filesystem::exists(entry->path, ec) && std::filesystem::remove(entry->path, ec))
    {
        AppendConsole(m_console, tr("Deleted asset: %1").arg(assetId), ConsoleSeverity::Info);
        statusBar()->showMessage(tr("Asset deleted: %1").arg(assetId), 3000);
        RescanAssets();
    }
    else
    {
        AppendConsole(m_console, tr("Failed to delete asset: %1").arg(assetId), ConsoleSeverity::Error);
        statusBar()->showMessage(tr("Failed to delete asset"), 2000);
    }
}

void EditorMainWindow::RenameAsset(const QString& assetId)
{
    if (assetId.isEmpty())
    {
        return;
    }

    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    if (!registry)
    {
        statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
        return;
    }

    const std::string idStr = assetId.toStdString();
    const auto* entry = registry->FindEntry(idStr);
    if (!entry)
    {
        statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
        return;
    }

    std::filesystem::path oldPath(entry->path);
    QString oldName = QString::fromStdString(oldPath.stem().string());
    QString extension = QString::fromStdString(oldPath.extension().string());

    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        tr("Rename Asset"),
        tr("Enter new name:"),
        QLineEdit::Normal,
        oldName,
        &ok);

    if (!ok || newName.isEmpty() || newName == oldName)
    {
        return;
    }

    std::filesystem::path newPath = oldPath.parent_path() / (newName.toStdString() + extension.toStdString());

    std::error_code ec;
    if (std::filesystem::exists(newPath, ec))
    {
        QMessageBox::warning(this, tr("Rename Failed"), tr("A file with that name already exists."));
        return;
    }

    std::filesystem::rename(oldPath, newPath, ec);
    if (ec)
    {
        AppendConsole(m_console, tr("Failed to rename asset: %1").arg(QString::fromStdString(ec.message())), ConsoleSeverity::Error);
        return;
    }

    AppendConsole(m_console, tr("Renamed asset: %1 -> %2").arg(oldName).arg(newName), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Asset renamed"), 3000);
    RescanAssets();
}

void EditorMainWindow::ShowAssetInExplorer(const QString& assetId)
{
    if (assetId.isEmpty())
    {
        return;
    }

    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    if (!registry)
    {
        statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
        return;
    }

    const std::string idStr = assetId.toStdString();
    const auto* entry = registry->FindEntry(idStr);
    if (!entry)
    {
        statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
        return;
    }

    QString filePath = QString::fromStdString(entry->path.string());
    
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(filePath)});
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", {"-R", filePath});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(entry->path.parent_path().string())));
#endif
}

void EditorMainWindow::DeleteEntity(Aetherion::Core::EntityId id)
{
    if (id == 0 || !m_scene)
    {
        return;
    }

    auto entity = m_scene->GetEntityById(id);
    if (!entity)
    {
        statusBar()->showMessage(tr("Entity not found"), 2000);
        return;
    }

    QString entityName = QString::fromStdString(entity->GetName());

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Delete Entity"),
        tr("Are you sure you want to delete '%1'?").arg(entityName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    m_scene->RemoveEntity(id);
    SetSceneDirty(true);

    if (m_selection)
    {
        m_selection->Clear();
    }

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->BindScene(m_scene);
    }

    AppendConsole(m_console, tr("Deleted entity: %1").arg(entityName), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Entity deleted: %1").arg(entityName), 3000);
}

void EditorMainWindow::DuplicateEntity(Aetherion::Core::EntityId id)
{
    if (id == 0 || !m_scene)
    {
        return;
    }

    auto sourceEntity = m_scene->GetEntityById(id);
    if (!sourceEntity)
    {
        statusBar()->showMessage(tr("Entity not found"), 2000);
        return;
    }

    // Generate a unique entity ID
    Core::EntityId newId = 1;
    for (const auto& entity : m_scene->GetEntities())
    {
        if (entity && entity->GetId() >= newId)
        {
            newId = entity->GetId() + 1;
        }
    }

    std::string newName = sourceEntity->GetName() + " (Copy)";
    auto newEntity = std::make_shared<Scene::Entity>(newId, newName);

    // Copy transform component
    auto sourceTransform = sourceEntity->GetComponent<Scene::TransformComponent>();
    if (sourceTransform)
    {
        auto transform = std::make_shared<Scene::TransformComponent>();
        float px = sourceTransform->GetPositionX();
        float py = sourceTransform->GetPositionY();
        transform->SetPosition(px + 0.5f, py);  // Offset slightly
        float sx = sourceTransform->GetScaleX();
        float sy = sourceTransform->GetScaleY();
        transform->SetScale(sx, sy);
        transform->SetRotationZDegrees(sourceTransform->GetRotationZDegrees());
        newEntity->AddComponent(transform);
    }

    // Copy mesh renderer component
    auto sourceMesh = sourceEntity->GetComponent<Scene::MeshRendererComponent>();
    if (sourceMesh)
    {
        auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
        meshRenderer->SetMeshAssetId(sourceMesh->GetMeshAssetId());
        auto [r, g, b] = sourceMesh->GetColor();
        meshRenderer->SetColor(r, g, b);
        newEntity->AddComponent(meshRenderer);
    }

    m_scene->AddEntity(newEntity);
    SetSceneDirty(true);

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->BindScene(m_scene);
        m_hierarchyPanel->SetSelectedEntity(newId);
    }

    if (m_selection)
    {
        m_selection->SelectEntity(newEntity);
    }

    AppendConsole(m_console, tr("Duplicated entity: %1").arg(QString::fromStdString(newName)), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Entity duplicated"), 3000);
}

void EditorMainWindow::RenameEntity(Aetherion::Core::EntityId id)
{
    if (id == 0 || !m_scene)
    {
        return;
    }

    auto entity = m_scene->GetEntityById(id);
    if (!entity)
    {
        statusBar()->showMessage(tr("Entity not found"), 2000);
        return;
    }

    QString oldName = QString::fromStdString(entity->GetName());

    bool ok = false;
    QString newName = QInputDialog::getText(
        this,
        tr("Rename Entity"),
        tr("Enter new name:"),
        QLineEdit::Normal,
        oldName,
        &ok);

    if (!ok || newName.isEmpty() || newName == oldName)
    {
        return;
    }

    entity->SetName(newName.toStdString());
    SetSceneDirty(true);

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->BindScene(m_scene);
        m_hierarchyPanel->SetSelectedEntity(id);
    }

    AppendConsole(m_console, tr("Renamed entity: %1 -> %2").arg(oldName).arg(newName), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Entity renamed"), 3000);
}

void EditorMainWindow::CreateEmptyEntity(Aetherion::Core::EntityId parentId)
{
    if (!m_scene)
    {
        statusBar()->showMessage(tr("No active scene"), 2000);
        return;
    }

    // Generate a unique entity ID
    Core::EntityId newId = 1;
    for (const auto& entity : m_scene->GetEntities())
    {
        if (entity && entity->GetId() >= newId)
        {
            newId = entity->GetId() + 1;
        }
    }

    auto newEntity = std::make_shared<Scene::Entity>(newId, "New Entity");

    auto transform = std::make_shared<Scene::TransformComponent>();
    transform->SetPosition(0.0f, 0.0f);
    transform->SetScale(1.0f, 1.0f);
    
    if (parentId != 0)
    {
        transform->SetParent(parentId);
    }

    newEntity->AddComponent(transform);
    m_scene->AddEntity(newEntity);
    SetSceneDirty(true);

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->BindScene(m_scene);
        m_hierarchyPanel->SetSelectedEntity(newId);
    }

    if (m_selection)
    {
        m_selection->SelectEntity(newEntity);
    }

    AppendConsole(m_console, tr("Created new entity"), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Entity created"), 3000);
}

void EditorMainWindow::SaveScene()
{
    if (m_scenePath.empty())
    {
        m_scenePath = GetDefaultScenePath();
    }
    SaveSceneToPath(m_scenePath);
}

void EditorMainWindow::ReloadScene()
{
    if (!ConfirmSaveIfDirty())
    {
        return;
    }
    if (m_scenePath.empty())
    {
        m_scenePath = GetDefaultScenePath();
    }
    LoadSceneFromPath(m_scenePath);
}

bool EditorMainWindow::ConfirmSaveIfDirty()
{
    if (!m_sceneDirty)
    {
        return true;
    }

    const auto choice = QMessageBox::question(this,
                                              tr("Unsaved Changes"),
                                              tr("The current scene has unsaved changes. Save before continuing?"),
                                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (choice == QMessageBox::Cancel)
    {
        return false;
    }
    if (choice == QMessageBox::Yes)
    {
        return SaveSceneToPath(m_scenePath);
    }
    return true;
}

bool EditorMainWindow::SaveSceneToPath(const std::filesystem::path& path)
{
    if (!m_scene || !m_runtimeApp)
    {
        statusBar()->showMessage(tr("No scene to save"), 2000);
        return false;
    }

    auto ctx = m_runtimeApp->GetContext();
    if (!ctx)
    {
        statusBar()->showMessage(tr("Runtime context unavailable"), 2000);
        return false;
    }

    const std::filesystem::path target = path.empty() ? GetDefaultScenePath() : path;
    Scene::SceneSerializer serializer(*ctx);
    if (!serializer.Save(*m_scene, target))
    {
        statusBar()->showMessage(tr("Failed to save scene"), 2000);
        return false;
    }

    m_scenePath = target;
    SetSceneDirty(false);
    statusBar()->showMessage(tr("Scene saved"), 2000);
    return true;
}

bool EditorMainWindow::LoadSceneFromPath(const std::filesystem::path& path)
{
    if (!m_runtimeApp)
    {
        statusBar()->showMessage(tr("Runtime unavailable"), 2000);
        return false;
    }

    auto ctx = m_runtimeApp->GetContext();
    if (!ctx)
    {
        statusBar()->showMessage(tr("Runtime context unavailable"), 2000);
        return false;
    }

    const std::filesystem::path target = path.empty() ? GetDefaultScenePath() : path;
    Scene::SceneSerializer serializer(*ctx);
    auto loaded = serializer.Load(target);
    if (!loaded)
    {
        statusBar()->showMessage(tr("Failed to load scene"), 2000);
        return false;
    }

    m_scenePath = target;
    m_scene = loaded;
    if (m_runtimeApp)
    {
        m_runtimeApp->SetActiveScene(m_scene);
    }

    if (m_selection)
    {
        m_selection->SetActiveScene(m_scene);
        if (!m_scene)
        {
            m_selection->Clear();
        }
    }

    if (m_hierarchyPanel)
    {
        m_hierarchyPanel->SetSelectionModel(m_selection);
        m_hierarchyPanel->BindScene(m_scene);
    }

    if (m_inspectorPanel)
    {
        m_inspectorPanel->SetSelectedEntity(m_selection ? m_selection->GetSelectedEntity() : nullptr);
    }

    SetSceneDirty(false);
    statusBar()->showMessage(tr("Scene loaded"), 2000);
    return true;
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
    m_scenePath = GetDefaultScenePath();
    SetSceneDirty(false);

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
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        m_inspectorPanel->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
    }

    AttachVulkanLogSink();
    RefreshAssetBrowser();

    if (m_surfaceInitialized && m_surfaceHandle != 0)
    {
        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
        if (vk)
        {
            try
            {
                DestroyViewportRenderer();
                auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
                m_vulkanViewport = std::make_unique<Rendering::VulkanViewport>(vk, registry);
                m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
                m_vulkanViewport->Initialize(reinterpret_cast<void*>(m_surfaceHandle),
                                             m_surfaceSize.width(),
                                             m_surfaceSize.height());
                
                // Sync camera from viewport widget
                if (m_viewport)
                {
                    m_vulkanViewport->SetCameraPosition(
                        m_viewport->getCameraX(),
                        m_viewport->getCameraY(),
                        m_viewport->getCameraZ());
                    m_vulkanViewport->SetCameraRotation(
                        m_viewport->getCameraRotationY(),
                        m_viewport->getCameraRotationX());
                    m_vulkanViewport->SetCameraZoom(m_viewport->getCameraZoom());
                }
                
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
    if (!ConfirmSaveIfDirty())
    {
        event->ignore();
        return;
    }
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

    // Mesh Preview panel (right side, below inspector)
    auto* meshPreviewDock = new QDockWidget(tr("Mesh Preview"), this);
    meshPreviewDock->setObjectName("MeshPreviewDock");
    meshPreviewDock->setAttribute(Qt::WA_NativeWindow, true);
    m_meshPreviewDock = meshPreviewDock;
    m_meshPreview = new EditorMeshPreview(meshPreviewDock);
    meshPreviewDock->setWidget(m_meshPreview);
    meshPreviewDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, meshPreviewDock);
    connect(meshPreviewDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (m_showMeshPreviewAction)
        {
            m_showMeshPreviewAction->blockSignals(true);
            m_showMeshPreviewAction->setChecked(visible);
            m_showMeshPreviewAction->blockSignals(false);
        }
    });

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

void EditorMainWindow::UpdateWindowTitle()
{
    QString title = tr("Aetherion Editor");
    if (m_scene)
    {
        const std::string name = m_scene->GetName().empty() ? std::string("Scene") : m_scene->GetName();
        title = tr("Aetherion Editor - %1").arg(QString::fromStdString(name));
    }
    if (m_sceneDirty)
    {
        title += tr(" *");
    }
    setWindowTitle(title);
}

void EditorMainWindow::SetSceneDirty(bool dirty)
{
    if (m_sceneDirty == dirty)
    {
        return;
    }
    m_sceneDirty = dirty;
    UpdateWindowTitle();
    if (dirty)
    {
        statusBar()->showMessage(tr("Scene modified"), 2000);
    }
}

std::filesystem::path EditorMainWindow::GetAssetsRootPath() const
{
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    auto root = registry ? registry->GetRootPath() : std::filesystem::path();
    if (root.empty())
    {
        root = std::filesystem::path("assets");
    }
    return root;
}

std::filesystem::path EditorMainWindow::GetDefaultScenePath() const
{
    return GetAssetsRootPath() / "scenes" / "bootstrap_scene.json";
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
    SetSceneDirty(true);
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
    SetSceneDirty(true);
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
    SetSceneDirty(true);
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

void EditorMainWindow::FocusCameraOnSelection()
{
    if (!m_selection)
    {
        statusBar()->showMessage(tr("No entity selected"), 2000);
        return;
    }

    auto entity = m_selection->GetSelectedEntity();
    if (!entity)
    {
        statusBar()->showMessage(tr("No entity selected"), 2000);
        return;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        statusBar()->showMessage(tr("Selected entity has no transform"), 2000);
        return;
    }

    float targetX = transform->GetPositionX();
    float targetY = transform->GetPositionY();
    float targetZ = 0.0f;
    float radius = 0.5f;

    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
    auto mesh = entity->GetComponent<Scene::MeshRendererComponent>();
    if (mesh && registry && !mesh->GetMeshAssetId().empty() && m_scene)
    {
        if (const auto* meshData = registry->LoadMeshData(mesh->GetMeshAssetId()))
        {
            const auto world = GetWorldMatrix(*m_scene, entity->GetId());
            const auto worldCenter = TransformPoint(world, meshData->boundsCenter);
            const float maxScale = ExtractMaxScale(world);
            targetX = worldCenter[0];
            targetY = worldCenter[1];
            targetZ = worldCenter[2];
            radius = std::max(meshData->boundsRadius * maxScale, 0.01f);
        }
    }

    if (m_viewport)
    {
        m_viewport->SetCameraTarget(targetX, targetY, targetZ);
    }
    if (m_vulkanViewport)
    {
        m_vulkanViewport->FocusOnBounds(targetX, targetY, targetZ, radius);
    }

    statusBar()->showMessage(tr("Focused on '%1'").arg(QString::fromStdString(entity->GetName())), 2000);
}
} // namespace Aetherion::Editor
