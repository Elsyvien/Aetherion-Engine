#include "Aetherion/Editor/EditorStatisticsPanel.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Scene/AIBehaviorComponent.h"

namespace Aetherion::Editor {

EditorStatisticsPanel::EditorStatisticsPanel(QWidget* parent)
    : QDockWidget(tr("Statistics"), parent)
{
    setObjectName("StatisticsPanel");
    setAllowedAreas(Qt::AllDockWidgetAreas);
    
    setupUI();
    
    // Auto-refresh every 500ms
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &EditorStatisticsPanel::RefreshStats);
    m_refreshTimer->start(500);
}

void EditorStatisticsPanel::SetScene(std::shared_ptr<Scene::Scene> scene)
{
    m_scene = std::move(scene);
    RefreshStats();
}

void EditorStatisticsPanel::UpdateFrameTime(float frameTimeMs)
{
    m_lastFrameTime = frameTimeMs;
    m_frameTimeAccum += frameTimeMs;
    m_frameCount++;
    
    // Calculate average every 10 frames
    if (m_frameCount >= 10) {
        m_avgFrameTime = m_frameTimeAccum / static_cast<float>(m_frameCount);
        m_frameTimeAccum = 0.0f;
        m_frameCount = 0;
    }
}

void EditorStatisticsPanel::UpdateRenderStats(int drawCalls, int triangles, int vertices)
{
    m_lastDrawCalls = drawCalls;
    m_lastTriangles = triangles;
    m_lastVertices = vertices;
}

void EditorStatisticsPanel::RefreshStats()
{
    // Performance stats
    const float fps = m_avgFrameTime > 0.0f ? 1000.0f / m_avgFrameTime : 0.0f;
    m_fpsLabel->setText(QString("%1 FPS").arg(static_cast<int>(fps)));
    m_frameTimeLabel->setText(QString("%1 ms").arg(m_avgFrameTime, 0, 'f', 2));
    
    // Color code FPS
    QString fpsColor = "#4CAF50"; // Green for good
    if (fps < 30.0f) {
        fpsColor = "#F44336"; // Red for bad
    } else if (fps < 60.0f) {
        fpsColor = "#FF9800"; // Orange for okay
    }
    m_fpsLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(fpsColor));
    
    // CPU usage approximation (frame time relative to 16.67ms target)
    const int cpuUsage = std::min(100, static_cast<int>((m_avgFrameTime / 16.67f) * 100.0f));
    m_cpuBar->setValue(cpuUsage);
    if (cpuUsage > 80) {
        m_cpuBar->setStyleSheet("QProgressBar::chunk { background-color: #F44336; }");
    } else if (cpuUsage > 50) {
        m_cpuBar->setStyleSheet("QProgressBar::chunk { background-color: #FF9800; }");
    } else {
        m_cpuBar->setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }");
    }
    
    // Scene stats
    if (m_scene) {
        const auto& entities = m_scene->GetEntities();
        int entityCount = 0;
        int componentCount = 0;
        int meshCount = 0;
        int lightCount = 0;
        int cameraCount = 0;
        
        for (const auto& entity : entities) {
            if (!entity) continue;
            entityCount++;
            
            if (entity->GetComponent<Scene::TransformComponent>()) componentCount++;
            if (entity->GetComponent<Scene::MeshRendererComponent>()) {
                componentCount++;
                meshCount++;
            }
            if (entity->GetComponent<Scene::LightComponent>()) {
                componentCount++;
                lightCount++;
            }
            if (entity->GetComponent<Scene::CameraComponent>()) {
                componentCount++;
                cameraCount++;
            }
            if (entity->GetComponent<Scene::RigidbodyComponent>()) componentCount++;
            if (entity->GetComponent<Scene::ColliderComponent>()) componentCount++;
            if (entity->GetComponent<Scene::AudioSourceComponent>()) componentCount++;
            if (entity->GetComponent<Scene::AIBehaviorComponent>()) componentCount++;
        }
        
        m_entityCountLabel->setText(formatNumber(entityCount));
        m_componentCountLabel->setText(formatNumber(componentCount));
        m_meshCountLabel->setText(formatNumber(meshCount));
        m_lightCountLabel->setText(formatNumber(lightCount));
        m_cameraCountLabel->setText(formatNumber(cameraCount));
    } else {
        m_entityCountLabel->setText("0");
        m_componentCountLabel->setText("0");
        m_meshCountLabel->setText("0");
        m_lightCountLabel->setText("0");
        m_cameraCountLabel->setText("0");
    }
    
    // Render stats
    m_drawCallsLabel->setText(formatNumber(m_lastDrawCalls));
    m_trianglesLabel->setText(formatNumber(m_lastTriangles));
    m_verticesLabel->setText(formatNumber(m_lastVertices));
}

void EditorStatisticsPanel::setupUI()
{
    m_content = new QWidget(this);
    m_layout = new QVBoxLayout(m_content);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(8);
    
    // Performance Group
    auto* perfGroup = new QGroupBox(tr("Performance"), m_content);
    auto* perfLayout = new QFormLayout(perfGroup);
    perfLayout->setSpacing(4);
    
    m_fpsLabel = new QLabel("60 FPS", perfGroup);
    m_fpsLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    perfLayout->addRow(tr("Frame Rate:"), m_fpsLabel);
    
    m_frameTimeLabel = new QLabel("16.67 ms", perfGroup);
    perfLayout->addRow(tr("Frame Time:"), m_frameTimeLabel);
    
    m_cpuBar = new QProgressBar(perfGroup);
    m_cpuBar->setRange(0, 100);
    m_cpuBar->setValue(50);
    m_cpuBar->setTextVisible(true);
    m_cpuBar->setFormat("%p% (target: 60 FPS)");
    m_cpuBar->setMaximumHeight(16);
    perfLayout->addRow(tr("CPU Load:"), m_cpuBar);
    
    m_layout->addWidget(perfGroup);
    
    // Scene Group
    auto* sceneGroup = new QGroupBox(tr("Scene"), m_content);
    auto* sceneLayout = new QFormLayout(sceneGroup);
    sceneLayout->setSpacing(4);
    
    m_entityCountLabel = new QLabel("0", sceneGroup);
    sceneLayout->addRow(tr("Entities:"), m_entityCountLabel);
    
    m_componentCountLabel = new QLabel("0", sceneGroup);
    sceneLayout->addRow(tr("Components:"), m_componentCountLabel);
    
    m_meshCountLabel = new QLabel("0", sceneGroup);
    sceneLayout->addRow(tr("Meshes:"), m_meshCountLabel);
    
    m_lightCountLabel = new QLabel("0", sceneGroup);
    sceneLayout->addRow(tr("Lights:"), m_lightCountLabel);
    
    m_cameraCountLabel = new QLabel("0", sceneGroup);
    sceneLayout->addRow(tr("Cameras:"), m_cameraCountLabel);
    
    m_layout->addWidget(sceneGroup);
    
    // Render Group
    auto* renderGroup = new QGroupBox(tr("Rendering"), m_content);
    auto* renderLayout = new QFormLayout(renderGroup);
    renderLayout->setSpacing(4);
    
    m_drawCallsLabel = new QLabel("0", renderGroup);
    renderLayout->addRow(tr("Draw Calls:"), m_drawCallsLabel);
    
    m_trianglesLabel = new QLabel("0", renderGroup);
    renderLayout->addRow(tr("Triangles:"), m_trianglesLabel);
    
    m_verticesLabel = new QLabel("0", renderGroup);
    renderLayout->addRow(tr("Vertices:"), m_verticesLabel);
    
    m_layout->addWidget(renderGroup);
    
    m_layout->addStretch();
    
    setWidget(m_content);
}

QString EditorStatisticsPanel::formatMemory(size_t bytes) const
{
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    
    const double value = static_cast<double>(bytes);
    
    if (value >= gb) {
        return QString("%1 GB").arg(value / gb, 0, 'f', 2);
    } else if (value >= mb) {
        return QString("%1 MB").arg(value / mb, 0, 'f', 2);
    } else if (value >= kb) {
        return QString("%1 KB").arg(value / kb, 0, 'f', 1);
    }
    return QString("%1 B").arg(bytes);
}

QString EditorStatisticsPanel::formatNumber(int number) const
{
    if (number >= 1000000) {
        return QString("%1M").arg(number / 1000000.0, 0, 'f', 1);
    } else if (number >= 1000) {
        return QString("%1K").arg(number / 1000.0, 0, 'f', 1);
    }
    return QString::number(number);
}

} // namespace Aetherion::Editor
