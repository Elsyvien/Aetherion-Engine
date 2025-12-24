#include "Aetherion/Editor/EditorViewport.h"

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QWindow>
#include <cmath>

namespace Aetherion::Editor
{
EditorViewport::EditorViewport(QWidget* parent)
    : QWidget(parent)
{
    // Ensure the viewport widget itself expands.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(100, 100);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Use a simple native QWidget instead of QWindow+createWindowContainer.
    // This avoids positioning issues on macOS where QWindow may render at wrong location.
    m_surface = new QWidget(this);
    m_surface->setAttribute(Qt::WA_NativeWindow, true);
    m_surface->setAttribute(Qt::WA_PaintOnScreen, true);
    m_surface->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_surface->setAttribute(Qt::WA_NoSystemBackground, true);
    m_surface->setAutoFillBackground(false);
    m_surface->setMouseTracking(true);
    
    // Policy for proper resizing - container must expand to fill layout
    m_surface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_surface->setMinimumSize(100, 100);

    layout->addWidget(m_surface, 1);

    setLayout(layout);

    // Debounce timer: emit resize only after user stops resizing for 50ms.
    m_resizeDebounceTimer = new QTimer(this);
    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(50);
    connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this] {
        if (!m_surface)
        {
            return;
        }

        const QSize surfaceSize = m_surface->size();
        if (!surfaceSize.isEmpty())
        {
            emit surfaceResized(surfaceSize.width(), surfaceSize.height());
        }
    });

    resetCamera();
}

EditorViewport::~EditorViewport()
{
    // m_surface is a child widget, automatically deleted.
}

void EditorViewport::resetCamera()
{
    m_cameraX = 0.0f;
    m_cameraY = 0.0f;
    m_cameraZ = 0.0f;
    m_cameraRotationY = 30.0f;  // Slight yaw for 3D view
    m_cameraRotationX = 25.0f;  // Slight pitch for 3D view
    m_cameraZoom = 1.0f;
    emit cameraChanged();
}

void EditorViewport::SetCameraTarget(float x, float y, float z)
{
    m_cameraX = x;
    m_cameraY = y;
    m_cameraZ = z;
    emit cameraChanged();
}

void EditorViewport::SetCameraZoom(float zoom)
{
    m_cameraZoom = zoom;
    emit cameraChanged();
}

void EditorViewport::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);

    // Emit ready once the native handle exists.
    if (!m_emittedReady && m_surface)
    {
        // Force creation of the native window handle.
        WId handle = m_surface->winId();
        if (handle != 0)
        {
            m_emittedReady = true;
            const QSize surfaceSize = m_surface->size();
            emit surfaceReady(handle, surfaceSize.width(), surfaceSize.height());

            // Schedule a deferred resize in case layout adjusts after show.
            QTimer::singleShot(0, this, [this] {
                if (m_resizeDebounceTimer)
                {
                    m_resizeDebounceTimer->start();
                }
            });
        }
    }
}

void EditorViewport::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);

    // Restart debounce timer; actual resize will happen after user stops.
    if (m_resizeDebounceTimer)
    {
        m_resizeDebounceTimer->start();
    }
}

void EditorViewport::mousePressEvent(QMouseEvent* e)
{
    m_lastMousePos = e->pos();
    
    if (e->button() == Qt::MiddleButton)
    {
        // Middle button: pan
        m_isPanning = true;
        e->accept();
        return;
    }
    else if (e->button() == Qt::RightButton)
    {
        // Right button: rotate
        m_isRotating = true;
        e->accept();
        return;
    }

    QWidget::mousePressEvent(e);
}

void EditorViewport::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton)
    {
        m_isPanning = false;
        e->accept();
        return;
    }
    else if (e->button() == Qt::RightButton)
    {
        m_isRotating = false;
        e->accept();
        return;
    }

    QWidget::mouseReleaseEvent(e);
}

void EditorViewport::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint delta = e->pos() - m_lastMousePos;
    m_lastMousePos = e->pos();

    if (m_isPanning)
    {
        // Pan the camera
        const float panSpeed = 0.01f * m_cameraZoom;
        
        // Convert screen delta to world delta based on camera rotation
        const float yawRad = m_cameraRotationY * 3.14159265f / 180.0f;
        const float dx = delta.x() * panSpeed;
        const float dy = delta.y() * panSpeed;
        
        m_cameraX -= dx * std::cos(yawRad);
        m_cameraZ -= dx * std::sin(yawRad);
        m_cameraY += dy;
        
        emit cameraChanged();
        e->accept();
        return;
    }
    else if (m_isRotating)
    {
        // Rotate the camera (orbit)
        const float rotateSpeed = 0.3f;
        m_cameraRotationY += delta.x() * rotateSpeed;
        m_cameraRotationX += delta.y() * rotateSpeed;
        
        // Clamp pitch to prevent gimbal lock
        if (m_cameraRotationX > 89.0f) m_cameraRotationX = 89.0f;
        if (m_cameraRotationX < -89.0f) m_cameraRotationX = -89.0f;
        
        emit cameraChanged();
        e->accept();
        return;
    }

    QWidget::mouseMoveEvent(e);
}

void EditorViewport::wheelEvent(QWheelEvent* e)
{
    // Zoom in/out
    const float zoomSpeed = 0.001f;
    const float delta = e->angleDelta().y() * zoomSpeed;
    
    m_cameraZoom *= (1.0f - delta);
    
    // Clamp zoom
    if (m_cameraZoom < 0.1f) m_cameraZoom = 0.1f;
    if (m_cameraZoom > 10.0f) m_cameraZoom = 10.0f;
    
    emit cameraChanged();
    e->accept();
}

void EditorViewport::keyPressEvent(QKeyEvent* e)
{
    const float moveSpeed = 0.1f * m_cameraZoom;
    const float yawRad = m_cameraRotationY * 3.14159265f / 180.0f;
    bool handled = true;

    switch (e->key())
    {
    case Qt::Key_W:
        // Move forward (into the scene)
        m_cameraX -= moveSpeed * std::sin(yawRad);
        m_cameraZ -= moveSpeed * std::cos(yawRad);
        break;
    case Qt::Key_S:
        // Move backward
        m_cameraX += moveSpeed * std::sin(yawRad);
        m_cameraZ += moveSpeed * std::cos(yawRad);
        break;
    case Qt::Key_A:
        // Strafe left
        m_cameraX -= moveSpeed * std::cos(yawRad);
        m_cameraZ += moveSpeed * std::sin(yawRad);
        break;
    case Qt::Key_D:
        // Strafe right
        m_cameraX += moveSpeed * std::cos(yawRad);
        m_cameraZ -= moveSpeed * std::sin(yawRad);
        break;
    case Qt::Key_Q:
        // Move down
        m_cameraY -= moveSpeed;
        break;
    case Qt::Key_E:
        // Move up
        m_cameraY += moveSpeed;
        break;
    default:
        handled = false;
        break;
    }

    if (handled)
    {
        emit cameraChanged();
        e->accept();
        return;
    }

    QWidget::keyPressEvent(e);
}

void EditorViewport::keyReleaseEvent(QKeyEvent* e)
{
    QWidget::keyReleaseEvent(e);
}
} // namespace Aetherion::Editor
