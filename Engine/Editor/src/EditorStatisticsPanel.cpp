#include "Aetherion/Editor/EditorStatisticsPanel.h"
#include "Aetherion/Editor/EditorTheme.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <vector>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Core/Types.h"
#include "Aetherion/Scene/AIBehaviorComponent.h"
#include "Aetherion/Scene/AnimatorComponent.h"
#include "Aetherion/Scene/AudioListenerComponent.h"
#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/ParticleEmitterComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/ScriptComponent.h"
#include "Aetherion/Scene/SemanticComponent.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Editor {
namespace {
struct Vec3 {
  float x;
  float y;
  float z;
};

Vec3 ToVec3(const std::array<float, 3> &value) {
  return {value[0], value[1], value[2]};
}

Vec3 Min(const Vec3 &a, const Vec3 &b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

Vec3 Max(const Vec3 &a, const Vec3 &b) {
  return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

struct Bounds {
  bool valid{false};
  Vec3 min{};
  Vec3 max{};
};

void ExpandBounds(Bounds &bounds, const Vec3 &point) {
  if (!bounds.valid) {
    bounds.valid = true;
    bounds.min = point;
    bounds.max = point;
    return;
  }
  bounds.min = Min(bounds.min, point);
  bounds.max = Max(bounds.max, point);
}

void Mat4Identity(float out[16]) {
  std::memset(out, 0, sizeof(float) * 16);
  out[0] = 1.0f;
  out[5] = 1.0f;
  out[10] = 1.0f;
  out[15] = 1.0f;
}

void Mat4Mul(float out[16], const float a[16], const float b[16]) {
  float r[16];
  for (int c = 0; c < 4; ++c) {
    for (int rIdx = 0; rIdx < 4; ++rIdx) {
      r[c * 4 + rIdx] =
          a[0 * 4 + rIdx] * b[c * 4 + 0] + a[1 * 4 + rIdx] * b[c * 4 + 1] +
          a[2 * 4 + rIdx] * b[c * 4 + 2] + a[3 * 4 + rIdx] * b[c * 4 + 3];
    }
  }
  std::memcpy(out, r, sizeof(r));
}

void Mat4Translation(float out[16], float x, float y, float z) {
  Mat4Identity(out);
  out[12] = x;
  out[13] = y;
  out[14] = z;
}

void Mat4RotationZ(float out[16], float radians) {
  Mat4Identity(out);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  out[0] = c;
  out[4] = -s;
  out[1] = s;
  out[5] = c;
}

void Mat4RotationX(float out[16], float radians) {
  Mat4Identity(out);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  out[5] = c;
  out[9] = -s;
  out[6] = s;
  out[10] = c;
}

void Mat4RotationY(float out[16], float radians) {
  Mat4Identity(out);
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  out[0] = c;
  out[8] = s;
  out[2] = -s;
  out[10] = c;
}

void Mat4Scale(float out[16], float x, float y, float z) {
  Mat4Identity(out);
  out[0] = x;
  out[5] = y;
  out[10] = z;
}

std::array<float, 16>
BuildLocalMatrix(const Scene::TransformComponent &transform) {
  float t[16];
  float rx[16];
  float ry[16];
  float rz[16];
  float rzy[16];
  float r[16];
  float s[16];
  float tr[16];
  float local[16];
  Mat4Translation(t, transform.GetPositionX(), transform.GetPositionY(),
                  transform.GetPositionZ());
  Mat4RotationX(rx, transform.GetRotationXDegrees() *
                        (3.14159265358979323846f / 180.0f));
  Mat4RotationY(ry, transform.GetRotationYDegrees() *
                        (3.14159265358979323846f / 180.0f));
  Mat4RotationZ(rz, transform.GetRotationZDegrees() *
                        (3.14159265358979323846f / 180.0f));
  Mat4Mul(rzy, rz, ry);
  Mat4Mul(r, rzy, rx);
  Mat4Scale(s, transform.GetScaleX(), transform.GetScaleY(),
            transform.GetScaleZ());
  Mat4Mul(tr, t, r);
  Mat4Mul(local, tr, s);
  std::array<float, 16> out{};
  std::memcpy(out.data(), local, sizeof(local));
  return out;
}

std::array<float, 16>
GetWorldMatrix(const Scene::Scene &scene, Core::EntityId id,
               std::unordered_map<Core::EntityId, std::array<float, 16>> &cache) {
  auto it = cache.find(id);
  if (it != cache.end()) {
    return it->second;
  }

  std::array<float, 16> identity{};
  Mat4Identity(identity.data());

  auto entity = scene.FindEntityById(id);
  if (!entity) {
    cache.emplace(id, identity);
    return identity;
  }

  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform) {
    cache.emplace(id, identity);
    return identity;
  }

  auto local = BuildLocalMatrix(*transform);
  if (!transform->HasParent()) {
    cache.emplace(id, local);
    return local;
  }

  auto parent = GetWorldMatrix(scene, transform->GetParentId(), cache);
  float world[16];
  Mat4Mul(world, parent.data(), local.data());
  std::array<float, 16> out{};
  std::memcpy(out.data(), world, sizeof(world));
  cache.emplace(id, out);
  return out;
}

Vec3 TransformPoint(const std::array<float, 16> &m, const Vec3 &p) {
  return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
          m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
          m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}

void AccumulateMeshBounds(const Assets::AssetRegistry::MeshData &mesh,
                          const std::array<float, 16> &world,
                          Bounds &bounds) {
  const Vec3 min = ToVec3(mesh.boundsMin);
  const Vec3 max = ToVec3(mesh.boundsMax);
  const std::array<Vec3, 8> corners = {
      Vec3{min.x, min.y, min.z}, Vec3{max.x, min.y, min.z},
      Vec3{min.x, max.y, min.z}, Vec3{max.x, max.y, min.z},
      Vec3{min.x, min.y, max.z}, Vec3{max.x, min.y, max.z},
      Vec3{min.x, max.y, max.z}, Vec3{max.x, max.y, max.z}};
  for (const auto &corner : corners) {
    ExpandBounds(bounds, TransformPoint(world, corner));
  }
}

size_t EstimateMeshMemory(const Assets::AssetRegistry::MeshData &mesh) {
  size_t total = 0;
  total += mesh.positions.size() * sizeof(float) * 3;
  total += mesh.normals.size() * sizeof(float) * 3;
  total += mesh.colors.size() * sizeof(float) * 4;
  total += mesh.uvs.size() * sizeof(float) * 2;
  total += mesh.tangents.size() * sizeof(float) * 4;
  total += mesh.indices.size() * sizeof(std::uint32_t);
  return total;
}

size_t SafeFileSize(const std::filesystem::path &path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return 0;
  }
  const auto size = std::filesystem::file_size(path, ec);
  return ec ? 0 : static_cast<size_t>(size);
}

QString FormatLightTypes(int directional, int point, int spot) {
  return QString("Dir %1 / Point %2 / Spot %3")
      .arg(directional)
      .arg(point)
      .arg(spot);
}

QString BudgetStatusColor(float ratio) {
  if (ratio > 1.3f) {
    return EditorTheme::Hex(EditorTheme::Semantic::Error);
  }
  if (ratio > 1.0f) {
    return EditorTheme::Hex(EditorTheme::Semantic::Warning);
  }
  return EditorTheme::Hex(EditorTheme::Semantic::Success);
}

QString StabilityStatusColor(int stabilityPercent) {
  if (stabilityPercent < 60) {
    return EditorTheme::Hex(EditorTheme::Semantic::Error);
  }
  if (stabilityPercent < 80) {
    return EditorTheme::Hex(EditorTheme::Semantic::Warning);
  }
  return EditorTheme::Hex(EditorTheme::Semantic::Success);
}

QString ComplexityStatusColor(float complexityScore) {
  if (complexityScore >= 70.0f) {
    return EditorTheme::Hex(EditorTheme::Semantic::Error);
  }
  if (complexityScore >= 20.0f) {
    return EditorTheme::Hex(EditorTheme::Semantic::Warning);
  }
  return EditorTheme::Hex(EditorTheme::Semantic::Success);
}

QString FormatRigidbodyCounts(int total, int statics, int kinematics,
                              int dynamics) {
  return QString("%1 (S %2 / K %3 / D %4)")
      .arg(total)
      .arg(statics)
      .arg(kinematics)
      .arg(dynamics);
}

QString FormatColliderCounts(int total, int boxes, int spheres, int capsules,
                             int triggers) {
  return QString("%1 (Box %2 / Sphere %3 / Capsule %4 / Trigger %5)")
      .arg(total)
      .arg(boxes)
      .arg(spheres)
      .arg(capsules)
      .arg(triggers);
}
} // namespace

EditorStatisticsPanel::EditorStatisticsPanel(QWidget *parent)
    : QDockWidget(tr("Statistics"), parent) {
  setObjectName("StatisticsPanel");
  setAllowedAreas(Qt::AllDockWidgetAreas);

  setupUI();

  m_refreshTimer = new QTimer(this);
  connect(m_refreshTimer, &QTimer::timeout, this,
          &EditorStatisticsPanel::RefreshStats);
  m_refreshTimer->start(500);
  m_assetRefreshTimer.start();
}

void EditorStatisticsPanel::SetScene(std::shared_ptr<Scene::Scene> scene) {
  m_scene = std::move(scene);
  m_meshCache.clear();
  RefreshStats();
}

void EditorStatisticsPanel::SetAssetRegistry(
    std::shared_ptr<Assets::AssetRegistry> registry) {
  m_assetRegistry = std::move(registry);
  m_meshCache.clear();
  RefreshStats();
}

void EditorStatisticsPanel::SetTargetFrameTime(float targetFrameTimeMs) {
  m_targetFrameTimeMs = std::max(1.0f, targetFrameTimeMs);
  RefreshStats();
}

void EditorStatisticsPanel::UpdateFrameProfile(double cpuMs, double gpuMs,
                                               bool valid) {
  if (!valid) {
    m_hasGpuStats = false;
    m_lastCpuMs = 0.0;
    m_lastGpuMs = 0.0;
    return;
  }

  m_hasGpuStats = true;
  m_lastCpuMs = cpuMs;
  m_lastGpuMs = gpuMs;
}

void EditorStatisticsPanel::UpdateFrameTime(float frameTimeMs) {
  m_lastFrameTime = frameTimeMs;

  constexpr size_t kHistory = 180;
  m_frameHistory.push_back(frameTimeMs);
  if (m_frameHistory.size() > kHistory) {
    m_frameHistory.pop_front();
  }
}

void EditorStatisticsPanel::UpdateRenderStats(int drawCalls, int triangles,
                                              int vertices) {
  m_lastDrawCalls = drawCalls;
  m_lastTriangles = triangles;
  m_lastVertices = vertices;
}

void EditorStatisticsPanel::ResetMetrics() {
  m_frameHistory.clear();
  m_avgFrameTime = 16.67f;
  m_lastFrameTime = 16.67f;
  m_lastCpuMs = 0.0;
  m_lastGpuMs = 0.0;
  m_hasGpuStats = false;
  RefreshStats();
}

QString EditorStatisticsPanel::BuildSummaryText() const {
  if (m_lastSummaryText.isEmpty()) {
    return tr("Statistics not available yet.");
  }
  return m_lastSummaryText;
}

QString EditorStatisticsPanel::BuildSummaryJson() const {
  if (m_lastSummaryJson.isEmpty()) {
    return QStringLiteral("{}");
  }
  return m_lastSummaryJson;
}

void EditorStatisticsPanel::RefreshStats() {
  const bool hasHistory = !m_frameHistory.empty();
  const float avgFrameTime =
      hasHistory ? std::accumulate(m_frameHistory.begin(), m_frameHistory.end(),
                                   0.0f) /
                       static_cast<float>(m_frameHistory.size())
                 : m_lastFrameTime;

  float minFrameTime = avgFrameTime;
  float maxFrameTime = avgFrameTime;
  float p95FrameTime = avgFrameTime;
  float jitterMs = 0.0f;

  if (hasHistory) {
    minFrameTime = *std::min_element(m_frameHistory.begin(),
                                     m_frameHistory.end());
    maxFrameTime = *std::max_element(m_frameHistory.begin(),
                                     m_frameHistory.end());
    std::vector<float> sorted(m_frameHistory.begin(), m_frameHistory.end());
    std::sort(sorted.begin(), sorted.end());
    const size_t p95Index =
        static_cast<size_t>(std::ceil(0.95 * sorted.size())) - 1;
    p95FrameTime = sorted[std::min(p95Index, sorted.size() - 1)];

    float variance = 0.0f;
    for (float value : m_frameHistory) {
      const float diff = value - avgFrameTime;
      variance += diff * diff;
    }
    variance /= static_cast<float>(m_frameHistory.size());
    jitterMs = std::sqrt(variance);
  }

  m_avgFrameTime = avgFrameTime;

  const float fps = avgFrameTime > 0.0f ? 1000.0f / avgFrameTime : 0.0f;
  m_fpsLabel->setText(QString("%1 FPS").arg(fps, 0, 'f', 1));
  m_frameTimeLabel->setText(QString("%1 ms").arg(avgFrameTime, 0, 'f', 2));
  m_frameRangeLabel->setText(
      QString("%1 - %2 ms")
          .arg(minFrameTime, 0, 'f', 2)
          .arg(maxFrameTime, 0, 'f', 2));
  m_p95FrameLabel->setText(QString("%1 ms").arg(p95FrameTime, 0, 'f', 2));
  m_jitterLabel->setText(QString("%1 ms").arg(jitterMs, 0, 'f', 2));

  const QString cpuText =
      m_hasGpuStats ? QString("%1 ms").arg(m_lastCpuMs, 0, 'f', 2) : tr("--");
  const QString gpuText =
      m_hasGpuStats ? QString("%1 ms").arg(m_lastGpuMs, 0, 'f', 2) : tr("--");
  m_cpuTimeLabel->setText(cpuText);
  m_gpuTimeLabel->setText(gpuText);

  const float budgetRatio =
      m_targetFrameTimeMs > 0.0f ? (avgFrameTime / m_targetFrameTimeMs) : 0.0f;
  const int budgetPercent =
      std::clamp(static_cast<int>(budgetRatio * 100.0f), 0, 200);
  m_budgetBar->setValue(budgetPercent);
  m_budgetBar->setFormat(QString("%1% of target (%2 ms)")
                             .arg(budgetPercent)
                             .arg(m_targetFrameTimeMs, 0, 'f', 1));

  const QString budgetColor = BudgetStatusColor(budgetRatio);
  m_budgetBar->setStyleSheet(
      QString("QProgressBar::chunk { background-color: %1; }")
          .arg(budgetColor));

  const float stability =
      avgFrameTime > 0.0f
          ? std::clamp(1.0f - (jitterMs / avgFrameTime), 0.0f, 1.0f)
          : 1.0f;
  const int stabilityPercent =
      std::clamp(static_cast<int>(stability * 100.0f), 0, 100);
  m_stabilityBar->setValue(stabilityPercent);
  m_stabilityBar->setFormat(QString("%1% stable").arg(stabilityPercent));

  const QString stabilityColor = StabilityStatusColor(stabilityPercent);
  m_stabilityBar->setStyleSheet(
      QString("QProgressBar::chunk { background-color: %1; }")
          .arg(stabilityColor));

  int entityCount = 0;
  int componentCount = 0;
  int meshCount = 0;
  int lightCount = 0;
  int cameraCount = 0;
  int animatorCount = 0;
  int skeletonCount = 0;
  int boneCount = 0;
  int scriptCount = 0;
  int aiBehaviorCount = 0;
  int semanticCount = 0;
  int particleEmitterCount = 0;
  int particleActiveCount = 0;
  int particleMaxCount = 0;
  int rigidbodyCount = 0;
  int rigidbodyStaticCount = 0;
  int rigidbodyKinematicCount = 0;
  int rigidbodyDynamicCount = 0;
  int colliderCount = 0;
  int colliderBoxCount = 0;
  int colliderSphereCount = 0;
  int colliderCapsuleCount = 0;
  int colliderTriggerCount = 0;
  int audioSourceCount = 0;
  int audioListenerCount = 0;
  int directionalLights = 0;
  int pointLights = 0;
  int spotLights = 0;

  long long totalTriangles = 0;
  long long totalVertices = 0;
  size_t uniqueMeshMemory = 0;
  int uniqueMeshes = 0;
  int drawCallEstimate = 0;

  Bounds bounds;
  std::unordered_map<Core::EntityId, std::array<float, 16>> worldCache;
  std::unordered_map<std::string, int> meshInstances;

  if (m_scene) {
    const auto &entities = m_scene->GetEntities();
    for (const auto &entity : entities) {
      if (!entity) {
        continue;
      }
      entityCount++;
      componentCount += static_cast<int>(entity->GetComponents().size());

      if (entity->GetComponent<Scene::MeshRendererComponent>()) {
        meshCount++;
        auto mesh = entity->GetComponent<Scene::MeshRendererComponent>();
        if (mesh && !mesh->GetMeshAssetId().empty()) {
          meshInstances[mesh->GetMeshAssetId()]++;
        }
      }

      if (auto light = entity->GetComponent<Scene::LightComponent>()) {
        lightCount++;
        switch (light->GetType()) {
        case Scene::LightComponent::LightType::Directional:
          directionalLights++;
          break;
        case Scene::LightComponent::LightType::Point:
          pointLights++;
          break;
        case Scene::LightComponent::LightType::Spot:
          spotLights++;
          break;
        }
      }
      if (entity->GetComponent<Scene::CameraComponent>()) {
        cameraCount++;
      }
      if (entity->GetComponent<Scene::AnimatorComponent>()) {
        animatorCount++;
      }
      if (auto skeleton = entity->GetComponent<Scene::SkeletonComponent>()) {
        skeletonCount++;
        if (auto skelData = skeleton->GetSkeleton()) {
          boneCount += static_cast<int>(skelData->GetBoneCount());
        }
      }
      if (entity->GetComponent<Scene::ScriptComponent>()) {
        scriptCount++;
      }
      if (entity->GetComponent<Scene::AIBehaviorComponent>()) {
        aiBehaviorCount++;
      }
      if (entity->GetComponent<Scene::SemanticComponent>()) {
        semanticCount++;
      }
      if (auto emitter = entity->GetComponent<Scene::ParticleEmitterComponent>()) {
        particleEmitterCount++;
        particleActiveCount += static_cast<int>(emitter->GetActiveParticleCount());
        particleMaxCount += static_cast<int>(emitter->GetMaxParticles());
      }
      if (auto rigidbody = entity->GetComponent<Scene::RigidbodyComponent>()) {
        rigidbodyCount++;
        switch (rigidbody->GetMotionType()) {
        case Physics::MotionType::Static:
          rigidbodyStaticCount++;
          break;
        case Physics::MotionType::Kinematic:
          rigidbodyKinematicCount++;
          break;
        case Physics::MotionType::Dynamic:
          rigidbodyDynamicCount++;
          break;
        }
      }
      if (auto collider = entity->GetComponent<Scene::ColliderComponent>()) {
        colliderCount++;
        if (collider->IsTrigger()) {
          colliderTriggerCount++;
        }
        switch (collider->GetShapeType()) {
        case Physics::ShapeType::Box:
          colliderBoxCount++;
          break;
        case Physics::ShapeType::Sphere:
          colliderSphereCount++;
          break;
        case Physics::ShapeType::Capsule:
          colliderCapsuleCount++;
          break;
        }
      }
      if (entity->GetComponent<Scene::AudioSourceComponent>()) {
        audioSourceCount++;
      }
      if (entity->GetComponent<Scene::AudioListenerComponent>()) {
        audioListenerCount++;
      }

      auto transform = entity->GetComponent<Scene::TransformComponent>();
      if (!transform || !m_scene) {
        continue;
      }

      const auto world = GetWorldMatrix(*m_scene, entity->GetId(), worldCache);
      bool usedMeshBounds = false;
      if (auto mesh = entity->GetComponent<Scene::MeshRendererComponent>()) {
        if (m_assetRegistry && !mesh->GetMeshAssetId().empty()) {
          if (const auto *meshData =
                  m_assetRegistry->LoadMeshData(mesh->GetMeshAssetId())) {
            AccumulateMeshBounds(*meshData, world, bounds);
            usedMeshBounds = true;
          }
        }
      }
      if (!usedMeshBounds) {
        ExpandBounds(bounds, {world[12], world[13], world[14]});
      }
    }
  }

  drawCallEstimate = meshCount;

  if (m_assetRegistry && !meshInstances.empty()) {
    for (const auto &entry : meshInstances) {
      const std::string &meshId = entry.first;
      const int instances = entry.second;
      auto cacheIt = m_meshCache.find(meshId);
      if (cacheIt == m_meshCache.end() || !cacheIt->second.valid) {
        MeshCacheEntry cached;
        if (const auto *meshData = m_assetRegistry->LoadMeshData(meshId)) {
          cached.vertices = static_cast<long long>(meshData->positions.size());
          if (!meshData->indices.empty()) {
            cached.triangles =
                static_cast<long long>(meshData->indices.size() / 3);
          } else {
            cached.triangles =
                static_cast<long long>(meshData->positions.size() / 3);
          }
          cached.memoryBytes = EstimateMeshMemory(*meshData);
          cached.valid = true;
        }
        cacheIt = m_meshCache.insert_or_assign(meshId, cached).first;
      }

      if (cacheIt->second.valid) {
        uniqueMeshes++;
        uniqueMeshMemory += cacheIt->second.memoryBytes;
        totalVertices += cacheIt->second.vertices * instances;
        totalTriangles += cacheIt->second.triangles * instances;
      }
    }
  }

  m_entityCountLabel->setText(formatNumber(entityCount));
  m_componentCountLabel->setText(formatNumber(componentCount));
  m_meshCountLabel->setText(formatNumber(meshCount));
  m_lightCountLabel->setText(formatNumber(lightCount));
  m_lightTypeLabel->setText(
      FormatLightTypes(directionalLights, pointLights, spotLights));
  m_cameraCountLabel->setText(formatNumber(cameraCount));
  m_animatorCountLabel->setText(formatNumber(animatorCount));
  m_skeletonCountLabel->setText(formatNumber(skeletonCount));
  m_boneCountLabel->setText(formatNumber(boneCount));
  m_scriptCountLabel->setText(formatNumber(scriptCount));
  m_aiBehaviorCountLabel->setText(formatNumber(aiBehaviorCount));
  m_semanticCountLabel->setText(formatNumber(semanticCount));

  m_drawCallsLabel->setText(formatNumber(drawCallEstimate));
  m_trianglesLabel->setText(formatNumber(totalTriangles));
  m_verticesLabel->setText(formatNumber(totalVertices));
  m_uniqueMeshesLabel->setText(formatNumber(uniqueMeshes));
  m_meshMemoryLabel->setText(formatMemory(uniqueMeshMemory));

  m_rigidbodyLabel->setText(FormatRigidbodyCounts(
      rigidbodyCount, rigidbodyStaticCount, rigidbodyKinematicCount,
      rigidbodyDynamicCount));
  m_colliderLabel->setText(
      FormatColliderCounts(colliderCount, colliderBoxCount, colliderSphereCount,
                           colliderCapsuleCount, colliderTriggerCount));
  m_audioLabel->setText(
      QString("%1 src / %2 listener").arg(audioSourceCount).arg(audioListenerCount));
  m_particleLabel->setText(
      QString("%1 emitters (%2 / %3 particles)")
          .arg(particleEmitterCount)
          .arg(particleActiveCount)
          .arg(particleMaxCount));

  if (bounds.valid) {
    const Vec3 size = {bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y,
                       bounds.max.z - bounds.min.z};
    const float volume = std::max(0.0f, size.x * size.y * size.z);
    const float density = volume > 0.0f
                              ? static_cast<float>(entityCount) / volume
                              : 0.0f;
    m_boundsLabel->setText(
        QString("min (%1, %2, %3)  max (%4, %5, %6)")
            .arg(bounds.min.x, 0, 'f', 1)
            .arg(bounds.min.y, 0, 'f', 1)
            .arg(bounds.min.z, 0, 'f', 1)
            .arg(bounds.max.x, 0, 'f', 1)
            .arg(bounds.max.y, 0, 'f', 1)
            .arg(bounds.max.z, 0, 'f', 1));
    m_boundsSizeLabel->setText(QString("%1 x %2 x %3")
                                   .arg(size.x, 0, 'f', 1)
                                   .arg(size.y, 0, 'f', 1)
                                   .arg(size.z, 0, 'f', 1));
    m_boundsVolumeLabel->setText(QString("%1").arg(volume, 0, 'f', 1));
    m_densityLabel->setText(QString("%1 / unit^3").arg(density, 0, 'f', 3));
  } else {
    m_boundsLabel->setText(tr("--"));
    m_boundsSizeLabel->setText(tr("--"));
    m_boundsVolumeLabel->setText(tr("--"));
    m_densityLabel->setText(tr("--"));
  }

  const float complexityScore = std::clamp(
      (static_cast<float>(totalTriangles) / 20000.0f) +
          static_cast<float>(meshCount) * 0.6f +
          static_cast<float>(lightCount) * 4.0f +
          static_cast<float>(particleActiveCount) / 250.0f +
          static_cast<float>(rigidbodyCount + colliderCount) * 0.6f +
          static_cast<float>(scriptCount + aiBehaviorCount) * 0.7f,
      0.0f, 100.0f);

  QString complexityRating = tr("Low");
  QString complexityColor = ComplexityStatusColor(complexityScore);
  if (complexityScore >= 70.0f) {
    complexityRating = tr("Extreme");
  } else if (complexityScore >= 40.0f) {
    complexityRating = tr("High");
  } else if (complexityScore >= 20.0f) {
    complexityRating = tr("Moderate");
  }

  m_complexityBar->setValue(static_cast<int>(complexityScore));
  m_complexityBar->setFormat(
      QString("%1 / 100").arg(complexityScore, 0, 'f', 1));
  m_complexityBar->setStyleSheet(
      QString("QProgressBar::chunk { background-color: %1; }")
          .arg(complexityColor));
  m_complexityLabel->setText(QString("%1 (%2)")
                                 .arg(complexityRating)
                                 .arg(complexityScore, 0, 'f', 1));
  m_complexityLabel->setStyleSheet(QString("color: %1;").arg(complexityColor));

  if (!m_assetRegistry) {
    m_assetCountLabel->setText(tr("--"));
    m_assetSizeLabel->setText(tr("--"));
    m_textureAssetLabel->setText(tr("--"));
    m_meshAssetLabel->setText(tr("--"));
    m_audioAssetLabel->setText(tr("--"));
    m_scriptAssetLabel->setText(tr("--"));
    m_shaderAssetLabel->setText(tr("--"));
    m_animationAssetLabel->setText(tr("--"));
    m_skeletonAssetLabel->setText(tr("--"));
    m_sceneAssetLabel->setText(tr("--"));
    m_otherAssetLabel->setText(tr("--"));
  } else {
    if (!m_assetRefreshTimer.isValid() || m_assetRefreshTimer.elapsed() > 2500) {
      m_lastAssetTotals = {};
      for (const auto &entry : m_assetRegistry->GetEntries()) {
        const size_t size = SafeFileSize(entry.path);
        m_lastAssetTotals.totalCount++;
        m_lastAssetTotals.totalBytes += size;
        switch (entry.type) {
        case Assets::AssetRegistry::AssetType::Texture:
          m_lastAssetTotals.textureCount++;
          m_lastAssetTotals.textureBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Mesh:
          m_lastAssetTotals.meshCount++;
          m_lastAssetTotals.meshBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Audio:
          m_lastAssetTotals.audioCount++;
          m_lastAssetTotals.audioBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Script:
          m_lastAssetTotals.scriptCount++;
          m_lastAssetTotals.scriptBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Shader:
          m_lastAssetTotals.shaderCount++;
          m_lastAssetTotals.shaderBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Animation:
          m_lastAssetTotals.animationCount++;
          m_lastAssetTotals.animationBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Skeleton:
          m_lastAssetTotals.skeletonCount++;
          m_lastAssetTotals.skeletonBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Scene:
          m_lastAssetTotals.sceneCount++;
          m_lastAssetTotals.sceneBytes += size;
          break;
        case Assets::AssetRegistry::AssetType::Other:
        default:
          m_lastAssetTotals.otherCount++;
          m_lastAssetTotals.otherBytes += size;
          break;
        }
      }
      m_assetRefreshTimer.restart();
    }

    m_assetCountLabel->setText(formatNumber(m_lastAssetTotals.totalCount));
    m_assetSizeLabel->setText(formatMemory(m_lastAssetTotals.totalBytes));
    m_textureAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.textureCount))
            .arg(formatMemory(m_lastAssetTotals.textureBytes)));
    m_meshAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.meshCount))
            .arg(formatMemory(m_lastAssetTotals.meshBytes)));
    m_audioAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.audioCount))
            .arg(formatMemory(m_lastAssetTotals.audioBytes)));
    m_scriptAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.scriptCount))
            .arg(formatMemory(m_lastAssetTotals.scriptBytes)));
    m_shaderAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.shaderCount))
            .arg(formatMemory(m_lastAssetTotals.shaderBytes)));
    m_animationAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.animationCount))
            .arg(formatMemory(m_lastAssetTotals.animationBytes)));
    m_skeletonAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.skeletonCount))
            .arg(formatMemory(m_lastAssetTotals.skeletonBytes)));
    m_sceneAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.sceneCount))
            .arg(formatMemory(m_lastAssetTotals.sceneBytes)));
    m_otherAssetLabel->setText(
        QString("%1 (%2)")
            .arg(formatNumber(m_lastAssetTotals.otherCount))
            .arg(formatMemory(m_lastAssetTotals.otherBytes)));
  }

  QJsonObject summary;
  summary["fps"] = fps;
  summary["avgFrameMs"] = avgFrameTime;
  summary["p95FrameMs"] = p95FrameTime;
  summary["jitterMs"] = jitterMs;
  summary["cpuMs"] = m_hasGpuStats ? m_lastCpuMs : 0.0;
  summary["gpuMs"] = m_hasGpuStats ? m_lastGpuMs : 0.0;
  summary["entities"] = entityCount;
  summary["components"] = componentCount;
  summary["meshes"] = meshCount;
  summary["lights"] = lightCount;
  summary["cameras"] = cameraCount;
  summary["triangles"] = static_cast<double>(totalTriangles);
  summary["vertices"] = static_cast<double>(totalVertices);
  summary["complexityScore"] = complexityScore;
  summary["complexityRating"] = complexityRating;
  summary["assetCount"] = m_lastAssetTotals.totalCount;
  summary["assetBytes"] = static_cast<double>(m_lastAssetTotals.totalBytes);

  QJsonDocument doc(summary);
  m_lastSummaryJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

  m_lastSummaryText =
      tr("Performance: %1 FPS, avg %2 ms, p95 %3 ms, jitter %4 ms\n"
         "Scene: %5 entities, %6 components, %7 meshes, %8 lights\n"
         "Rendering: %9 draw calls (est), %10 tris, %11 verts\n"
         "Complexity: %12 (%13/100)\n"
         "Assets: %14 items, %15 total")
          .arg(fps, 0, 'f', 1)
          .arg(avgFrameTime, 0, 'f', 2)
          .arg(p95FrameTime, 0, 'f', 2)
          .arg(jitterMs, 0, 'f', 2)
          .arg(entityCount)
          .arg(componentCount)
          .arg(meshCount)
          .arg(lightCount)
          .arg(drawCallEstimate)
          .arg(formatNumber(totalTriangles))
          .arg(formatNumber(totalVertices))
          .arg(complexityRating)
          .arg(complexityScore, 0, 'f', 1)
          .arg(formatNumber(m_lastAssetTotals.totalCount))
          .arg(formatMemory(m_lastAssetTotals.totalBytes));
}

void EditorStatisticsPanel::setupUI() {
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);

  m_content = new QWidget(m_scrollArea);
  m_layout = new QVBoxLayout(m_content);
  m_layout->setContentsMargins(12, 12, 12, 12);
  m_layout->setSpacing(12);

  auto *actionRow = new QHBoxLayout();
  m_copyStatsButton = new QPushButton(tr("Copy Summary"), m_content);
  m_exportStatsButton = new QPushButton(tr("Export JSON"), m_content);
  m_resetStatsButton = new QPushButton(tr("Reset Metrics"), m_content);
  actionRow->addWidget(m_copyStatsButton);
  actionRow->addWidget(m_exportStatsButton);
  actionRow->addWidget(m_resetStatsButton);
  actionRow->addStretch();
  m_layout->addLayout(actionRow);

  connect(m_copyStatsButton, &QPushButton::clicked, this, [this]() {
    QApplication::clipboard()->setText(BuildSummaryText());
  });
  connect(m_exportStatsButton, &QPushButton::clicked, this, [this]() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export Statistics"), QStringLiteral("stats.json"),
        tr("JSON (*.json)"));
    if (path.isEmpty()) {
      return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      return;
    }
    file.write(BuildSummaryJson().toUtf8());
    file.close();
  });
  connect(m_resetStatsButton, &QPushButton::clicked, this,
          [this]() { ResetMetrics(); });

  auto *perfGroup = new QGroupBox(tr("Performance"), m_content);
  auto *perfLayout = new QFormLayout(perfGroup);
  perfLayout->setSpacing(6);
  perfLayout->setLabelAlignment(Qt::AlignLeft);

  m_fpsLabel = new QLabel("--", perfGroup);
  m_frameTimeLabel = new QLabel("--", perfGroup);
  m_frameRangeLabel = new QLabel("--", perfGroup);
  m_p95FrameLabel = new QLabel("--", perfGroup);
  m_jitterLabel = new QLabel("--", perfGroup);
  m_cpuTimeLabel = new QLabel("--", perfGroup);
  m_gpuTimeLabel = new QLabel("--", perfGroup);
  m_budgetBar = new QProgressBar(perfGroup);
  m_budgetBar->setRange(0, 200);
  m_budgetBar->setValue(0);
  m_budgetBar->setTextVisible(true);
  m_budgetBar->setMaximumHeight(18);
  m_stabilityBar = new QProgressBar(perfGroup);
  m_stabilityBar->setRange(0, 100);
  m_stabilityBar->setValue(100);
  m_stabilityBar->setTextVisible(true);
  m_stabilityBar->setMaximumHeight(18);

  perfLayout->addRow(tr("Frame Rate:"), m_fpsLabel);
  perfLayout->addRow(tr("Avg Frame:"), m_frameTimeLabel);
  perfLayout->addRow(tr("Frame Range:"), m_frameRangeLabel);
  perfLayout->addRow(tr("P95 Frame:"), m_p95FrameLabel);
  perfLayout->addRow(tr("Jitter:"), m_jitterLabel);
  perfLayout->addRow(tr("CPU Time:"), m_cpuTimeLabel);
  perfLayout->addRow(tr("GPU Time:"), m_gpuTimeLabel);
  perfLayout->addRow(tr("Frame Budget:"), m_budgetBar);
  perfLayout->addRow(tr("Stability:"), m_stabilityBar);
  m_layout->addWidget(perfGroup);

  auto *sceneGroup = new QGroupBox(tr("Scene Composition"), m_content);
  auto *sceneLayout = new QFormLayout(sceneGroup);
  sceneLayout->setSpacing(6);
  sceneLayout->setLabelAlignment(Qt::AlignLeft);

  m_entityCountLabel = new QLabel("0", sceneGroup);
  m_componentCountLabel = new QLabel("0", sceneGroup);
  m_meshCountLabel = new QLabel("0", sceneGroup);
  m_lightCountLabel = new QLabel("0", sceneGroup);
  m_lightTypeLabel = new QLabel("--", sceneGroup);
  m_cameraCountLabel = new QLabel("0", sceneGroup);
  m_animatorCountLabel = new QLabel("0", sceneGroup);
  m_skeletonCountLabel = new QLabel("0", sceneGroup);
  m_boneCountLabel = new QLabel("0", sceneGroup);
  m_scriptCountLabel = new QLabel("0", sceneGroup);
  m_aiBehaviorCountLabel = new QLabel("0", sceneGroup);
  m_semanticCountLabel = new QLabel("0", sceneGroup);

  sceneLayout->addRow(tr("Entities:"), m_entityCountLabel);
  sceneLayout->addRow(tr("Components:"), m_componentCountLabel);
  sceneLayout->addRow(tr("Meshes:"), m_meshCountLabel);
  sceneLayout->addRow(tr("Lights:"), m_lightCountLabel);
  sceneLayout->addRow(tr("Light Types:"), m_lightTypeLabel);
  sceneLayout->addRow(tr("Cameras:"), m_cameraCountLabel);
  sceneLayout->addRow(tr("Animators:"), m_animatorCountLabel);
  sceneLayout->addRow(tr("Skeletons:"), m_skeletonCountLabel);
  sceneLayout->addRow(tr("Bones:"), m_boneCountLabel);
  sceneLayout->addRow(tr("Scripts:"), m_scriptCountLabel);
  sceneLayout->addRow(tr("AI Behaviors:"), m_aiBehaviorCountLabel);
  sceneLayout->addRow(tr("Semantic Tags:"), m_semanticCountLabel);
  m_layout->addWidget(sceneGroup);

  auto *renderGroup = new QGroupBox(tr("Rendering"), m_content);
  auto *renderLayout = new QFormLayout(renderGroup);
  renderLayout->setSpacing(6);
  renderLayout->setLabelAlignment(Qt::AlignLeft);

  m_drawCallsLabel = new QLabel("0", renderGroup);
  m_trianglesLabel = new QLabel("0", renderGroup);
  m_verticesLabel = new QLabel("0", renderGroup);
  m_uniqueMeshesLabel = new QLabel("0", renderGroup);
  m_meshMemoryLabel = new QLabel("0", renderGroup);

  renderLayout->addRow(tr("Draw Calls (est):"), m_drawCallsLabel);
  renderLayout->addRow(tr("Triangles (est):"), m_trianglesLabel);
  renderLayout->addRow(tr("Vertices (est):"), m_verticesLabel);
  renderLayout->addRow(tr("Unique Meshes:"), m_uniqueMeshesLabel);
  renderLayout->addRow(tr("Mesh Data Size:"), m_meshMemoryLabel);
  m_layout->addWidget(renderGroup);

  auto *simulationGroup = new QGroupBox(tr("Simulation"), m_content);
  auto *simLayout = new QFormLayout(simulationGroup);
  simLayout->setSpacing(6);
  simLayout->setLabelAlignment(Qt::AlignLeft);

  m_rigidbodyLabel = new QLabel("--", simulationGroup);
  m_colliderLabel = new QLabel("--", simulationGroup);
  m_audioLabel = new QLabel("--", simulationGroup);
  m_particleLabel = new QLabel("--", simulationGroup);

  simLayout->addRow(tr("Rigidbodies:"), m_rigidbodyLabel);
  simLayout->addRow(tr("Colliders:"), m_colliderLabel);
  simLayout->addRow(tr("Audio:"), m_audioLabel);
  simLayout->addRow(tr("Particles:"), m_particleLabel);
  m_layout->addWidget(simulationGroup);

  auto *spatialGroup = new QGroupBox(tr("Spatial"), m_content);
  auto *spatialLayout = new QFormLayout(spatialGroup);
  spatialLayout->setSpacing(6);
  spatialLayout->setLabelAlignment(Qt::AlignLeft);

  m_boundsLabel = new QLabel("--", spatialGroup);
  m_boundsSizeLabel = new QLabel("--", spatialGroup);
  m_boundsVolumeLabel = new QLabel("--", spatialGroup);
  m_densityLabel = new QLabel("--", spatialGroup);

  spatialLayout->addRow(tr("Bounds:"), m_boundsLabel);
  spatialLayout->addRow(tr("Size:"), m_boundsSizeLabel);
  spatialLayout->addRow(tr("Volume:"), m_boundsVolumeLabel);
  spatialLayout->addRow(tr("Density:"), m_densityLabel);
  m_layout->addWidget(spatialGroup);

  auto *complexityGroup = new QGroupBox(tr("Complexity Model"), m_content);
  auto *complexityLayout = new QVBoxLayout(complexityGroup);
  m_complexityLabel = new QLabel("--", complexityGroup);
  m_complexityBar = new QProgressBar(complexityGroup);
  m_complexityBar->setRange(0, 100);
  m_complexityBar->setValue(0);
  m_complexityBar->setTextVisible(true);
  m_complexityBar->setMaximumHeight(18);
  complexityLayout->addWidget(m_complexityLabel);
  complexityLayout->addWidget(m_complexityBar);
  m_layout->addWidget(complexityGroup);

  auto *assetGroup = new QGroupBox(tr("Assets"), m_content);
  auto *assetLayout = new QFormLayout(assetGroup);
  assetLayout->setSpacing(6);
  assetLayout->setLabelAlignment(Qt::AlignLeft);

  m_assetCountLabel = new QLabel("--", assetGroup);
  m_assetSizeLabel = new QLabel("--", assetGroup);
  m_textureAssetLabel = new QLabel("--", assetGroup);
  m_meshAssetLabel = new QLabel("--", assetGroup);
  m_audioAssetLabel = new QLabel("--", assetGroup);
  m_scriptAssetLabel = new QLabel("--", assetGroup);
  m_shaderAssetLabel = new QLabel("--", assetGroup);
  m_animationAssetLabel = new QLabel("--", assetGroup);
  m_skeletonAssetLabel = new QLabel("--", assetGroup);
  m_sceneAssetLabel = new QLabel("--", assetGroup);
  m_otherAssetLabel = new QLabel("--", assetGroup);

  assetLayout->addRow(tr("Total Assets:"), m_assetCountLabel);
  assetLayout->addRow(tr("Total Size:"), m_assetSizeLabel);
  assetLayout->addRow(tr("Textures:"), m_textureAssetLabel);
  assetLayout->addRow(tr("Meshes:"), m_meshAssetLabel);
  assetLayout->addRow(tr("Audio:"), m_audioAssetLabel);
  assetLayout->addRow(tr("Scripts:"), m_scriptAssetLabel);
  assetLayout->addRow(tr("Shaders:"), m_shaderAssetLabel);
  assetLayout->addRow(tr("Animations:"), m_animationAssetLabel);
  assetLayout->addRow(tr("Skeletons:"), m_skeletonAssetLabel);
  assetLayout->addRow(tr("Scenes:"), m_sceneAssetLabel);
  assetLayout->addRow(tr("Other:"), m_otherAssetLabel);
  m_layout->addWidget(assetGroup);

  m_layout->addStretch();

  m_content->setLayout(m_layout);
  m_scrollArea->setWidget(m_content);
  setWidget(m_scrollArea);
}

QString EditorStatisticsPanel::formatMemory(size_t bytes) const {
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

QString EditorStatisticsPanel::formatNumber(long long number) const {
  if (number >= 1000000) {
    return QString("%1M").arg(number / 1000000.0, 0, 'f', 1);
  } else if (number >= 1000) {
    return QString("%1K").arg(number / 1000.0, 0, 'f', 1);
  }
  return QString::number(number);
}

} // namespace Aetherion::Editor
