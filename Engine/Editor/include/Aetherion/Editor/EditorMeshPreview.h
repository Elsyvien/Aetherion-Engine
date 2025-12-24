#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QLabel;
class QSlider;
class QVBoxLayout;

namespace Aetherion::Rendering
{
class VulkanContext;
class VulkanViewport;
} // namespace Aetherion::Rendering

namespace Aetherion::Assets
{
class AssetRegistry;
} // namespace Aetherion::Assets

namespace Aetherion::Editor
{
class EditorViewportSurface;

class EditorMeshPreview : public QWidget
{
    Q_OBJECT

public:
    explicit EditorMeshPreview(QWidget* parent = nullptr);
    ~EditorMeshPreview() override;

    void SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context);
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);
    void SetMeshAsset(const QString& assetId);
    void ClearPreview();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onSurfaceReady();
    void onRenderFrame();

private:
    void initializeRenderer();
    void shutdownRenderer();
    void ApplyAutoFit();

    QLabel* m_header = nullptr;
    QLabel* m_assetLabel = nullptr;
    QWidget* m_viewportContainer = nullptr;
    EditorViewportSurface* m_surface = nullptr;
    class QTimer* m_renderTimer = nullptr;

    std::shared_ptr<Rendering::VulkanContext> m_vulkanContext;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    std::unique_ptr<Rendering::VulkanViewport> m_viewport;

    QString m_currentAssetId;
    bool m_surfaceReady = false;
    WId m_surfaceHandle = 0;
    QSize m_surfaceSize;

    // Camera for preview rotation
    float m_rotationY = 0.0f;
    float m_rotationX = 20.0f;
    float m_zoom = 3.0f;
    bool m_rotating = false;
    QPoint m_lastMousePos;
    bool m_pendingFit = false;
};
} // namespace Aetherion::Editor
