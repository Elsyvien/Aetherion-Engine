#pragma once

#include <QDockWidget>
#include <memory>

class QLabel;
class QTimer;
class QVBoxLayout;
class QProgressBar;

namespace Aetherion::Scene {
class Scene;
}

namespace Aetherion::Editor {

/// @brief Panel displaying scene and performance statistics
///
/// Shows real-time information about:
/// - Frame time and FPS
/// - Entity and component counts
/// - Memory usage estimates
/// - Render statistics
class EditorStatisticsPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit EditorStatisticsPanel(QWidget* parent = nullptr);
    ~EditorStatisticsPanel() override = default;

    /// @brief Set the scene to gather statistics from
    void SetScene(std::shared_ptr<Scene::Scene> scene);

    /// @brief Update frame timing information
    /// @param frameTimeMs Frame time in milliseconds
    void UpdateFrameTime(float frameTimeMs);

    /// @brief Update render statistics
    /// @param drawCalls Number of draw calls
    /// @param triangles Number of triangles rendered
    /// @param vertices Number of vertices
    void UpdateRenderStats(int drawCalls, int triangles, int vertices);

public slots:
    /// @brief Refresh all statistics
    void RefreshStats();

private:
    void setupUI();
    QString formatMemory(size_t bytes) const;
    QString formatNumber(int number) const;

    std::shared_ptr<Scene::Scene> m_scene;
    QTimer* m_refreshTimer = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_layout = nullptr;

    // Performance section
    QLabel* m_fpsLabel = nullptr;
    QLabel* m_frameTimeLabel = nullptr;
    QProgressBar* m_cpuBar = nullptr;

    // Scene section
    QLabel* m_entityCountLabel = nullptr;
    QLabel* m_componentCountLabel = nullptr;
    QLabel* m_meshCountLabel = nullptr;
    QLabel* m_lightCountLabel = nullptr;
    QLabel* m_cameraCountLabel = nullptr;

    // Render section
    QLabel* m_drawCallsLabel = nullptr;
    QLabel* m_trianglesLabel = nullptr;
    QLabel* m_verticesLabel = nullptr;

    // Tracking
    float m_lastFrameTime = 16.67f;
    float m_frameTimeAccum = 0.0f;
    int m_frameCount = 0;
    float m_avgFrameTime = 16.67f;

    int m_lastDrawCalls = 0;
    int m_lastTriangles = 0;
    int m_lastVertices = 0;
};

} // namespace Aetherion::Editor
