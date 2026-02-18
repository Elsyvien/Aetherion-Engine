#pragma once

#include <QDockWidget>
#include <QElapsedTimer>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

class QLabel;
class QTimer;
class QVBoxLayout;
class QProgressBar;
class QPushButton;
class QScrollArea;

namespace Aetherion::Scene {
class Scene;
}

namespace Aetherion::Assets {
class AssetRegistry;
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

    /// @brief Provide asset registry for asset analytics
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);

    /// @brief Set the target frame time for budget calculations
    void SetTargetFrameTime(float targetFrameTimeMs);

    /// @brief Update CPU/GPU timing info (if available)
    void UpdateFrameProfile(double cpuMs, double gpuMs, bool valid);

public slots:
    /// @brief Refresh all statistics
    void RefreshStats();

private:
    void setupUI();
    QString formatMemory(size_t bytes) const;
    QString formatNumber(long long number) const;
    void ResetMetrics();
    QString BuildSummaryText() const;
    QString BuildSummaryJson() const;

    std::shared_ptr<Scene::Scene> m_scene;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    QTimer* m_refreshTimer = nullptr;
    QElapsedTimer m_assetRefreshTimer;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_layout = nullptr;

    // Performance section
    QLabel* m_fpsLabel = nullptr;
    QLabel* m_frameTimeLabel = nullptr;
    QLabel* m_frameRangeLabel = nullptr;
    QLabel* m_p95FrameLabel = nullptr;
    QLabel* m_jitterLabel = nullptr;
    QLabel* m_cpuTimeLabel = nullptr;
    QLabel* m_gpuTimeLabel = nullptr;
    QProgressBar* m_budgetBar = nullptr;
    QProgressBar* m_stabilityBar = nullptr;
    QLabel* m_budgetLabel = nullptr;

    // Scene section
    QLabel* m_entityCountLabel = nullptr;
    QLabel* m_componentCountLabel = nullptr;
    QLabel* m_meshCountLabel = nullptr;
    QLabel* m_lightCountLabel = nullptr;
    QLabel* m_lightTypeLabel = nullptr;
    QLabel* m_cameraCountLabel = nullptr;
    QLabel* m_animatorCountLabel = nullptr;
    QLabel* m_skeletonCountLabel = nullptr;
    QLabel* m_boneCountLabel = nullptr;
    QLabel* m_scriptCountLabel = nullptr;
    QLabel* m_aiBehaviorCountLabel = nullptr;
    QLabel* m_semanticCountLabel = nullptr;

    // Render section
    QLabel* m_drawCallsLabel = nullptr;
    QLabel* m_trianglesLabel = nullptr;
    QLabel* m_verticesLabel = nullptr;
    QLabel* m_uniqueMeshesLabel = nullptr;
    QLabel* m_meshMemoryLabel = nullptr;

    // Simulation section
    QLabel* m_rigidbodyLabel = nullptr;
    QLabel* m_colliderLabel = nullptr;
    QLabel* m_audioLabel = nullptr;
    QLabel* m_particleLabel = nullptr;

    // Spatial section
    QLabel* m_boundsLabel = nullptr;
    QLabel* m_boundsSizeLabel = nullptr;
    QLabel* m_boundsVolumeLabel = nullptr;
    QLabel* m_densityLabel = nullptr;

    // Assets section
    QLabel* m_assetCountLabel = nullptr;
    QLabel* m_assetSizeLabel = nullptr;
    QLabel* m_textureAssetLabel = nullptr;
    QLabel* m_meshAssetLabel = nullptr;
    QLabel* m_audioAssetLabel = nullptr;
    QLabel* m_scriptAssetLabel = nullptr;
    QLabel* m_shaderAssetLabel = nullptr;
    QLabel* m_animationAssetLabel = nullptr;
    QLabel* m_skeletonAssetLabel = nullptr;
    QLabel* m_sceneAssetLabel = nullptr;
    QLabel* m_otherAssetLabel = nullptr;

    // Summary / actions
    QLabel* m_complexityLabel = nullptr;
    QProgressBar* m_complexityBar = nullptr;
    QPushButton* m_copyStatsButton = nullptr;
    QPushButton* m_exportStatsButton = nullptr;
    QPushButton* m_resetStatsButton = nullptr;

    // Tracking
    float m_lastFrameTime = 16.67f;
    float m_avgFrameTime = 16.67f;
    float m_targetFrameTimeMs = 16.67f;
    double m_lastCpuMs = 0.0;
    double m_lastGpuMs = 0.0;
    bool m_hasGpuStats = false;
    std::deque<float> m_frameHistory;

    int m_lastDrawCalls = 0;
    long long m_lastTriangles = 0;
    long long m_lastVertices = 0;

    struct MeshCacheEntry {
        long long vertices = 0;
        long long triangles = 0;
        size_t memoryBytes = 0;
        bool valid = false;
    };
    std::unordered_map<std::string, MeshCacheEntry> m_meshCache;
    QString m_lastSummaryText;
    QString m_lastSummaryJson;

    struct AssetTotals {
        int totalCount = 0;
        size_t totalBytes = 0;
        int textureCount = 0;
        size_t textureBytes = 0;
        int meshCount = 0;
        size_t meshBytes = 0;
        int audioCount = 0;
        size_t audioBytes = 0;
        int scriptCount = 0;
        size_t scriptBytes = 0;
        int shaderCount = 0;
        size_t shaderBytes = 0;
        int animationCount = 0;
        size_t animationBytes = 0;
        int skeletonCount = 0;
        size_t skeletonBytes = 0;
        int sceneCount = 0;
        size_t sceneBytes = 0;
        int otherCount = 0;
        size_t otherBytes = 0;
    } m_lastAssetTotals;
};

} // namespace Aetherion::Editor
