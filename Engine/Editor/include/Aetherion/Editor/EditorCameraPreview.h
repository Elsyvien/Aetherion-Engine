#pragma once

#include <QElapsedTimer>
#include <QSize>
#include <QWidget>
#include <memory>

#include "Aetherion/Core/Types.h"

class QLabel;
class QTimer;

namespace Aetherion::Rendering
{
class RenderView;
class VulkanContext;
class VulkanViewport;
} // namespace Aetherion::Rendering

namespace Aetherion::Assets
{
class AssetRegistry;
} // namespace Aetherion::Assets

namespace Aetherion::Scene
{
class Scene;
} // namespace Aetherion::Scene

namespace Aetherion::Editor
{
class CameraPreviewSurface;

class EditorCameraPreview : public QWidget
{
    Q_OBJECT

public:
    explicit EditorCameraPreview(QWidget* parent = nullptr);
    ~EditorCameraPreview() override;

    void SetVulkanContext(std::shared_ptr<Rendering::VulkanContext> context);
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);
    void SetRenderViewSource(std::shared_ptr<Rendering::RenderView> view);
    void SetScene(std::shared_ptr<Scene::Scene> scene);
    void SetSelectedCameraId(Core::EntityId id);
    void ClearPreview();

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSurfaceReady();
    void onRenderFrame();

private:
    void initializeRenderer();
    void shutdownRenderer();
    void UpdateStatusLabel();

    QLabel* m_header = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWidget* m_viewportContainer = nullptr;
    CameraPreviewSurface* m_surface = nullptr;
    QTimer* m_renderTimer = nullptr;

    std::shared_ptr<Rendering::VulkanContext> m_vulkanContext;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    std::weak_ptr<Rendering::RenderView> m_renderView;
    std::shared_ptr<Scene::Scene> m_scene;
    std::unique_ptr<Rendering::VulkanViewport> m_viewport;

    Core::EntityId m_selectedCameraId{0};
    bool m_surfaceReady = false;
    WId m_surfaceHandle = 0;
    QSize m_surfaceSize{};
    QElapsedTimer m_frameTimer;
};
} // namespace Aetherion::Editor
