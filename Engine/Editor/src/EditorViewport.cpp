#include "Aetherion/Editor/EditorViewport.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QToolButton>
#include <QLabel>
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
    m_surface->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_surface->setAutoFillBackground(false);
    m_surface->setMouseTracking(true);
    
    // Policy for proper resizing - container must expand to fill layout
    m_surface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_surface->setMinimumSize(100, 100);

    layout->addWidget(m_surface, 1);

    setLayout(layout);

    m_overlayWidget = new QWidget(this);
    m_overlayWidget->setObjectName("viewportOverlay");
    m_overlayWidget->setAttribute(Qt::WA_TranslucentBackground, true);
    m_overlayWidget->setFocusPolicy(Qt::NoFocus);
    m_overlayWidget->setStyleSheet(
        "QWidget#viewportOverlay { background-color: rgba(20, 20, 20, 160); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 6px; }"
        "QToolButton { color: #f2f2f2; background: transparent; padding: 2px 6px; }"
        "QLabel#focusHint { color: #e0e0e0; background-color: rgba(255, 255, 255, 30); "
        "padding: 1px 4px; border-radius: 3px; }");

    auto* overlayLayout = new QHBoxLayout(m_overlayWidget);
    overlayLayout->setContentsMargins(6, 4, 6, 4);
    overlayLayout->setSpacing(6);

    m_focusButton = new QToolButton(m_overlayWidget);
    m_focusButton->setText(tr("Focus"));
    m_focusButton->setToolTip(tr("Focus on selection (F)"));
    m_focusButton->setFocusPolicy(Qt::NoFocus);

    m_focusHint = new QLabel(tr("F"), m_overlayWidget);
    m_focusHint->setObjectName("focusHint");
    m_focusHint->setFocusPolicy(Qt::NoFocus);

    m_speedLabel = new QLabel(tr("1x"), m_overlayWidget);
    m_speedLabel->setStyleSheet("color: #e0e0e0; padding-left: 8px; font-weight: bold;");
    m_speedLabel->setFocusPolicy(Qt::NoFocus);

    overlayLayout->addWidget(m_focusButton);
    overlayLayout->addWidget(m_focusHint);
    overlayLayout->addWidget(m_speedLabel);

    m_overlayWidget->raise();
    UpdateOverlayGeometry();

    connect(m_focusButton, &QToolButton::clicked, this, [this] {
        emit focusRequested();
    });

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

bool EditorViewport::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(e);
        switch (keyEvent->key())
        {
        case Qt::Key_W:
        case Qt::Key_A:
        case Qt::Key_S:
        case Qt::Key_D:
        case Qt::Key_Q:
        case Qt::Key_E:
            e->accept();
            return true;
        default:
            break;
        }
    }
    return QWidget::event(e);
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

void EditorViewport::UpdateOverlayGeometry()
{
    if (!m_overlayWidget)
    {
        return;
    }

    m_overlayWidget->adjustSize();
    m_overlayWidget->move(8, 8);
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
    UpdateOverlayGeometry();
}

void EditorViewport::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);

    // Restart debounce timer; actual resize will happen after user stops.
    if (m_resizeDebounceTimer)
    {
        m_resizeDebounceTimer->start();
    }
    UpdateOverlayGeometry();
}

void EditorViewport::mousePressEvent(QMouseEvent* e)
{
    setFocus();
    m_lastMousePos = e->pos();

    if (e->button() == Qt::LeftButton)
    {
        m_isGizmoDragging = true;
        emit gizmoDragStarted();
        e->accept();
        return;
    }
    else if (e->button() == Qt::MiddleButton)
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
    if (e->button() == Qt::LeftButton)
    {
        m_isGizmoDragging = false;
        emit gizmoDragEnded();
        e->accept();
        return;
    }
    else if (e->button() == Qt::MiddleButton)
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

    if (m_isGizmoDragging)
    {
        emit gizmoDrag(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        e->accept();
        return;
    }

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
        // FPS-style Look: Rotate around the camera position (Eye), not the target (Center).
        // Eye = Center + Offset. We want Eye to remain fixed.
        // NewCenter = Eye - NewOffset = (Center + OldOffset) - NewOffset
        // NewCenter = Center + (OldOffset - NewOffset)

        constexpr float kBaseDistance = 5.0f;
        const float dist = kBaseDistance * m_cameraZoom;

        // 1. Calculate Old Offset
        const float oldYawRad = m_cameraRotationY * 3.14159265f / 180.0f;
        const float oldPitchRad = m_cameraRotationX * 3.14159265f / 180.0f;

        const float oldOffsetX = dist * std::cos(oldPitchRad) * std::sin(oldYawRad);
        const float oldOffsetY = dist * std::sin(oldPitchRad);
        const float oldOffsetZ = dist * std::cos(oldPitchRad) * std::cos(oldYawRad);

        // 2. Update Rotation
        const float rotateSpeed = 0.3f;
        m_cameraRotationY += delta.x() * rotateSpeed;
        m_cameraRotationX += delta.y() * rotateSpeed;
        
        // Clamp pitch to prevent gimbal lock
        if (m_cameraRotationX > 89.0f) m_cameraRotationX = 89.0f;
        if (m_cameraRotationX < -89.0f) m_cameraRotationX = -89.0f;
        
        // 3. Calculate New Offset
        const float newYawRad = m_cameraRotationY * 3.14159265f / 180.0f;
        const float newPitchRad = m_cameraRotationX * 3.14159265f / 180.0f;

        const float newOffsetX = dist * std::cos(newPitchRad) * std::sin(newYawRad);
        const float newOffsetY = dist * std::sin(newPitchRad);
        const float newOffsetZ = dist * std::cos(newPitchRad) * std::cos(newYawRad);

        // 4. Update Center
        m_cameraX += (oldOffsetX - newOffsetX);
        m_cameraY += (oldOffsetY - newOffsetY);
        m_cameraZ += (oldOffsetZ - newOffsetZ);
        
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

void EditorViewport::updateCamera(float deltaTime)
{
    float speedMult = 1.0f;
    if (m_keyFast)
    {
        speedMult = 4.0f;
    }
    if (m_keySlow)
    {
        speedMult = 0.1f;
    }

    if (m_speedLabel)
    {
        m_speedLabel->setText(QString::number(speedMult, 'g', 2) + "x");
    }

    float speed = 5.0f * m_cameraZoom * speedMult; // Base speed

    const float moveAmount = speed * deltaTime;
    const float yawRad = m_cameraRotationY * 3.14159265f / 180.0f;
    
    bool changed = false;

    if (m_keyForward)
    {
        float dx = moveAmount * std::sin(yawRad);
        float dz = moveAmount * std::cos(yawRad);
        m_cameraX -= dx;
        m_cameraZ -= dz;
        changed = true;
    }
    if (m_keyBackward)
    {
        m_cameraX += moveAmount * std::sin(yawRad);
        m_cameraZ += moveAmount * std::cos(yawRad);
        changed = true;
    }
    if (m_keyLeft)
    {
        m_cameraX -= moveAmount * std::cos(yawRad);
        m_cameraZ += moveAmount * std::sin(yawRad);
        changed = true;
    }
    if (m_keyRight)
    {
        m_cameraX += moveAmount * std::cos(yawRad);
        m_cameraZ -= moveAmount * std::sin(yawRad);
        changed = true;
    }
    if (m_keyUp)
    {
        m_cameraY += moveAmount;
        changed = true;
    }
    if (m_keyDown)
    {
        m_cameraY -= moveAmount;
        changed = true;
    }

    if (changed)
    {
        emit cameraChanged();
    }
}

void EditorViewport::keyPressEvent(QKeyEvent* e)
{
    bool handled = true;

    switch (e->key())
    {
    case Qt::Key_W: m_keyForward = true; break;
    case Qt::Key_S: m_keyBackward = true; break;
    case Qt::Key_A: m_keyLeft = true; break;
    case Qt::Key_D: m_keyRight = true; break;
    case Qt::Key_Q: m_keyDown = true; break;
    case Qt::Key_E: m_keyUp = true; break;
    case Qt::Key_Shift: m_keyFast = true; break;
    case Qt::Key_Control: m_keySlow = true; break;
    default: handled = false; break;
    }

    if (handled)
    {
        e->accept();
        return;
    }

    QWidget::keyPressEvent(e);
}

void EditorViewport::keyReleaseEvent(QKeyEvent* e)
{
    bool handled = true;

    switch (e->key())
    {
    case Qt::Key_W: m_keyForward = false; break;
    case Qt::Key_S: m_keyBackward = false; break;
    case Qt::Key_A: m_keyLeft = false; break;
    case Qt::Key_D: m_keyRight = false; break;
    case Qt::Key_Q: m_keyDown = false; break;
    case Qt::Key_E: m_keyUp = false; break;
    case Qt::Key_Shift: m_keyFast = false; break;
    case Qt::Key_Control: m_keySlow = false; break;
    default: handled = false; break;
    }

    if (handled)
    {
        e->accept();
        return;
    }

    QWidget::keyReleaseEvent(e);
}
} // namespace Aetherion::Editor
