#include "Aetherion/Editor/EditorCameraPreview.h"

#include <QEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Rendering/VulkanViewport.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"

namespace Aetherion::Editor
{
class CameraPreviewSurface : public QWidget
{
public:
    explicit CameraPreviewSurface(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(100, 100);
        setFocusPolicy(Qt::StrongFocus);
    }
};

EditorCameraPreview::EditorCameraPreview(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_header = new QLabel(tr("Camera Preview"), this);
    layout->addWidget(m_header);

    m_statusLabel = new QLabel(tr("No camera selected"), this);
    m_statusLabel->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(m_statusLabel);

    m_viewportContainer = new QWidget(this);
    m_viewportContainer->setMinimumSize(150, 150);
    m_viewportContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* containerLayout = new QVBoxLayout(m_viewportContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    m_surface = new CameraPreviewSurface(m_viewportContainer);
    m_surface->installEventFilter(this);
    containerLayout->addWidget(m_surface, 1);

    layout->addWidget(m_viewportContainer, 1);

    setLayout(layout);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(33);
    connect(m_renderTimer, &QTimer::timeout, this, &EditorCameraPreview::onRenderFrame);

    QTimer::singleShot(100, this, &EditorCameraPreview::onSurfaceReady);
}

EditorCameraPreview::~EditorCameraPreview()
{
    shutdownRenderer();
}

void EditorCameraPreview::SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context)
{
    m_vulkanContext = std::move(context);
    if (m_surfaceReady && m_vulkanContext)
    {
        initializeRenderer();
    }
}

void EditorCameraPreview::SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry)
{
    m_assetRegistry = std::move(registry);
}

void EditorCameraPreview::SetRenderViewSource(std::shared_ptr<Rendering::RenderView> view)
{
    m_renderView = view;
    UpdateStatusLabel();
}

void EditorCameraPreview::SetScene(std::shared_ptr<Scene::Scene> scene)
{
    m_scene = std::move(scene);
    UpdateStatusLabel();
}

void EditorCameraPreview::SetSelectedCameraId(Core::EntityId id)
{
    if (m_selectedCameraId == id)
    {
        return;
    }
    m_selectedCameraId = id;
    UpdateStatusLabel();
}

void EditorCameraPreview::ClearPreview()
{
    m_selectedCameraId = 0;
    UpdateStatusLabel();
}

void EditorCameraPreview::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (m_viewport && m_surface)
    {
        const QSize size = m_surface->size();
        if (!size.isEmpty())
        {
            m_viewport->Resize(size.width(), size.height());
        }
    }
}

bool EditorCameraPreview::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_surface)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::Wheel:
            return true;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void EditorCameraPreview::onSurfaceReady()
{
    if (!m_surface)
    {
        return;
    }

    m_surfaceHandle = m_surface->winId();
    m_surfaceSize = m_surface->size();
    m_surfaceReady = true;

    if (m_vulkanContext)
    {
        initializeRenderer();
    }
}

void EditorCameraPreview::onRenderFrame()
{
    if (!m_viewport || !m_viewport->IsReady())
    {
        return;
    }

    auto source = m_renderView.lock();
    if (!source)
    {
        return;
    }

    Rendering::RenderView view = *source;
    view.selectedEntityId = 0;
    view.showEditorIcons = false;

    const Rendering::RenderCamera* activeCamera = nullptr;
    if (m_selectedCameraId != 0)
    {
        for (const auto& camera : view.cameras)
        {
            if (camera.entityId == m_selectedCameraId)
            {
                activeCamera = &camera;
                break;
            }
        }
    }

    if (!activeCamera && source->camera.enabled)
    {
        activeCamera = &source->camera;
    }

    if (activeCamera)
    {
        view.camera = *activeCamera;
        view.camera.enabled = true;
    }
    else
    {
        view.camera.enabled = false;
    }

    float dt = 0.0f;
    if (m_frameTimer.isValid())
    {
        dt = static_cast<float>(m_frameTimer.nsecsElapsed()) / 1'000'000'000.0f;
        m_frameTimer.restart();
    }
    else
    {
        m_frameTimer.start();
    }

    m_viewport->RenderFrame(dt, view);
}

void EditorCameraPreview::initializeRenderer()
{
    if (!m_vulkanContext || !m_surfaceReady || m_surfaceHandle == 0)
    {
        return;
    }

    shutdownRenderer();

    try
    {
        m_viewport = std::make_unique<Rendering::VulkanViewport>(m_vulkanContext, m_assetRegistry);
        m_viewport->SetLoggingEnabled(false);
        m_viewport->Initialize(reinterpret_cast<void*>(m_surfaceHandle),
                               m_surfaceSize.width(),
                               m_surfaceSize.height());

        if (m_viewport->IsReady())
        {
            m_renderTimer->start();
        }
    }
    catch (const std::exception& ex)
    {
        if (m_statusLabel)
        {
            m_statusLabel->setText(tr("Render error: %1").arg(QString::fromStdString(ex.what())));
            m_statusLabel->setStyleSheet("color: red;");
        }
    }
}

void EditorCameraPreview::shutdownRenderer()
{
    if (m_renderTimer)
    {
        m_renderTimer->stop();
    }

    if (m_viewport)
    {
        m_viewport->Shutdown();
        m_viewport.reset();
    }
}

void EditorCameraPreview::UpdateStatusLabel()
{
    if (!m_statusLabel)
    {
        return;
    }

    QString label = tr("No camera selected");
    if (m_selectedCameraId != 0)
    {
        QString name = tr("Camera %1").arg(QString::number(static_cast<qulonglong>(m_selectedCameraId)));
        if (m_scene)
        {
            if (auto entity = m_scene->FindEntityById(m_selectedCameraId))
            {
                name = QString::fromStdString(entity->GetName());
            }
        }
        label = tr("Selected: %1").arg(name);
    }
    else if (auto view = m_renderView.lock())
    {
        if (view->camera.enabled)
        {
            label = tr("Previewing primary camera");
        }
    }

    m_statusLabel->setText(label);
}
} // namespace Aetherion::Editor
