#pragma once

#include <QWidget>
#include <QPoint>

class QTimer;
class QMouseEvent;
class QWheelEvent;
class QToolButton;
class QLabel;
class QEvent;

namespace Aetherion::Editor
{
class EditorViewport : public QWidget
{
    Q_OBJECT

public:
    explicit EditorViewport(QWidget* parent = nullptr);
    ~EditorViewport() override;

    QWidget* surfaceWidget() const { return m_surface; }

    // Camera state accessors for renderer
    [[nodiscard]] float getCameraX() const noexcept { return m_cameraX; }
    [[nodiscard]] float getCameraY() const noexcept { return m_cameraY; }
    [[nodiscard]] float getCameraZ() const noexcept { return m_cameraZ; }
    [[nodiscard]] float getCameraRotationY() const noexcept { return m_cameraRotationY; }
    [[nodiscard]] float getCameraRotationX() const noexcept { return m_cameraRotationX; }
    [[nodiscard]] float getCameraZoom() const noexcept { return m_cameraZoom; }
    void resetCamera();
    void updateCamera(float deltaTime);
    void SetCameraTarget(float x, float y, float z);
    void SetCameraZoom(float zoom);

signals:
    // WId ist unter Windows i.d.R. ein HWND (kann in Rendering/Win32 zu HWND gecastet werden)
    void surfaceReady(WId nativeHandle, int width, int height);
    void surfaceResized(int width, int height);
    void cameraChanged();
    void focusRequested();
    void gizmoDrag(float dx, float dy);
    void gizmoDragStarted();
    void gizmoDragEnded();

protected:
    bool event(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;

private:
    void UpdateOverlayGeometry();
    QWidget* m_surface = nullptr;
    bool m_emittedReady = false;
    QTimer* m_resizeDebounceTimer = nullptr;
    QWidget* m_overlayWidget = nullptr;
    QToolButton* m_focusButton = nullptr;
    QLabel* m_focusHint = nullptr;
    QLabel* m_speedLabel = nullptr;

    // Camera state
    float m_cameraX = 0.0f;
    float m_cameraY = 0.0f;
    float m_cameraZ = 0.0f;
    float m_cameraRotationY = 0.0f; // Yaw in degrees
    float m_cameraRotationX = 0.0f; // Pitch in degrees
    float m_cameraZoom = 1.0f;

    // Mouse tracking
    bool m_isPanning = false;
    bool m_isRotating = false;
    bool m_isGizmoDragging = false;
    QPoint m_lastMousePos;
    
    // Keyboard navigation state
    bool m_keyForward = false;
    bool m_keyBackward = false;
    bool m_keyLeft = false;
    bool m_keyRight = false;
    bool m_keyUp = false;
    bool m_keyDown = false;
    bool m_keyFast = false;
    bool m_keySlow = false;
};
} // namespace Aetherion::Editor
