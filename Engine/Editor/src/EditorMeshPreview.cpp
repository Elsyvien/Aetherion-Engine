#include "Aetherion/Editor/EditorMeshPreview.h"

#include <QElapsedTimer>
#include <QLabel>
#include <QMouseEvent>
#include <QObject>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>
#include <filesystem>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Rendering/VulkanViewport.h"

namespace Aetherion::Editor
{
// Simple surface widget for the preview
class EditorViewportSurface : public QWidget
{
public:
    explicit EditorViewportSurface(QWidget* parent = nullptr)
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
    }
};

EditorMeshPreview::EditorMeshPreview(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_header = new QLabel(tr("Mesh Preview"), this);
    layout->addWidget(m_header);

    m_assetLabel = new QLabel(tr("No mesh selected"), this);
    m_assetLabel->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(m_assetLabel);

    // Viewport container
    m_viewportContainer = new QWidget(this);
    m_viewportContainer->setMinimumSize(150, 150);
    m_viewportContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto* containerLayout = new QVBoxLayout(m_viewportContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    
    m_surface = new EditorViewportSurface(m_viewportContainer);
    m_surface->installEventFilter(this);
    containerLayout->addWidget(m_surface, 1);
    
    layout->addWidget(m_viewportContainer, 1);

    setLayout(layout);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    // Render timer
    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(33); // ~30 FPS for preview
    connect(m_renderTimer, &QTimer::timeout, this, &EditorMeshPreview::onRenderFrame);

    // Delay surface ready check
    QTimer::singleShot(100, this, &EditorMeshPreview::onSurfaceReady);
}

EditorMeshPreview::~EditorMeshPreview()
{
    shutdownRenderer();
}

void EditorMeshPreview::SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context)
{
    m_vulkanContext = std::move(context);
    if (m_surfaceReady && m_vulkanContext && !m_currentAssetId.isEmpty())
    {
        initializeRenderer();
    }
}

void EditorMeshPreview::SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry)
{
    m_assetRegistry = std::move(registry);
}

void EditorMeshPreview::SetMeshAsset(const QString& assetId)
{
    if (assetId == m_currentAssetId)
    {
        return;
    }

    m_currentAssetId = assetId;
    
    if (assetId.isEmpty())
    {
        ClearPreview();
        return;
    }

    m_assetLabel->setText(assetId);
    m_assetLabel->setStyleSheet("color: white;");

    // Reset camera for new mesh
    m_rotationY = 0.0f;
    m_rotationX = 20.0f;
    m_zoom = 3.0f;
    m_pendingFit = true;

    if (m_vulkanContext && m_surfaceReady)
    {
        initializeRenderer();
    }
}

void EditorMeshPreview::ClearPreview()
{
    m_currentAssetId.clear();
    m_assetLabel->setText(tr("No mesh selected"));
    m_assetLabel->setStyleSheet("color: gray; font-style: italic;");
    
    shutdownRenderer();
}

void EditorMeshPreview::resizeEvent(QResizeEvent* event)
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

void EditorMeshPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    {
        m_rotating = true;
        m_lastMousePos = event->pos();
        event->accept();
    }
    else
    {
        QWidget::mousePressEvent(event);
    }
}

void EditorMeshPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    {
        m_rotating = false;
        event->accept();
    }
    else
    {
        QWidget::mouseReleaseEvent(event);
    }
}

void EditorMeshPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (m_rotating)
    {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        m_rotationY += delta.x() * 0.5f;
        m_rotationX += delta.y() * 0.5f;

        // Clamp pitch
        m_rotationX = std::clamp(m_rotationX, -89.0f, 89.0f);

        if (m_viewport)
        {
            m_viewport->SetCameraRotation(m_rotationY, m_rotationX);
        }

        event->accept();
    }
    else
    {
        QWidget::mouseMoveEvent(event);
    }
}

void EditorMeshPreview::wheelEvent(QWheelEvent* event)
{
    float delta = event->angleDelta().y() / 120.0f;
    m_zoom = std::clamp(m_zoom - delta * 0.3f, 0.5f, 20.0f);

    if (m_viewport)
    {
        m_viewport->SetCameraDistance(m_zoom);
    }

    event->accept();
}

bool EditorMeshPreview::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_surface)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
            mousePressEvent(static_cast<QMouseEvent*>(event));
            return event->isAccepted();
        case QEvent::MouseButtonRelease:
            mouseReleaseEvent(static_cast<QMouseEvent*>(event));
            return event->isAccepted();
        case QEvent::MouseMove:
            mouseMoveEvent(static_cast<QMouseEvent*>(event));
            return event->isAccepted();
        case QEvent::Wheel:
            wheelEvent(static_cast<QWheelEvent*>(event));
            return event->isAccepted();
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void EditorMeshPreview::onSurfaceReady()
{
    if (!m_surface)
    {
        return;
    }

    m_surfaceHandle = m_surface->winId();
    m_surfaceSize = m_surface->size();
    m_surfaceReady = true;

    if (m_vulkanContext && !m_currentAssetId.isEmpty())
    {
        initializeRenderer();
    }
}

void EditorMeshPreview::onRenderFrame()
{
    if (!m_viewport || !m_viewport->IsReady() || m_currentAssetId.isEmpty())
    {
        return;
    }

    // Create a simple render view with just the preview mesh
    Rendering::RenderView view;
    
    Rendering::RenderInstance instance;
    instance.entityId = 1;
    instance.meshAssetId = m_currentAssetId.toStdString();
    if (m_assetRegistry)
    {
        const std::string stem = std::filesystem::path(instance.meshAssetId).stem().string();
        if (const auto* cached = m_assetRegistry->GetMesh(stem); cached && !cached->textureIds.empty())
        {
            instance.albedoTextureId = cached->textureIds.front();
        }
    }
    instance.transform = nullptr;
    instance.mesh = nullptr;
    // Identity matrix
    instance.model[0] = 1.0f; instance.model[5] = 1.0f; 
    instance.model[10] = 1.0f; instance.model[15] = 1.0f;
    instance.hasModel = true;

    view.instances.push_back(instance);
    view.directionalLight.enabled = true;
    view.directionalLight.direction[0] = -0.4f;
    view.directionalLight.direction[1] = -1.0f;
    view.directionalLight.direction[2] = -0.6f;

    static QElapsedTimer timer;
    static bool timerStarted = false;
    if (!timerStarted)
    {
        timer.start();
        timerStarted = true;
    }

    float dt = timer.elapsed() / 1000.0f;
    timer.restart();

    m_viewport->RenderFrame(dt, view);
}

void EditorMeshPreview::initializeRenderer()
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

        // Set initial camera
        m_viewport->SetCameraPosition(0.0f, 0.0f, 0.0f);
        m_viewport->SetCameraRotation(m_rotationY, m_rotationX);
        m_viewport->SetCameraDistance(m_zoom);
        ApplyAutoFit();

        if (m_viewport->IsReady())
        {
            m_renderTimer->start();
        }
    }
    catch (const std::exception& ex)
    {
        m_assetLabel->setText(tr("Render error: %1").arg(QString::fromStdString(ex.what())));
        m_assetLabel->setStyleSheet("color: red;");
    }
}

void EditorMeshPreview::shutdownRenderer()
{
    m_renderTimer->stop();

    if (m_viewport)
    {
        m_viewport->Shutdown();
        m_viewport.reset();
    }
}

void EditorMeshPreview::ApplyAutoFit()
{
    if (!m_pendingFit || !m_viewport || !m_assetRegistry || m_currentAssetId.isEmpty())
    {
        return;
    }

    const auto* meshData = m_assetRegistry->LoadMeshData(m_currentAssetId.toStdString());
    if (!meshData)
    {
        return;
    }

    const float radius = std::max(meshData->boundsRadius, 0.01f);
    const float padding = 1.35f;
    const float fovRad = 60.0f * (3.14159265358979323846f / 180.0f);
    const float distance = radius / std::sin(fovRad * 0.5f) * padding;

    m_zoom = distance;
    m_viewport->FocusOnBounds(meshData->boundsCenter[0],
                              meshData->boundsCenter[1],
                              meshData->boundsCenter[2],
                              radius,
                              padding);
    m_viewport->SetCameraRotation(m_rotationY, m_rotationX);
    m_pendingFit = false;
}
} // namespace Aetherion::Editor
