#include "Aetherion/Editor/EditorMainWindow.h"
#include "Aetherion/Editor/AICopilotPanel.h"
#include "Aetherion/Editor/AICopilotProcessor.h"
#include "Aetherion/Editor/EditorAssetGenerationPanel.h"
#include "Aetherion/Editor/EditorStatisticsPanel.h"
#include "Aetherion/Editor/EditorLogicCopilotPanel.h"
#include "Aetherion/Scripting/LogicCopilot.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QSysInfo>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QSet>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <deque>
#include <fstream>
#include <filesystem>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Assets/LLMClient.h"
#include "Aetherion/Editor/EditorAssetBrowser.h"
#include "Aetherion/Editor/EditorCameraPreview.h"
#include "Aetherion/Editor/EditorConsole.h"
#include "Aetherion/Editor/EditorCommandPalette.h"
#include "Aetherion/Editor/EditorHierarchyPanel.h"
#include "Aetherion/Editor/EditorInspectorPanel.h"
#include "Aetherion/Editor/EditorMeshPreview.h"
#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Editor/EditorSettingsDialog.h"
#include "Aetherion/Editor/EditorViewport.h"
#include "Aetherion/Editor/TabPanelManager.h"
#include "Aetherion/Rendering/RenderView.h"
#include "Aetherion/Rendering/VulkanContext.h"
#include "Aetherion/Rendering/VulkanViewport.h"
#include "Aetherion/Runtime/EngineApplication.h"

#include "Aetherion/Editor/Commands/EntityCommands.h"
#include "Aetherion/Editor/Commands/TransformCommand.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/SceneSerializer.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Scene/AIBehaviorComponent.h"
#include "nlohmann/json.hpp"

namespace Aetherion::Editor {
class EditorAuxPanel final : public QWidget {
public:
  explicit EditorAuxPanel(QWidget *parent = nullptr) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Quick Info"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(4);

    m_sceneValue = new QLabel(tr("--"), this);
    m_entityValue = new QLabel(tr("--"), this);
    m_assetValue = new QLabel(tr("--"), this);
    m_cameraValue = new QLabel(tr("--"), this);
    m_viewportValue = new QLabel(tr("--"), this);

    m_sceneValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_entityValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_assetValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_cameraValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_viewportValue->setTextInteractionFlags(Qt::TextSelectableByMouse);

    form->addRow(tr("Scene"), m_sceneValue);
    form->addRow(tr("Selection"), m_entityValue);
    form->addRow(tr("Asset"), m_assetValue);
    form->addRow(tr("Camera"), m_cameraValue);
    form->addRow(tr("Viewport"), m_viewportValue);

    layout->addLayout(form);

    auto *hints = new QLabel(tr("Shortcuts: F focus selection · Home reset "
                                "camera · Ctrl+F focus asset filter"),
                             this);
    hints->setWordWrap(true);
    hints->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(hints);

    layout->addStretch(1);
    setLayout(layout);
  }

  void SetSceneText(const QString &value) { m_sceneValue->setText(value); }
  void SetEntityText(const QString &value) { m_entityValue->setText(value); }
  void SetAssetText(const QString &value) { m_assetValue->setText(value); }
  void SetCameraText(const QString &value) { m_cameraValue->setText(value); }
  void SetViewportText(const QString &value) {
    m_viewportValue->setText(value);
  }

private:
  QLabel *m_sceneValue = nullptr;
  QLabel *m_entityValue = nullptr;
  QLabel *m_assetValue = nullptr;
  QLabel *m_cameraValue = nullptr;
  QLabel *m_viewportValue = nullptr;
};

namespace {
struct Vec3 {
  float x, y, z;
};

Vec3 operator-(const Vec3 &a, const Vec3 &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 operator+(const Vec3 &a, const Vec3 &b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 operator*(const Vec3 &a, float s) { return {a.x * s, a.y * s, a.z * s}; }
float Dot(const Vec3 &a, const Vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 Cross(const Vec3 &a, const Vec3 &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 Normalize(const Vec3 &v) {
  float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
  return l > 0 ? v * (1.0f / l) : v;
}

void SetCurrentTab(QTabWidget *tabs, QWidget *widget) {
  if (!tabs || !widget) {
    return;
  }
  const int index = tabs->indexOf(widget);
  if (index >= 0) {
    tabs->setCurrentIndex(index);
  }
}

void FocusBottomTab(TabPanelManager *manager, QWidget *widget) {
  if (!manager || !widget) {
    return;
  }
  manager->SetBottomPanelVisible(true);
  SetCurrentTab(manager->GetBottomPanel(), widget);
}

void FocusSideTab(TabPanelManager *manager, QWidget *widget, bool useRight) {   
  if (!manager || !widget) {
    return;
  }
  QTabWidget *tabs = useRight ? manager->GetRightPanel()
                              : manager->GetLeftPanel();
  SetCurrentTab(tabs, widget);
}

Assets::LLMProvider ConvertProvider(LLMProviderType provider) {
  switch (provider) {
  case LLMProviderType::OpenAI:
    return Assets::LLMProvider::OpenAI;
  case LLMProviderType::Anthropic:
    return Assets::LLMProvider::Anthropic;
  case LLMProviderType::StabilityAI:
    return Assets::LLMProvider::StabilityAI;
  case LLMProviderType::LocalOllama:
    return Assets::LLMProvider::LocalOllama;
  case LLMProviderType::Custom:
    return Assets::LLMProvider::Custom;
  default:
    return Assets::LLMProvider::OpenAI;
  }
}

bool BuildAIConfig(const LLMSettings &settings, Assets::LLMConfig &config) {
  if (settings.provider == LLMProviderType::None) {
    return false;
  }

  config.provider = ConvertProvider(settings.provider);
  config.apiKey = settings.apiKey;
  config.endpoint = settings.endpoint;
  config.model = settings.model;
  config.imageModel = settings.imageModel;
  config.timeoutMs = settings.timeoutMs;
  config.enableLogging = settings.enableLogging;

  if (config.endpoint.empty()) {
    config.endpoint = Assets::LLMConfig::GetDefaultEndpoint(config.provider);
  }
  if (config.model.empty()) {
    config.model = Assets::LLMConfig::GetDefaultModel(config.provider);
  }
  if (config.imageModel.empty()) {
    config.imageModel = Assets::LLMConfig::GetDefaultImageModel(config.provider);
  }

  return true;
}

// Returns t for L1 (P1 + t*D1) closest to L2 (P2 + s*D2)
float ClosestPointLineLine(const Vec3 &p1, const Vec3 &d1, const Vec3 &p2,      
                           const Vec3 &d2) {
  Vec3 r = p1 - p2;
  float a = Dot(d1, d1);
  float b = Dot(d1, d2);
  float c = Dot(d1, r);
  float e = Dot(d2, d2);
  float f = Dot(d2, r);
  float d = a * e - b * b;
  if (d != 0.0f) {
    return (b * f - e * c) / d; // Parameter for L1
  }
  return 0.0f;
}

// Distance between two lines
float DistLineLine(const Vec3 &p1, const Vec3 &d1, const Vec3 &p2,
                   const Vec3 &d2) {
  Vec3 w0 = p1 - p2;
  float a = Dot(d1, d1);
  float b = Dot(d1, d2);
  float c = Dot(d2, d2);
  float d = Dot(d1, w0);
  float e = Dot(d2, w0);
  float denom = a * c - b * b;
  if (denom < 1e-5f)
    return 0.0f; // Parallel
  float sc = (b * e - c * d) / denom;
  float tc = (a * e - b * d) / denom;
  Vec3 diff = w0 + (d1 * sc) - (d2 * tc);
  return std::sqrt(Dot(diff, diff));
}

Vec3 GetCameraEye(const EditorViewport *vp) {
  if (!vp)
    return {0, 0, 0};
  const float yawRad = vp->getCameraRotationY() * (3.14159265f / 180.0f);
  const float pitchRad = vp->getCameraRotationX() * (3.14159265f / 180.0f);
  const float distance = std::max(0.01f, 5.0f * vp->getCameraZoom());

  const float eyeX =
      vp->getCameraX() + distance * std::cos(pitchRad) * std::sin(yawRad);
  const float eyeY = vp->getCameraY() + distance * std::sin(pitchRad);
  const float eyeZ =
      vp->getCameraZ() + distance * std::cos(pitchRad) * std::cos(yawRad);
  return {eyeX, eyeY, eyeZ};
}

Vec3 GetCameraRayDir(const EditorViewport *vp, int mx, int my, int w, int h) {
  if (!vp || w <= 0 || h <= 0)
    return {0, 0, 1};
  Vec3 eye = GetCameraEye(vp);
  Vec3 center = {vp->getCameraX(), vp->getCameraY(), vp->getCameraZ()};
  Vec3 f = Normalize(center - eye);
  Vec3 up = {0.0f, 1.0f, 0.0f};
  Vec3 r = Normalize(Cross(f, up));
  Vec3 u = Cross(r, f);

  const float fovRad = 60.0f * (3.14159265f / 180.0f);
  const float aspect = static_cast<float>(w) / static_cast<float>(h);
  const float tanHalfFov = std::tan(fovRad * 0.5f);

  const float ndcX =
      (2.0f * (static_cast<float>(mx) + 0.5f) / static_cast<float>(w)) - 1.0f;
  const float ndcY =
      1.0f - (2.0f * (static_cast<float>(my) + 0.5f) / static_cast<float>(h));

  Vec3 dir = f + (r * (ndcX * aspect * tanHalfFov)) + (u * (ndcY * tanHalfFov));
  return Normalize(dir);
}

ConsoleSeverity ToConsoleSeverity(Rendering::LogSeverity severity) {
  switch (severity) {
  case Rendering::LogSeverity::Error:
    return ConsoleSeverity::Error;
  case Rendering::LogSeverity::Warning:
    return ConsoleSeverity::Warning;
  default:
    return ConsoleSeverity::Info;
  }
}

void AppendConsole(EditorConsole *console, const QString &message,
                   ConsoleSeverity severity) {
  if (console) {
    console->AppendMessage(message, severity);
  }
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

std::array<float, 4>
EulerDegreesToQuaternion(const std::array<float, 3> &euler) {
  constexpr float degToRad = 3.14159265358979323846f / 180.0f;
  const float x = euler[0] * degToRad * 0.5f;
  const float y = euler[1] * degToRad * 0.5f;
  const float z = euler[2] * degToRad * 0.5f;

  const float cx = std::cos(x);
  const float sx = std::sin(x);
  const float cy = std::cos(y);
  const float sy = std::sin(y);
  const float cz = std::cos(z);
  const float sz = std::sin(z);

  return {sx * cy * cz - cx * sy * sz, cx * sy * cz + sx * cy * sz,
          cx * cy * sz - sx * sy * cz, cx * cy * cz + sx * sy * sz};
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

std::array<float, 16> GetWorldMatrix(const Scene::Scene &scene,
                                     Core::EntityId id) {
  auto entity = scene.FindEntityById(id);
  if (!entity) {
    std::array<float, 16> identity{};
    Mat4Identity(identity.data());
    return identity;
  }

  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform) {
    std::array<float, 16> identity{};
    Mat4Identity(identity.data());
    return identity;
  }

  auto local = BuildLocalMatrix(*transform);
  if (!transform->HasParent()) {
    return local;
  }

  auto parent = GetWorldMatrix(scene, transform->GetParentId());
  float world[16];
  Mat4Mul(world, parent.data(), local.data());
  std::array<float, 16> out{};
  std::memcpy(out.data(), world, sizeof(world));
  return out;
}

std::array<float, 3> TransformPoint(const std::array<float, 16> &m,
                                    const std::array<float, 3> &p) {
  std::array<float, 3> out{};
  out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
  out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
  out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
  return out;
}

float ExtractMaxScale(const std::array<float, 16> &m) {
  const float sx = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
  const float sy = std::sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6]);
  const float sz = std::sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10]);
  return std::max(sx, std::max(sy, sz));
}

std::filesystem::path NormalizePath(const std::filesystem::path &path) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(path, ec);
  if (!ec) {
    return canonical;
  }
  return path.lexically_normal();
}

bool PathStartsWith(const std::filesystem::path &path,
                    const std::filesystem::path &prefix) {
  auto pathIt = path.begin();
  auto prefixIt = prefix.begin();
  for (; prefixIt != prefix.end(); ++prefixIt, ++pathIt) {
    if (pathIt == path.end() || *pathIt != *prefixIt) {
      return false;
    }
  }
  return true;
}

std::filesystem::path MakeUniquePath(const std::filesystem::path &base) {
  std::error_code ec;
  if (!std::filesystem::exists(base, ec)) {
    return base;
  }

  const auto dir = base.parent_path();
  const auto stem = base.stem().string();
  const auto ext = base.extension().string();
  for (int i = 1; i < 1000; ++i) {
    auto candidate = dir / (stem + "_" + std::to_string(i) + ext);
    if (!std::filesystem::exists(candidate, ec)) {
      return candidate;
    }
  }

  return base;
}

} // namespace

EditorMainWindow::EditorMainWindow(
    std::shared_ptr<Runtime::EngineApplication> runtimeApp,
    const EditorSettings &settings, QWidget *parent)
    : QMainWindow(parent), m_runtimeApp(std::move(runtimeApp)),
      m_settings(settings) {
  m_commandHistory = std::make_unique<CommandHistory>();
  m_copilotProcessor = std::make_unique<AICopilotProcessor>(
      [this](std::unique_ptr<Command> cmd) { ExecuteCommand(std::move(cmd)); });

  m_settings.Clamp();
  m_validationEnabled = m_settings.validationEnabled;
  m_renderLoggingEnabled = m_settings.verboseLogging;
  m_targetFrameIntervalMs =
      std::max(1, 1000 / std::max(1, m_settings.targetFps));
  m_headlessSleepMs = m_settings.headlessSleepMs;
  m_selection = new EditorSelection(this);
  ApplyRuntimeAISettings();
  InitializeCommandPalette();

  setWindowTitle("Aetherion Editor");
  setWindowIcon(QApplication::windowIcon());
  resize(1440, 900);

  m_panelManager = new TabPanelManager(this);
  m_viewport = new EditorViewport(m_panelManager);
  m_viewport->setFocusPolicy(Qt::StrongFocus);
  m_viewport->installEventFilter(this);
  if (m_viewport->surfaceWidget()) {
    m_viewport->surfaceWidget()->installEventFilter(this);
  }
  m_panelManager->SetCentralWidget(m_viewport);

  m_renderTimer = new QTimer(this);
  m_renderTimer->setInterval(m_targetFrameIntervalMs);
  connect(m_renderTimer, &QTimer::timeout, this, [this] {
    const bool viewportReady = m_vulkanViewport && m_vulkanViewport->IsReady();
    UpdateRenderTimerInterval(viewportReady);
    if (m_runtimeApp) {
      m_runtimeApp->Tick();
    }
    if (!viewportReady) {
      if (m_fpsLabel) {
        m_fpsLabel->setText(tr("FPS: --"));
      }
      m_fpsFrameCounter = 0;
      m_fpsTimer.invalidate();
      return;
    }
    if (isMinimized()) {
      if (m_fpsLabel) {
        m_fpsLabel->setText(tr("FPS: --"));
      }
      m_fpsFrameCounter = 0;
      m_fpsTimer.invalidate();
      return;
    }

    const qint64 nanos =
        m_frameTimer.isValid() ? m_frameTimer.nsecsElapsed() : 0;
    const float dt = static_cast<float>(nanos) / 1'000'000'000.0f;
    m_frameTimer.restart();

    if (m_viewport) {
      m_viewport->updateCamera(dt);
      // Smoothly interpolate interactive transform target if active
      UpdateInteractiveTransform(dt);
    }

    const bool useSceneCamera =
        m_modePlaytestAction && m_modePlaytestAction->isChecked();
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto renderView = ctx ? ctx->GetRenderView() : nullptr;
    Rendering::RenderView emptyView{};
    Rendering::RenderView viewCopy{};
    const Rendering::RenderView *activeView = &emptyView;
    if (renderView) {
      viewCopy = *renderView;
      viewCopy.selectedEntityId =
          (m_selection && m_selection->GetSelectedEntity())
              ? m_selection->GetSelectedEntity()->GetId()
              : 0;
      viewCopy.showEditorIcons = !useSceneCamera;
      if (!useSceneCamera) {
        viewCopy.camera.enabled = false;
      }
      activeView = &viewCopy;
    }
    try {
      m_vulkanViewport->RenderFrame(dt, *activeView);
    } catch (const std::exception &ex) {
      AppendConsole(m_console, QString::fromStdString(ex.what()),
                    ConsoleSeverity::Error);
      fprintf(stderr, "Render failed: %s\n", ex.what());
      m_renderTimer->stop();
      statusBar()->showMessage(
          tr("Renderer error: %1").arg(QString::fromStdString(ex.what())));
      return;
    }

    if (m_vulkanViewport) {
      const auto pick = m_vulkanViewport->GetLastPickResult();
      if (pick.valid) {
        if (m_selection) {
          if (pick.entityId != 0) {
            m_selection->SelectEntityById(pick.entityId);
          } else {
            m_selection->Clear();
          }
        }
        m_vulkanViewport->ClearPickResult();
      }
    }

    if (m_fpsLabel) {
      if (!m_fpsTimer.isValid()) {
        m_fpsTimer.start();
        m_fpsFrameCounter = 0;
      }

      ++m_fpsFrameCounter;
      const qint64 elapsedMs = m_fpsTimer.elapsed();
      if (elapsedMs >= 1000) {
        const double fps = (elapsedMs > 0)
                               ? (static_cast<double>(m_fpsFrameCounter) *
                                  1000.0 / static_cast<double>(elapsedMs))
                               : 0.0;
        m_fpsLabel->setText(tr("FPS: %1").arg(QString::number(fps, 'f', 1)));
        m_fpsFrameCounter = 0;
        m_fpsTimer.restart();
      }
    }
  });

  connect(
      m_viewport, &EditorViewport::surfaceReady, this,
      [this](WId nativeHandle, int width, int height) {
        m_surfaceHandle = nativeHandle;
        m_surfaceSize = QSize(width, height);
        m_surfaceInitialized = true;

        auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
        auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
        if (!vk) {
          return;
        }

        DestroyViewportRenderer();

        try {
          auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
          m_vulkanViewport =
              std::make_unique<Rendering::VulkanViewport>(vk, registry);
          m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
          m_vulkanViewport->Initialize(reinterpret_cast<void *>(nativeHandle),
                                       width, height);

          // Sync camera from viewport widget
          if (m_viewport) {
            m_vulkanViewport->SetCameraPosition(m_viewport->getCameraX(),
                                                m_viewport->getCameraY(),
                                                m_viewport->getCameraZ());
            m_vulkanViewport->SetCameraRotation(
                m_viewport->getCameraRotationY(),
                m_viewport->getCameraRotationX());
            m_vulkanViewport->SetCameraZoom(m_viewport->getCameraZoom());
          }
        } catch (const std::exception &ex) {
          AppendConsole(m_console, QString::fromStdString(ex.what()),
                        ConsoleSeverity::Error);
          statusBar()->showMessage(tr("Viewport renderer unavailable: %1")
                                       .arg(QString::fromStdString(ex.what())));
          return;
        }

        if (m_vulkanViewport->IsReady()) {
          m_frameTimer.start();
          m_renderTimer->start();
          m_fpsFrameCounter = 0;
          m_fpsTimer.start();
          statusBar()->showMessage(tr("Viewport Vulkan renderer active"));

          // Initialize mesh preview with shared Vulkan context
          if (m_meshPreview) {
            auto previewCtx =
                m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
            auto previewVk =
                previewCtx ? previewCtx->GetVulkanContext() : nullptr;
            auto previewRegistry =
                previewCtx ? previewCtx->GetAssetRegistry() : nullptr;
            m_meshPreview->SetVulkanContext(previewVk);
            m_meshPreview->SetAssetRegistry(previewRegistry);
          }
          if (m_cameraPreview) {
            auto previewCtx =
                m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
            auto previewVk =
                previewCtx ? previewCtx->GetVulkanContext() : nullptr;
            auto previewRegistry =
                previewCtx ? previewCtx->GetAssetRegistry() : nullptr;
            m_cameraPreview->SetVulkanContext(previewVk);
            m_cameraPreview->SetAssetRegistry(previewRegistry);
            m_cameraPreview->SetRenderViewSource(
                previewCtx ? previewCtx->GetRenderView() : nullptr);
            m_cameraPreview->SetScene(m_scene);
          }
        } else {
          statusBar()->showMessage(
              tr("Viewport renderer unavailable: swapchain not supported on "
                 "this adapter"));
        }
      });

  connect(m_viewport, &EditorViewport::surfaceResized, this,
          [this](int width, int height) {
            m_surfaceSize = QSize(width, height);
            if (m_vulkanViewport && m_vulkanViewport->IsReady()) {
              try {
                m_vulkanViewport->Resize(width, height);
              } catch (const std::exception &ex) {
                AppendConsole(m_console, QString::fromStdString(ex.what()),
                              ConsoleSeverity::Error);
                statusBar()->showMessage(
                    tr("Viewport resize failed: %1")
                        .arg(QString::fromStdString(ex.what())));
              }
            }

            if (m_auxPanel) {
              m_auxPanel->SetViewportText(tr("%1×%2").arg(width).arg(height));
            }
          });

  // Connect camera changes from viewport to renderer
  connect(m_viewport, &EditorViewport::cameraChanged, this, [this]() {
    if (m_vulkanViewport) {
      m_vulkanViewport->SetCameraPosition(m_viewport->getCameraX(),
                                          m_viewport->getCameraY(),
                                          m_viewport->getCameraZ());
      m_vulkanViewport->SetCameraRotation(m_viewport->getCameraRotationY(),
                                          m_viewport->getCameraRotationX());
      m_vulkanViewport->SetCameraZoom(m_viewport->getCameraZoom());
    }

    if (m_auxPanel && m_viewport) {
      m_auxPanel->SetCameraText(
          tr("pos(%1, %2, %3) rot(%4, %5) zoom %6")
              .arg(QString::number(m_viewport->getCameraX(), 'f', 2))
              .arg(QString::number(m_viewport->getCameraY(), 'f', 2))
              .arg(QString::number(m_viewport->getCameraZ(), 'f', 2))
              .arg(QString::number(m_viewport->getCameraRotationY(), 'f', 1))
              .arg(QString::number(m_viewport->getCameraRotationX(), 'f', 1))
              .arg(QString::number(m_viewport->getCameraZoom(), 'f', 2)));
    }
  });

  connect(m_viewport, &EditorViewport::focusRequested, this,
          [this]() { FocusCameraOnSelection(); });

  connect(
      m_viewport, &EditorViewport::gizmoDrag, this, [this](float dx, float dy) {
        if (!m_selection || !m_selection->GetSelectedEntity())
          return;

        const float rotateSpeed = 0.5f;
        const float scaleSpeed = 0.01f;

        if (m_gizmoMode == GizmoMode::Translate) {
          if (m_activeGizmoAxis == GizmoAxis::None) {
            return;
          }

          auto entity = m_selection->GetSelectedEntity();
          auto transform = entity->GetComponent<Scene::TransformComponent>();
          if (transform) {
            Vec3 origin = {transform->GetPositionX(), transform->GetPositionY(),
                           transform->GetPositionZ()};
            if (m_scene) {
              const auto world = GetWorldMatrix(*m_scene, entity->GetId());
              origin = {world[12], world[13], world[14]};
            }

            QPoint globalPos = QCursor::pos();
            QPoint localPos =
                m_viewport->surfaceWidget()->mapFromGlobal(globalPos);

            Vec3 rayOrigin = GetCameraEye(m_viewport);
            Vec3 rayDir =
                GetCameraRayDir(m_viewport, localPos.x(), localPos.y(),
                                m_viewport->width(), m_viewport->height());

            Vec3 axisDir = {0, 0, 0};
            if (m_activeGizmoAxis == GizmoAxis::X)
              axisDir = {1, 0, 0};
            else if (m_activeGizmoAxis == GizmoAxis::Y)
              axisDir = {0, 1, 0};
            else if (m_activeGizmoAxis == GizmoAxis::Z)
              axisDir = {0, 0, 1};

            float tCurr =
                ClosestPointLineLine(origin, axisDir, rayOrigin, rayDir);

            Vec3 prevRayDir =
                GetCameraRayDir(m_viewport, localPos.x() - static_cast<int>(dx),
                                localPos.y() - static_cast<int>(dy),
                                m_viewport->width(), m_viewport->height());
            float tPrev =
                ClosestPointLineLine(origin, axisDir, rayOrigin, prevRayDir);

            float delta = tCurr - tPrev;

            if (m_interactiveTransformActive) {
              if (m_activeGizmoAxis == GizmoAxis::X)
                UpdateInteractiveTransformTarget(delta, 0, 0);
              else if (m_activeGizmoAxis == GizmoAxis::Y)
                UpdateInteractiveTransformTarget(0, delta, 0);
              else if (m_activeGizmoAxis == GizmoAxis::Z)
                UpdateInteractiveTransformTarget(0, 0, delta);
            } else {
              if (m_activeGizmoAxis == GizmoAxis::X)
                ApplyTranslationDelta(delta, 0, 0);
              else if (m_activeGizmoAxis == GizmoAxis::Y)
                ApplyTranslationDelta(0, delta, 0);
              else if (m_activeGizmoAxis == GizmoAxis::Z)
                ApplyTranslationDelta(0, 0, delta);
            }
          }
        } else if (m_gizmoMode == GizmoMode::Rotate) {
          if (m_interactiveTransformActive) {
            UpdateInteractiveTransformTarget(
                0, 0, 0); // rotation handled separately if needed
          } else {
            ApplyRotationDelta(dx * rotateSpeed);
          }
        } else if (m_gizmoMode == GizmoMode::Scale) {
          if (m_interactiveTransformActive) {
            UpdateInteractiveTransformTarget(
                0, 0,
                0); // scaling during interactive drag not implemented here
          } else {
            ApplyScaleDelta(-dy * scaleSpeed);
          }
        }
      });

  connect(m_viewport, &EditorViewport::gizmoDragStarted, this, [this]() {
    if (m_gizmoMode != GizmoMode::Translate ||
        m_activeGizmoAxis == GizmoAxis::None) {
      return;
    }
    BeginInteractiveTransform();
  });

  connect(m_viewport, &EditorViewport::gizmoDragEnded, this,
          [this]() { EndInteractiveTransform(); });

  setCentralWidget(m_panelManager);

  CreateTabPanels();
  LoadBookmarks();
  RefreshBookmarksList();
  CreateMenuBarContent();
  CreateToolBarContent();
  ConfigureStatusBar();
  LoadLayout();

  if (m_hierarchyPanel) {
    m_hierarchyPanel->SetSelectionModel(m_selection);
  }

  m_scene = m_runtimeApp ? m_runtimeApp->GetActiveScene() : nullptr;
  if (m_selection) {
    m_selection->SetActiveScene(m_scene);
  }
  if (m_scene && m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
  }
  if (m_scene && m_statsPanel) {
    m_statsPanel->SetScene(m_scene);
  }
  m_scenePath = GetDefaultScenePath();
  UpdateWindowTitle();

  if (m_auxPanel) {
    const QString sceneName =
        m_scene ? QString::fromStdString(m_scene->GetName()) : tr("--");
    const QString scenePath = QString::fromStdString(m_scenePath.string());
    m_auxPanel->SetSceneText(tr("%1 (%2)").arg(sceneName, scenePath));
    m_auxPanel->SetEntityText(tr("None"));
    m_auxPanel->SetAssetText(tr("None"));
    if (m_viewport) {
      m_auxPanel->SetViewportText(
          tr("%1×%2").arg(m_surfaceSize.width()).arg(m_surfaceSize.height()));
      m_auxPanel->SetCameraText(
          tr("pos(%1, %2, %3) rot(%4, %5) zoom %6")
              .arg(QString::number(m_viewport->getCameraX(), 'f', 2))
              .arg(QString::number(m_viewport->getCameraY(), 'f', 2))
              .arg(QString::number(m_viewport->getCameraZ(), 'f', 2))
              .arg(QString::number(m_viewport->getCameraRotationY(), 'f', 1))
              .arg(QString::number(m_viewport->getCameraRotationX(), 'f', 1))
              .arg(QString::number(m_viewport->getCameraZoom(), 'f', 2)));
    }
  }
  if (m_inspectorPanel) {
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    m_inspectorPanel->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
  }
  if (m_cameraPreview) {
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    m_cameraPreview->SetScene(m_scene);
    m_cameraPreview->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
    m_cameraPreview->SetRenderViewSource(ctx ? ctx->GetRenderView() : nullptr);
  }

  AttachVulkanLogSink();
  RefreshAssetBrowser();
  if (auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr) {
    if (auto registry = ctx->GetAssetRegistry()) {
      m_assetChangeSerial = registry->GetChangeSerial();
    }
  }

  m_assetFileWatcher = new QFileSystemWatcher(this);
  connect(m_assetFileWatcher, &QFileSystemWatcher::directoryChanged, this,
          [this](const QString &) { m_assetWatcherDirty = true; });
  connect(m_assetFileWatcher, &QFileSystemWatcher::fileChanged, this,
          [this](const QString &) { m_assetWatcherDirty = true; });
  RefreshAssetWatchPaths();
  m_assetWatcherDirty = false;

  m_assetWatchTimer = new QTimer(this);
  m_assetWatchTimer->setInterval(500);
  connect(m_assetWatchTimer, &QTimer::timeout, this,
          &EditorMainWindow::PollAssetChanges);
  m_assetWatchTimer->start();

  connect(m_selection, &EditorSelection::SelectionChanged, this,
          [this](Aetherion::Core::EntityId) {
            AppendConsole(m_console, tr("Selection: entity changed"),
                          ConsoleSeverity::Info);
            if (m_assetBrowser) {
              m_assetBrowser->ClearSelection();
            }
            if (m_inspectorPanel) {
              m_inspectorPanel->SetSelectedEntity(
                  m_selection->GetSelectedEntity());
            }

            if (m_auxPanel) {
              const auto entity =
                  m_selection ? m_selection->GetSelectedEntity() : nullptr;
              if (entity) {
                m_auxPanel->SetEntityText(
                    tr("%1 (id %2)")
                        .arg(QString::fromStdString(entity->GetName()))
                        .arg(QString::number(
                            static_cast<qulonglong>(entity->GetId()))));
              } else {
                m_auxPanel->SetEntityText(tr("None"));
              }
              // Keep asset line as-is.
            }

            if (m_cameraPreview) {
              auto entity =
                  m_selection ? m_selection->GetSelectedEntity() : nullptr;
              if (entity && entity->GetComponent<Scene::CameraComponent>()) {
                m_cameraPreview->SetSelectedCameraId(entity->GetId());
              } else {
                m_cameraPreview->SetSelectedCameraId(0);
              }
            }
            UpdateAiHudFromSelection();
          });
  connect(m_selection, &EditorSelection::SelectionCleared, this, [this]() {
    AppendConsole(m_console, tr("Selection: entity cleared"),
                  ConsoleSeverity::Info);
    if (m_inspectorPanel) {
      m_inspectorPanel->SetSelectedEntity(nullptr);
    }

    if (m_auxPanel) {
      m_auxPanel->SetEntityText(tr("None"));
    }

    if (m_cameraPreview) {
      m_cameraPreview->SetSelectedCameraId(0);
    }
    UpdateAiHudFromSelection();
  });

  if (m_hierarchyPanel) {
    connect(m_hierarchyPanel, &EditorHierarchyPanel::entityActivated, this,
            [this](Aetherion::Core::EntityId) {
              // Re-open the Properties/Inspector panel if the user closed it.
              FocusSideTab(m_panelManager, m_inspectorPanel, true);
              // QoL: Focus camera on double-click
              FocusCameraOnSelection();
            });

    connect(
        m_hierarchyPanel, &EditorHierarchyPanel::entityReparentRequested, this,
        [this](Aetherion::Core::EntityId childId,
               Aetherion::Core::EntityId newParentId) {
          if (!m_scene) {
            return;
          }

          const bool success = m_scene->SetParent(childId, newParentId);
          if (!success) {
            if (m_console) {
              m_console->AppendMessage(tr("Reparent blocked (invalid target)"),
                                       ConsoleSeverity::Warning);
            }
            if (m_hierarchyPanel) {
              m_hierarchyPanel->BindScene(m_scene);
            }
            return;
          }

          SetSceneDirty(true);

          if (m_console) {
            const QString msg = (newParentId == 0)
                                    ? tr("Entity %1 unparented").arg(childId)
                                    : tr("Entity %1 parented to %2")
                                          .arg(childId)
                                          .arg(newParentId);
            m_console->AppendMessage(msg, ConsoleSeverity::Info);
          }
          if (m_hierarchyPanel) {
            m_hierarchyPanel->BindScene(m_scene);
            m_hierarchyPanel->SetSelectedEntity(childId);
          }
        });

    // Connect hierarchy context menu actions
    connect(m_hierarchyPanel, &EditorHierarchyPanel::entityDeleteRequested,
            this, &EditorMainWindow::DeleteEntity);
    connect(m_hierarchyPanel, &EditorHierarchyPanel::entityDuplicateRequested,
            this, &EditorMainWindow::DuplicateEntity);
    connect(m_hierarchyPanel, &EditorHierarchyPanel::entityRenameRequested,
            this, &EditorMainWindow::RenameEntity);
    connect(m_hierarchyPanel, &EditorHierarchyPanel::createEmptyEntityRequested,
            this, &EditorMainWindow::CreateEmptyEntity);
    connect(m_hierarchyPanel,
            &EditorHierarchyPanel::createEmptyEntityAtRootRequested, this,
            [this]() { CreateEmptyEntity(0); });
    connect(m_hierarchyPanel, &EditorHierarchyPanel::createLightEntityRequested,
            this, [this](Aetherion::Core::EntityId parentId) {
              CreateLightEntity(parentId);
            });
    connect(m_hierarchyPanel,
            &EditorHierarchyPanel::createCameraEntityRequested, this,
            [this](Aetherion::Core::EntityId parentId) {
              CreateCameraEntity(parentId);
            });
    connect(m_hierarchyPanel, &EditorHierarchyPanel::createMeshEntityRequested,
            this,
            [this](Aetherion::Core::EntityId parentId,
                   const QString &meshAssetId, const QString &displayName) {
              CreateMeshEntity(parentId, meshAssetId, displayName);
            });
  }

  if (m_inspectorPanel) {
    connect(m_inspectorPanel, &EditorInspectorPanel::sceneModified, this,
            [this] { SetSceneDirty(true); });
  }

  if (m_assetBrowser) {
    connect(m_assetBrowser, &EditorAssetBrowser::AssetSelected, this,
            [this](const QString &assetId) {
              AppendConsole(m_console,
                            tr("AssetBrowser: selected '%1'").arg(assetId),
                            ConsoleSeverity::Info);

              m_selectedAssetId = assetId;
              if (m_auxPanel) {
                m_auxPanel->SetAssetText(assetId);
              }
              if (m_inspectorPanel) {
                m_inspectorPanel->SetSelectedAsset(assetId);
                AppendConsole(m_console,
                              tr("Inspector: showing asset '%1'").arg(assetId),
                              ConsoleSeverity::Info);
              }
              // Update mesh preview if it's a mesh asset
              if (m_meshPreview) {
                auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
                auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
                if (registry) {
                  const auto *entry =
                      registry->FindEntry(assetId.toStdString());
                  if (entry &&
                      entry->type == Assets::AssetRegistry::AssetType::Mesh) {
                    m_meshPreview->SetMeshAsset(assetId);
                  } else {
                    m_meshPreview->ClearPreview();
                  }
                }
              }
            });
    connect(m_assetBrowser, &EditorAssetBrowser::AssetSelectionCleared, this,
            [this] {
              AppendConsole(m_console, tr("AssetBrowser: selection cleared"),
                            ConsoleSeverity::Info);

              m_selectedAssetId.clear();
              if (m_auxPanel) {
                m_auxPanel->SetAssetText(tr("None"));
              }
              // Keep current entity selection (if any) as source of truth.
              if (m_inspectorPanel) {
                m_inspectorPanel->SetSelectedEntity(
                    m_selection ? m_selection->GetSelectedEntity() : nullptr);
                AppendConsole(m_console,
                              tr("Inspector: reverted to entity selection"),
                              ConsoleSeverity::Info);
              }
              if (m_meshPreview) {
                m_meshPreview->ClearPreview();
              }
            });
    connect(m_assetBrowser, &EditorAssetBrowser::AssetActivated, this, [this] {
      FocusSideTab(m_panelManager, m_inspectorPanel, true);
    });
    connect(m_assetBrowser, &EditorAssetBrowser::RescanRequested, this,
            &EditorMainWindow::RescanAssets);
    connect(m_assetBrowser, &EditorAssetBrowser::AssetDroppedOnScene, this,
            &EditorMainWindow::AddAssetToScene);
    connect(m_assetBrowser, &EditorAssetBrowser::AssetDeleteRequested, this,
            &EditorMainWindow::DeleteAsset);
    connect(m_assetBrowser, &EditorAssetBrowser::AssetRenameRequested, this,
            &EditorMainWindow::RenameAsset);
    connect(m_assetBrowser, &EditorAssetBrowser::AssetShowInExplorerRequested,
            this, &EditorMainWindow::ShowAssetInExplorer);
  }
}

EditorMainWindow::~EditorMainWindow() {
  DestroyViewportRenderer();
  DetachVulkanLogSink();

  if (m_runtimeApp) {
    m_runtimeApp->Shutdown();
  }
}

void EditorMainWindow::InitializeCommandPalette() {
  if (!m_commandPalette) {
    m_commandPalette = new EditorCommandPalette(this);
  }
}

void EditorMainWindow::RegisterCommandAction(QAction *action,
                                             const QString &category,
                                             const QString &description) {
  if (!m_commandPalette || !action) {
    return;
  }
  m_commandPalette->RegisterAction(action, category, description);
}

void EditorMainWindow::OpenCommandPalette() {
  if (m_commandPalette) {
    m_commandPalette->ShowPalette();
  }
}

void EditorMainWindow::CreateMenuBarContent() {
  auto *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(tr("New Project"), [] {});
  fileMenu->addAction(tr("Open Project..."), [] {});
  auto *importGltf = fileMenu->addAction(tr("Import glTF..."));
  connect(importGltf, &QAction::triggered, this,
          &EditorMainWindow::ImportGltfAsset);
  RegisterCommandAction(importGltf, tr("File"), tr("Import a glTF asset"));
  auto *openScene = fileMenu->addAction(tr("Open Scene..."));
  openScene->setShortcut(QKeySequence::Open);
  connect(openScene, &QAction::triggered, this, &EditorMainWindow::OpenScene);
  RegisterCommandAction(openScene, tr("File"), tr("Open a scene file"));
  auto *saveScene = fileMenu->addAction(tr("Save Scene"));
  saveScene->setShortcut(QKeySequence::Save);
  connect(saveScene, &QAction::triggered, this, &EditorMainWindow::SaveScene);
  RegisterCommandAction(saveScene, tr("File"), tr("Save the active scene"));
  auto *reloadScene = fileMenu->addAction(tr("Reload Scene"));
  reloadScene->setShortcut(QKeySequence::Refresh);
  connect(reloadScene, &QAction::triggered, this,
          &EditorMainWindow::ReloadScene);
  RegisterCommandAction(reloadScene, tr("File"), tr("Reload the active scene"));
  auto *rescanAssets = fileMenu->addAction(tr("Rescan Assets"));
  rescanAssets->setShortcut(QKeySequence(tr("Ctrl+Shift+R")));
  connect(rescanAssets, &QAction::triggered, this,
          &EditorMainWindow::RescanAssets);
  RegisterCommandAction(rescanAssets, tr("File"),
                        tr("Rescan the asset registry"));
  fileMenu->addSeparator();
  auto *exitAction = fileMenu->addAction(tr("Exit"), this, &QWidget::close);
  RegisterCommandAction(exitAction, tr("File"), tr("Exit the editor"));

  // QoL shortcuts that don't warrant extra UI.
  m_focusAssetFilterAction = new QAction(tr("Focus Asset Filter"), this);
  m_focusAssetFilterAction->setShortcut(QKeySequence::Find);
  addAction(m_focusAssetFilterAction);
  connect(m_focusAssetFilterAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_assetBrowser, false);
    if (m_assetBrowser) {
      m_assetBrowser->FocusFilter();
    }
  });
  RegisterCommandAction(m_focusAssetFilterAction, tr("Navigation"),
                        tr("Focus the asset browser filter"));

  auto *editMenu = menuBar()->addMenu(tr("&Edit"));
  m_undoAction = editMenu->addAction(tr("Undo"));
  m_undoAction->setShortcut(QKeySequence::Undo);
  connect(m_undoAction, &QAction::triggered, this, &EditorMainWindow::Undo);

  m_redoAction = editMenu->addAction(tr("Redo"));
  m_redoAction->setShortcut(QKeySequence::Redo);
  connect(m_redoAction, &QAction::triggered, this, &EditorMainWindow::Redo);

  RegisterCommandAction(m_undoAction, tr("Edit"), tr("Undo the last action"));
  RegisterCommandAction(m_redoAction, tr("Edit"), tr("Redo the last action"));
  UpdateUndoRedoState();
  editMenu->addSeparator();

  m_commandPaletteAction = editMenu->addAction(tr("Command Palette"));
  m_commandPaletteAction->setShortcut(QKeySequence(tr("Ctrl+Shift+P")));
  connect(m_commandPaletteAction, &QAction::triggered, this,
          &EditorMainWindow::OpenCommandPalette);
  RegisterCommandAction(m_commandPaletteAction, tr("Edit"),
                        tr("Search and run editor commands"));
  editMenu->addSeparator();

  auto *preferences = editMenu->addAction(tr("Preferences"));
  connect(preferences, &QAction::triggered, this,
          &EditorMainWindow::OpenSettingsDialog);
  RegisterCommandAction(preferences, tr("Edit"),
                        tr("Open editor preferences"));
  m_validationMenuAction =
      editMenu->addAction(tr("Enable Vulkan Validation Layers"));
  m_validationMenuAction->setCheckable(true);
  m_validationMenuAction->setChecked(m_validationEnabled);
  connect(m_validationMenuAction, &QAction::toggled, this,
          [this](bool enabled) {
            if (enabled == m_validationEnabled) {
              return;
            }
            m_settings.validationEnabled = enabled;
            ApplySettings(m_settings, true);
          });
  RegisterCommandAction(m_validationMenuAction, tr("Edit"),
                        tr("Toggle Vulkan validation layers"));

  m_loggingMenuAction = editMenu->addAction(tr("Verbose Rendering Logs"));
  m_loggingMenuAction->setCheckable(true);
  m_loggingMenuAction->setChecked(m_renderLoggingEnabled);
  connect(m_loggingMenuAction, &QAction::toggled, this, [this](bool enabled) {
    if (enabled == m_renderLoggingEnabled) {
      return;
    }
    m_settings.verboseLogging = enabled;
    ApplySettings(m_settings, true);
  });
  RegisterCommandAction(m_loggingMenuAction, tr("Edit"),
                        tr("Toggle verbose rendering logs"));

  auto *viewMenu = menuBar()->addMenu(tr("&View"));

  m_showHierarchyAction = viewMenu->addAction(tr("Focus Hierarchy"));
  m_showHierarchyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
  connect(m_showHierarchyAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_hierarchyPanel, false);
  });
  RegisterCommandAction(m_showHierarchyAction, tr("View"),
                        tr("Focus the hierarchy tab"));

  m_showAssetBrowserAction = viewMenu->addAction(tr("Focus Asset Browser"));
  m_showAssetBrowserAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
  connect(m_showAssetBrowserAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_assetBrowser, false);
  });
  RegisterCommandAction(m_showAssetBrowserAction, tr("View"),
                        tr("Focus the asset browser tab"));

  m_showInspectorAction = viewMenu->addAction(tr("Focus Inspector"));
  m_showInspectorAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
  connect(m_showInspectorAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_inspectorPanel, true);
  });
  RegisterCommandAction(m_showInspectorAction, tr("View"),
                        tr("Focus the inspector tab"));

  m_showConsoleAction = viewMenu->addAction(tr("Focus Console"));
  m_showConsoleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_4));
  connect(m_showConsoleAction, &QAction::triggered, this, [this] {
    FocusBottomTab(m_panelManager, m_console);
  });
  RegisterCommandAction(m_showConsoleAction, tr("View"),
                        tr("Focus the console tab"));

  m_showMeshPreviewAction = viewMenu->addAction(tr("Focus Mesh Preview"));
  m_showMeshPreviewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_5));
  connect(m_showMeshPreviewAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_meshPreview, true);
  });
  RegisterCommandAction(m_showMeshPreviewAction, tr("View"),
                        tr("Focus the mesh preview tab"));

  m_showCameraPreviewAction = viewMenu->addAction(tr("Focus Camera Preview"));
  m_showCameraPreviewAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_6));
  connect(m_showCameraPreviewAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_cameraPreview, true);
  });
  RegisterCommandAction(m_showCameraPreviewAction, tr("View"),
                        tr("Focus the camera preview tab"));

  m_showAICopilotAction = viewMenu->addAction(tr("Focus AI Copilot"));
  m_showAICopilotAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_7));
  connect(m_showAICopilotAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_copilotPanel, true);
  });
  RegisterCommandAction(m_showAICopilotAction, tr("View"),
                        tr("Focus the AI copilot tab"));

  m_showAssetGenAction = viewMenu->addAction(tr("Focus Asset Generation"));
  m_showAssetGenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_8));
  connect(m_showAssetGenAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_assetGenPanel, true);
  });
  RegisterCommandAction(m_showAssetGenAction, tr("View"),
                        tr("Focus the asset generation tab"));

  m_showStatsAction = viewMenu->addAction(tr("Focus Statistics"));
  m_showStatsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_9));
  connect(m_showStatsAction, &QAction::triggered, this, [this] {
    FocusBottomTab(m_panelManager, m_statsPanel);
  });
  RegisterCommandAction(m_showStatsAction, tr("View"),
                        tr("Focus the statistics tab"));

  m_showBookmarksAction = viewMenu->addAction(tr("Focus Bookmarks"));
  m_showBookmarksAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  connect(m_showBookmarksAction, &QAction::triggered, this, [this] {
    FocusSideTab(m_panelManager, m_bookmarkList ? m_bookmarkList->parentWidget()
                                                : nullptr,
                 true);
  });
  RegisterCommandAction(m_showBookmarksAction, tr("View"),
                        tr("Focus the bookmarks tab"));

  m_showBottomPanelAction = viewMenu->addAction(tr("Show Bottom Panel"));
  m_showBottomPanelAction->setCheckable(true);
  m_showBottomPanelAction->setChecked(true);
  connect(m_showBottomPanelAction, &QAction::toggled, this,
          [this](bool visible) {
            if (m_panelManager) {
              m_panelManager->SetBottomPanelVisible(visible);
            }
          });
  RegisterCommandAction(m_showBottomPanelAction, tr("View"),
                        tr("Toggle bottom panel visibility"));

  viewMenu->addSeparator();
  auto *saveLayoutAction = viewMenu->addAction(tr("Save Layout As..."), [this] {
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Save Layout"), tr("Layout name:"), QLineEdit::Normal, QString(),
        &ok);
    if (!ok) {
      return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
      return;
    }
    if (trimmed.contains('/') || trimmed.contains('\\')) {
      QMessageBox::warning(this, tr("Save Layout"),
                           tr("Layout names cannot include slashes."));
      return;
    }

    QSettings settings("Aetherion", "Editor");
    QStringList presets =
        settings.value("layout/presetNames").toStringList();
    if (presets.contains(trimmed)) {
      const auto reply = QMessageBox::question(
          this, tr("Overwrite Layout"),
          tr("A layout named '%1' already exists. Overwrite?")
              .arg(trimmed),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (reply != QMessageBox::Yes) {
        return;
      }
    } else {
      presets.push_back(trimmed);
      presets.sort(Qt::CaseInsensitive);
    }

    settings.setValue(QString("layout/presets/%1/state").arg(trimmed),
                      saveState());
    settings.setValue(QString("layout/presets/%1/geometry").arg(trimmed),
                      saveGeometry());
    if (m_panelManager) {
      m_panelManager->SavePanelState(
          settings, QString("layout/presets/%1/panel").arg(trimmed));
    }
    settings.setValue("layout/presetNames", presets);
    settings.sync();
    statusBar()->showMessage(tr("Layout preset saved"), 2000);
  });
  RegisterCommandAction(saveLayoutAction, tr("View"),
                        tr("Save the current layout preset"));
  auto *loadLayoutAction = viewMenu->addAction(tr("Load Layout..."), [this] {
    QSettings settings("Aetherion", "Editor");
    QStringList presets =
        settings.value("layout/presetNames").toStringList();
    if (presets.isEmpty()) {
      QMessageBox::information(this, tr("Load Layout"),
                               tr("No saved layouts found."));
      return;
    }
    presets.sort(Qt::CaseInsensitive);

    bool ok = false;
    const QString choice =
        QInputDialog::getItem(this, tr("Load Layout"),
                              tr("Select a layout preset:"), presets, 0, false,
                              &ok);
    if (!ok || choice.isEmpty()) {
      return;
    }

    const QByteArray geometry =
        settings.value(QString("layout/presets/%1/geometry").arg(choice))
            .toByteArray();
    const QByteArray state =
        settings.value(QString("layout/presets/%1/state").arg(choice))
            .toByteArray();
    if (!geometry.isEmpty()) {
      restoreGeometry(geometry);
    }
    if (!state.isEmpty()) {
      restoreState(state);
    }
    if (m_panelManager) {
      m_panelManager->RestorePanelState(
          settings, QString("layout/presets/%1/panel").arg(choice));
      if (m_showBottomPanelAction) {
        m_showBottomPanelAction->blockSignals(true);
        m_showBottomPanelAction->setChecked(
            m_panelManager->IsBottomPanelVisible());
        m_showBottomPanelAction->blockSignals(false);
      }
    }
    SaveLayout();
    statusBar()->showMessage(tr("Layout preset loaded"), 2000);
  });
  RegisterCommandAction(loadLayoutAction, tr("View"),
                        tr("Load a saved layout preset"));
  auto *resetLayoutAction = viewMenu->addAction(tr("Reset Layout"), [this] {
    if (!m_defaultLayoutGeometry.isEmpty()) {
      restoreGeometry(m_defaultLayoutGeometry);
      m_defaultLayoutGeometry = saveGeometry();
    }
    restoreState(m_defaultLayoutState);
    m_defaultLayoutState = saveState();
    if (m_panelManager) {
      QSettings defaultSettings("Aetherion", "Editor");
      m_panelManager->RestorePanelState(defaultSettings,
                                        "layout/defaultPanel");
      if (m_showBottomPanelAction) {
        m_showBottomPanelAction->blockSignals(true);
        m_showBottomPanelAction->setChecked(
            m_panelManager->IsBottomPanelVisible());
        m_showBottomPanelAction->blockSignals(false);
      }
    }
    SaveLayout();
    statusBar()->showMessage(tr("Layout reset"), 2000);
  });
  RegisterCommandAction(resetLayoutAction, tr("View"),
                        tr("Reset the layout to defaults"));
  viewMenu->addSeparator();

  auto *fullscreenAction = viewMenu->addAction(tr("Toggle Fullscreen"));        
  fullscreenAction->setCheckable(true);
  fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
  fullscreenAction->setChecked(isFullScreen());
  connect(fullscreenAction, &QAction::triggered, this,
          [this, fullscreenAction](bool checked) {
            if (checked) {
              showFullScreen();
            } else {
              showNormal();
            }
            fullscreenAction->setChecked(isFullScreen());
          });
  RegisterCommandAction(fullscreenAction, tr("View"),
                        tr("Toggle fullscreen mode"));

  auto *resetCameraAction = viewMenu->addAction(tr("Reset Camera"));
  resetCameraAction->setShortcut(QKeySequence(Qt::Key_Home));
  connect(resetCameraAction, &QAction::triggered, this, [this] {
    if (m_viewport) {
      m_viewport->resetCamera();
    }
    if (m_vulkanViewport) {
      m_vulkanViewport->ResetCamera();
    }
    statusBar()->showMessage(tr("Camera reset"), 2000);
  });
  RegisterCommandAction(resetCameraAction, tr("View"),
                        tr("Reset the editor camera"));

  auto *focusOnSelectionAction = viewMenu->addAction(tr("Focus on Selection"));
  focusOnSelectionAction->setShortcut(QKeySequence(Qt::Key_F));
  connect(focusOnSelectionAction, &QAction::triggered, this,
          [this] { FocusCameraOnSelection(); });
  RegisterCommandAction(focusOnSelectionAction, tr("View"),
                        tr("Frame the selected entity"));

  m_showAiHudAction = viewMenu->addAction(tr("Show AI HUD"));
  m_showAiHudAction->setCheckable(true);
  m_showAiHudAction->setChecked(m_aiHudVisible);
  connect(m_showAiHudAction, &QAction::toggled, this, [this](bool visible) {
    m_aiHudVisible = visible;
    UpdateAiHudFromSelection();
  });
  RegisterCommandAction(m_showAiHudAction, tr("View"),
                        tr("Toggle the AI HUD overlay"));

  auto *helpMenu = menuBar()->addMenu(tr("&Help"));
  auto *aboutAction = helpMenu->addAction(tr("About Aetherion"));
  connect(aboutAction, &QAction::triggered, this, [this] {
    const QString version = QCoreApplication::applicationVersion().isEmpty()
                                ? tr("dev")
                                : QCoreApplication::applicationVersion();
    const QString text =
        tr("Aetherion Editor using Aetherion-Engine.\n© Max Staneker "
           "2026\nVersion: %1\nUI: Qt 6\nRenderer: Vulkan\n\nBuild: %2 %3")
            .arg(version)
            .arg(QString::fromLatin1(__DATE__))
            .arg(QString::fromLatin1(__TIME__)) +
        tr("\nRunning on: %1").arg(QSysInfo::prettyProductName());
    QMessageBox::about(this, tr("About Aetherion"), text);
  });
  RegisterCommandAction(aboutAction, tr("Help"),
                        tr("Show information about Aetherion"));
}

void EditorMainWindow::CreateToolBarContent() {
  auto *toolBar = addToolBar(tr("Main"));
  toolBar->setObjectName("MainToolBar");
  toolBar->setMovable(false);
  toolBar->setAllowedAreas(Qt::TopToolBarArea);
  addToolBar(Qt::TopToolBarArea, toolBar);

  m_playAction = toolBar->addAction(tr("Play"));
  m_pauseAction = toolBar->addAction(tr("Pause"));
  m_stepAction = toolBar->addAction(tr("Step"));
  m_resetAction = toolBar->addAction(tr("Reset"));

  m_playAction->setCheckable(true);
  m_pauseAction->setCheckable(true);

  connect(m_playAction, &QAction::triggered, this,
          [this] { StartOrStopPlaySession(); });
  connect(m_pauseAction, &QAction::triggered, this,
          [this] { TogglePauseSession(); });
  connect(m_stepAction, &QAction::triggered, this,
          [this] { StepSimulationOnce(); });
  connect(m_resetAction, &QAction::triggered, this,
          [this] { ResetPlaySession(); });

  RegisterCommandAction(m_playAction, tr("Playback"),
                        tr("Start or stop play mode"));
  RegisterCommandAction(m_pauseAction, tr("Playback"), tr("Pause playback"));
  RegisterCommandAction(m_stepAction, tr("Playback"),
                        tr("Step the simulation once"));
  RegisterCommandAction(m_resetAction, tr("Playback"),
                        tr("Reset the play session"));

  toolBar->addSeparator();

  m_modeActionGroup = new QActionGroup(toolBar);
  m_modeActionGroup->setExclusive(true);

  m_modeEditAction = toolBar->addAction(tr("Edit"));
  m_modePlaytestAction = toolBar->addAction(tr("Playtest"));
  m_modeUILayoutAction = toolBar->addAction(tr("UI Layout"));

  m_modeEditAction->setCheckable(true);
  m_modePlaytestAction->setCheckable(true);
  m_modeUILayoutAction->setCheckable(true);

  m_modeEditAction->setActionGroup(m_modeActionGroup);
  m_modePlaytestAction->setActionGroup(m_modeActionGroup);
  m_modeUILayoutAction->setActionGroup(m_modeActionGroup);

  connect(m_modeEditAction, &QAction::triggered, this,
          [this] { ActivateModeTab(0); });
  connect(m_modePlaytestAction, &QAction::triggered, this,
          [this] { ActivateModeTab(1); });
  connect(m_modeUILayoutAction, &QAction::triggered, this,
          [this] { ActivateModeTab(2); });

  RegisterCommandAction(m_modeEditAction, tr("Mode"),
                        tr("Switch to edit mode"));
  RegisterCommandAction(m_modePlaytestAction, tr("Mode"),
                        tr("Switch to playtest mode"));
  RegisterCommandAction(m_modeUILayoutAction, tr("Mode"),
                        tr("Switch to UI layout mode"));

  toolBar->addSeparator();

  m_gizmoActionGroup = new QActionGroup(toolBar);
  m_gizmoActionGroup->setExclusive(true);

  m_gizmoTranslateAction = toolBar->addAction(tr("Move"));
  m_gizmoRotateAction = toolBar->addAction(tr("Rotate"));
  m_gizmoScaleAction = toolBar->addAction(tr("Scale"));

  m_gizmoTranslateAction->setShortcut(QKeySequence(Qt::Key_W));
  m_gizmoRotateAction->setShortcut(QKeySequence(Qt::Key_E));
  m_gizmoScaleAction->setShortcut(QKeySequence(Qt::Key_R));

  m_gizmoTranslateAction->setCheckable(true);
  m_gizmoRotateAction->setCheckable(true);
  m_gizmoScaleAction->setCheckable(true);

  m_gizmoTranslateAction->setActionGroup(m_gizmoActionGroup);
  m_gizmoRotateAction->setActionGroup(m_gizmoActionGroup);
  m_gizmoScaleAction->setActionGroup(m_gizmoActionGroup);

  connect(m_gizmoTranslateAction, &QAction::triggered, this,
          [this] { m_gizmoMode = GizmoMode::Translate; });
  connect(m_gizmoRotateAction, &QAction::triggered, this,
          [this] { m_gizmoMode = GizmoMode::Rotate; });
  connect(m_gizmoScaleAction, &QAction::triggered, this,
          [this] { m_gizmoMode = GizmoMode::Scale; });

  m_snapToggleAction = toolBar->addAction(tr("Snap"));
  m_snapToggleAction->setCheckable(true);
  m_snapToggleAction->setChecked(true);
  m_snapToggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));

  RegisterCommandAction(m_gizmoTranslateAction, tr("Gizmo"),
                        tr("Activate translate gizmo"));
  RegisterCommandAction(m_gizmoRotateAction, tr("Gizmo"),
                        tr("Activate rotate gizmo"));
  RegisterCommandAction(m_gizmoScaleAction, tr("Gizmo"),
                        tr("Activate scale gizmo"));
  RegisterCommandAction(m_snapToggleAction, tr("Gizmo"),
                        tr("Toggle snap for transforms"));

  ActivateModeTab(0);
  m_gizmoTranslateAction->setChecked(true);
  UpdateRuntimeControlStates();
}

void EditorMainWindow::ApplySettings(const EditorSettings &settings,
                                     bool persist) {
  const bool validationChanged =
      settings.validationEnabled != m_validationEnabled;
  const bool loggingChanged = settings.verboseLogging != m_renderLoggingEnabled;

  m_settings = settings;
  m_settings.Clamp();
  m_validationEnabled = m_settings.validationEnabled;
  m_renderLoggingEnabled = m_settings.verboseLogging;
  m_targetFrameIntervalMs =
      std::max(1, 1000 / std::max(1, m_settings.targetFps));
  m_headlessSleepMs = m_settings.headlessSleepMs;

  if (persist) {
    m_settings.Save();
  }

  if (m_validationMenuAction) {
    m_validationMenuAction->blockSignals(true);
    m_validationMenuAction->setChecked(m_validationEnabled);
    m_validationMenuAction->blockSignals(false);
  }
  if (m_loggingMenuAction) {
    m_loggingMenuAction->blockSignals(true);
    m_loggingMenuAction->setChecked(m_renderLoggingEnabled);
    m_loggingMenuAction->blockSignals(false);
  }

  if (auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr) {
    if (auto vk = ctx->GetVulkanContext()) {
      vk->SetLoggingEnabled(m_renderLoggingEnabled);
    }
  }

  if (validationChanged) {
    RecreateRuntimeAndRenderer(m_validationEnabled);
  } else if (m_vulkanViewport) {
    m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
  }

  // Update LLM generator configuration for AI-powered asset generation
  if (m_assetGenPanel) {
    m_assetGenPanel->ConfigureLLMGenerator(m_settings.llm);
  }

  ApplyRuntimeAISettings();

  UpdateRenderTimerInterval(m_vulkanViewport && m_vulkanViewport->IsReady());   
}

void EditorMainWindow::ApplyRuntimeAISettings() {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  if (!ctx) {
    return;
  }

  Assets::LLMConfig config;
  if (BuildAIConfig(m_settings.llm, config)) {
    ctx->SetAIConfig(std::move(config), true);
  } else {
    ctx->ClearAIConfig();
  }
}

void EditorMainWindow::UpdateRenderTimerInterval(bool viewportReady) {
  if (!m_renderTimer) {
    return;
  }

  const bool headless =
      !viewportReady || !m_surfaceInitialized || isMinimized();
  const int desiredInterval = (headless && m_headlessSleepMs > 0)
                                  ? m_headlessSleepMs
                                  : m_targetFrameIntervalMs;
  if (desiredInterval > 0 && m_renderTimer->interval() != desiredInterval) {
    m_renderTimer->setInterval(desiredInterval);
  }
}

void EditorMainWindow::OpenSettingsDialog() {
  EditorSettingsDialog dialog(m_settings, this);
  if (dialog.exec() == QDialog::Accepted) {
    ApplySettings(dialog.GetSettings(), true);
  }
}

void EditorMainWindow::RefreshAssetBrowser() {
  if (!m_assetBrowser) {
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;

  std::vector<EditorAssetBrowser::Item> items;
  if (!registry) {
    const QString label = tr("Assets unavailable");
    items.push_back({label, label, true, QString(), QString()});
    m_assetBrowser->SetItems(items);
    return;
  }

  const auto &entries = registry->GetEntries();
  struct Category {
    Assets::AssetRegistry::AssetType type;
    const char *label;
  };

  const Category categories[] = {
      {Assets::AssetRegistry::AssetType::Texture, "Textures/"},
      {Assets::AssetRegistry::AssetType::Mesh, "Meshes/"},
      {Assets::AssetRegistry::AssetType::Audio, "Audio/"},
      {Assets::AssetRegistry::AssetType::Script, "Scripts/"},
      {Assets::AssetRegistry::AssetType::Scene, "Scenes/"},
      {Assets::AssetRegistry::AssetType::Shader, "Shaders/"},
      {Assets::AssetRegistry::AssetType::Other, "Misc/"}};

  const size_t categoryCount = sizeof(categories) / sizeof(categories[0]);
  std::vector<std::vector<const Assets::AssetRegistry::AssetEntry *>> grouped(
      categoryCount);
  for (const auto &entry : entries) {
    for (size_t i = 0; i < categoryCount; ++i) {
      if (entry.type == categories[i].type) {
        grouped[i].push_back(&entry);
        break;
      }
    }
  }

  auto iconForType = [](Assets::AssetRegistry::AssetType type) -> QString {
    switch (type) {
    case Assets::AssetRegistry::AssetType::Mesh:
      return QStringLiteral(":/aetherion/icons/mesh.svg");
    default:
      return QStringLiteral(":/aetherion/icons/file.svg");
    }
  };

  for (size_t i = 0; i < categoryCount; ++i) {
    const QString header = tr(categories[i].label);
    items.push_back({header, header, true, QString(), QString()});

    auto &list = grouped[i];
    std::sort(list.begin(), list.end(), [](const auto *lhs, const auto *rhs) {
      return lhs->id < rhs->id;
    });

    for (const auto *entry : list) {
      std::filesystem::path displayPath = entry->path;
      std::error_code ec;
      if (!registry->GetRootPath().empty()) {
        auto relative =
            std::filesystem::relative(entry->path, registry->GetRootPath(), ec);
        if (!ec && !relative.empty()) {
          displayPath = relative;
        }
      }
      QString label =
          QString::fromStdString(displayPath.generic_string());
      if (registry->IsVirtualAsset(entry->id)) {
        label = tr("[Virtual] %1").arg(label);
      }
      const QString id = QString::fromStdString(entry->id);
      const QString assetPath = QString::fromStdString(entry->path.string());   
      items.push_back({QString("  %1").arg(label), id, false,
                       iconForType(entry->type), assetPath, label});
    }
  }

  m_assetBrowser->SetItems(items);
}

void EditorMainWindow::RescanAssets() {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  std::filesystem::path root = registry->GetRootPath();
  if (root.empty()) {
    root = std::filesystem::path("assets");
  }
  registry->Scan(root.string());
  RefreshAssetBrowser();
  RefreshAssetWatchPaths();
  m_assetChangeSerial = registry->GetChangeSerial();
  m_assetWatcherDirty = false;
  if (m_inspectorPanel) {
    m_inspectorPanel->SetAssetRegistry(registry);
  }
  statusBar()->showMessage(tr("Assets rescanned"), 2000);
}

void EditorMainWindow::RefreshAssetWatchPaths() {
  if (!m_assetFileWatcher) {
    return;
  }

  std::filesystem::path root = GetAssetsRootPath();
  if (root.empty()) {
    return;
  }

  const QDir rootDir(QString::fromStdString(root.string()));
  if (!rootDir.exists()) {
    const auto existing = m_assetFileWatcher->directories();
    if (!existing.isEmpty()) {
      m_assetFileWatcher->removePaths(existing);
    }
    return;
  }

  const QString rootPath = QDir::cleanPath(rootDir.absolutePath());
  QSet<QString> desired;
  desired.insert(rootPath);

  QDirIterator it(rootPath, QDir::Dirs | QDir::NoDotAndDotDot,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    desired.insert(QDir::cleanPath(it.next()));
  }

  const QStringList currentList = m_assetFileWatcher->directories();
  QSet<QString> current;
  for (const auto &dir : currentList) {
    current.insert(QDir::cleanPath(dir));
  }

  QStringList toAdd;
  QStringList toRemove;
  for (const auto &dir : desired) {
    if (!current.contains(dir)) {
      toAdd.push_back(dir);
    }
  }
  for (const auto &dir : current) {
    if (!desired.contains(dir)) {
      toRemove.push_back(dir);
    }
  }

  if (!toRemove.isEmpty()) {
    m_assetFileWatcher->removePaths(toRemove);
  }
  if (!toAdd.isEmpty()) {
    m_assetFileWatcher->addPaths(toAdd);
  }
}

void EditorMainWindow::PollAssetChanges() {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    return;
  }

  const bool watcherActive =
      m_assetFileWatcher && !m_assetFileWatcher->directories().isEmpty();
  if (watcherActive && !m_assetWatcherDirty) {
    return;
  }
  m_assetWatcherDirty = false;

  registry->Rescan();

  std::vector<Assets::AssetRegistry::AssetChange> changes;
  registry->GetChangesSince(m_assetChangeSerial, changes);
  if (changes.empty()) {
    return;
  }
  m_assetChangeSerial = registry->GetChangeSerial();

  bool selectionRemoved = false;
  bool sceneChanged = false;
  bool sceneRemoved = false;
  std::string selectedId;
  if (!m_selectedAssetId.isEmpty()) {
    selectedId = m_selectedAssetId.toStdString();
  }
  std::string sceneId;
  std::filesystem::path normalizedScenePath;
  if (!m_scenePath.empty()) {
    normalizedScenePath = NormalizePath(m_scenePath);
    if (const auto *sceneEntry =
            registry->FindEntry(m_scenePath.string())) {
      sceneId = sceneEntry->id;
    }
  }
  if (!m_selectedAssetId.isEmpty()) {
    for (const auto &change : changes) {
      if (change.id == selectedId &&
          change.kind == Assets::AssetRegistry::AssetChange::Kind::Removed) {   
        selectionRemoved = true;
      }
      if (change.type == Assets::AssetRegistry::AssetType::Scene &&
          (change.kind ==
               Assets::AssetRegistry::AssetChange::Kind::Removed ||
           change.kind ==
               Assets::AssetRegistry::AssetChange::Kind::Modified ||
           change.kind ==
               Assets::AssetRegistry::AssetChange::Kind::Moved)) {
        if (!sceneId.empty() && change.id == sceneId) {
          sceneChanged = true;
          sceneRemoved =
              change.kind ==
              Assets::AssetRegistry::AssetChange::Kind::Removed;
        } else if (!normalizedScenePath.empty()) {
          if (const auto *entry = registry->FindEntry(change.id)) {
            if (NormalizePath(entry->path) == normalizedScenePath) {
              sceneChanged = true;
              sceneRemoved =
                  change.kind ==
                  Assets::AssetRegistry::AssetChange::Kind::Removed;
            }
          }
        }
      }
    }
  } else {
    for (const auto &change : changes) {
      if (change.type == Assets::AssetRegistry::AssetType::Scene &&
          (change.kind ==
               Assets::AssetRegistry::AssetChange::Kind::Removed ||
           change.kind ==
               Assets::AssetRegistry::AssetChange::Kind::Modified ||
           change.kind ==
               Assets::AssetRegistry::AssetChange::Kind::Moved)) {
        if (!sceneId.empty() && change.id == sceneId) {
          sceneChanged = true;
          sceneRemoved =
              change.kind ==
              Assets::AssetRegistry::AssetChange::Kind::Removed;
          break;
        }
        if (!normalizedScenePath.empty()) {
          if (const auto *entry = registry->FindEntry(change.id)) {
            if (NormalizePath(entry->path) == normalizedScenePath) {
              sceneChanged = true;
              sceneRemoved =
                  change.kind ==
                  Assets::AssetRegistry::AssetChange::Kind::Removed;
              break;
            }
          }
        }
      }
    }
  }

  RefreshAssetBrowser();
  RefreshAssetWatchPaths();

  if (selectionRemoved) {
    if (m_assetBrowser) {
      m_assetBrowser->ClearSelection();
    }
    if (m_meshPreview) {
      m_meshPreview->ClearPreview();
    }
    if (m_inspectorPanel) {
      m_inspectorPanel->SetSelectedEntity(
          m_selection ? m_selection->GetSelectedEntity() : nullptr);
    }
    if (m_auxPanel) {
      m_auxPanel->SetAssetText(tr("None"));
    }
    m_selectedAssetId.clear();
  } else if (!m_selectedAssetId.isEmpty() && m_inspectorPanel) {
    m_inspectorPanel->SetSelectedAsset(m_selectedAssetId);
  }

  if (m_vulkanViewport) {
    m_vulkanViewport->HandleAssetChanges(changes);
  }
  if (m_meshPreview) {
    m_meshPreview->HandleAssetChanges(changes);
  }

  if (sceneChanged) {
    if (m_ignoreNextSceneChange) {
      m_ignoreNextSceneChange = false;
      return;
    }
    if (sceneRemoved) {
      statusBar()->showMessage(tr("Active scene removed on disk"), 4000);
    } else if (!m_sceneDirty) {
      ReloadScene();
      statusBar()->showMessage(tr("Scene reloaded from disk"), 2000);
    } else {
      statusBar()->showMessage(
          tr("Scene changed on disk; reload manually to keep edits"), 4000);
    }
  }
}

void EditorMainWindow::ImportGltfAsset() {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  const QString filter = tr("glTF (*.gltf *.glb)");
  const QString selected =
      QFileDialog::getOpenFileName(this, tr("Import glTF"), QString(), filter);
  if (selected.isEmpty()) {
    return;
  }

  std::filesystem::path sourcePath = selected.toStdString();
  std::filesystem::path rootPath = registry->GetRootPath();
  if (rootPath.empty()) {
    rootPath = std::filesystem::path("assets");
  }

  std::error_code ec;
  std::filesystem::create_directories(rootPath, ec);
  if (ec) {
    statusBar()->showMessage(tr("Failed to prepare assets folder"), 2000);
    return;
  }

  std::filesystem::path normalizedSource = NormalizePath(sourcePath);
  std::filesystem::path normalizedRoot = NormalizePath(rootPath);
  bool insideRoot = PathStartsWith(normalizedSource, normalizedRoot);
  std::filesystem::path importPath = sourcePath;

  if (!insideRoot) {
    const auto choice =
        QMessageBox::question(this, tr("Import glTF"),
                              tr("Selected file is outside the assets folder. "
                                 "Copy into assets/meshes and import?\n\nNote: "
                                 "external dependencies are not copied yet."),
                              QMessageBox::Yes | QMessageBox::Cancel);
    if (choice != QMessageBox::Yes) {
      return;
    }

    std::filesystem::path destDir = normalizedRoot / "meshes";
    std::filesystem::create_directories(destDir, ec);
    if (ec) {
      statusBar()->showMessage(tr("Failed to create assets/meshes"), 2000);
      return;
    }

    importPath = MakeUniquePath(destDir / sourcePath.filename());
    std::filesystem::copy_file(sourcePath, importPath,
                               std::filesystem::copy_options::none, ec);
    if (ec) {
      statusBar()->showMessage(tr("Failed to copy glTF"), 2000);
      return;
    }
  }

  const auto result = registry->ImportGltf(importPath.string());
  if (!result.success) {
    const QString message = tr("GLTF import failed: %1")
                                .arg(QString::fromStdString(result.message));
    AppendConsole(m_console, message, ConsoleSeverity::Error);
    statusBar()->showMessage(message, 3000);
    return;
  }

  registry->Scan(rootPath.string());
  RefreshAssetBrowser();
  RefreshAssetWatchPaths();
  m_assetChangeSerial = registry->GetChangeSerial();
  m_assetWatcherDirty = false;
  if (m_inspectorPanel) {
    m_inspectorPanel->SetAssetRegistry(registry);
  }

  const QString success =
      tr("Imported glTF: %1").arg(QString::fromStdString(result.id));
  AppendConsole(m_console, success, ConsoleSeverity::Info);
  statusBar()->showMessage(success, 3000);
}

void EditorMainWindow::AddAssetToScene(const QString &assetId) {
  if (assetId.isEmpty()) {
    statusBar()->showMessage(tr("Cannot add asset: invalid asset id"), 2000);
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  const std::string idStr = assetId.toStdString();
  const auto *entry = registry->FindEntry(idStr);
  if (!entry) {
    statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
    return;
  }

  if (entry->type == Assets::AssetRegistry::AssetType::Scene) {
    if (!ConfirmSaveIfDirty()) {
      return;
    }
    LoadSceneFromPath(entry->path);
    return;
  }

  if (!m_scene) {
    statusBar()->showMessage(tr("Cannot add asset: no active scene"), 2000);
    return;
  }

  // Only mesh assets can be added to the scene directly
  if (entry->type != Assets::AssetRegistry::AssetType::Mesh) {
    statusBar()->showMessage(tr("Only mesh assets can be added to scene"),
                             2000);
    return;
  }

  // Generate a unique entity ID
  Core::EntityId newId = 1;
  bool hasPrimaryDirectional = false;
  for (const auto &entity : m_scene->GetEntities()) {
    if (entity && entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
    if (entity) {
      if (auto existingLight = entity->GetComponent<Scene::LightComponent>()) {
        if (existingLight->GetType() ==
                Scene::LightComponent::LightType::Directional &&
            existingLight->IsPrimary()) {
          hasPrimaryDirectional = true;
        }
      }
    }
  }

  // Create the entity name from asset path
  std::filesystem::path assetPath = entry->path;
  std::string entityName = assetPath.stem().string();
  if (entityName.empty()) {
    entityName = "New Entity";
  }

  // Create entity with transform and mesh renderer
  auto newEntity = std::make_shared<Scene::Entity>(newId, entityName);

  auto transform = std::make_shared<Scene::TransformComponent>();
  transform->SetPosition(0.0f, 0.0f, 0.0f);
  transform->SetScale(1.0f, 1.0f, 1.0f);

  auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
  meshRenderer->SetMeshAssetId(idStr);
  meshRenderer->SetColor(1.0f, 1.0f, 1.0f);
  {
    if (const auto *cached = registry->GetMesh(idStr);
        cached && !cached->materialIds.empty()) {
      meshRenderer->SetMaterialAssetId(cached->materialIds.front());
    }
  }

  newEntity->AddComponent(transform);
  newEntity->AddComponent(meshRenderer);

  m_scene->AddEntity(newEntity);

  // Update UI
  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(newId);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  if (m_selection) {
    m_selection->SelectEntity(newEntity);
  }

  SetSceneDirty(true);

  const QString msg = tr("Added '%1' to scene as entity '%2'")
                          .arg(assetId)
                          .arg(QString::fromStdString(entityName));
  AppendConsole(m_console, msg, ConsoleSeverity::Info);
  statusBar()->showMessage(msg, 3000);

  FocusCameraOnSelection();
}

void EditorMainWindow::DeleteAsset(const QString &assetId) {
  if (assetId.isEmpty()) {
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  const std::string idStr = assetId.toStdString();
  const auto *entry = registry->FindEntry(idStr);
  if (!entry) {
    statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
    return;
  }

  // Confirm deletion
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Delete Asset"),
      tr("Are you sure you want to delete '%1'?\n\nThis will remove the file "
         "from disk.")
          .arg(assetId),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    return;
  }

  // Delete the file
  std::error_code ec;
  if (std::filesystem::exists(entry->path, ec) &&
      std::filesystem::remove(entry->path, ec)) {
    const std::filesystem::path metaPath =
        Assets::AssetRegistry::GetMetadataPathForAsset(entry->path);
    if (std::filesystem::exists(metaPath, ec)) {
      std::filesystem::remove(metaPath, ec);
    }
    AppendConsole(m_console, tr("Deleted asset: %1").arg(assetId),
                  ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Asset deleted: %1").arg(assetId), 3000);
    RescanAssets();
  } else {
    AppendConsole(m_console, tr("Failed to delete asset: %1").arg(assetId),
                  ConsoleSeverity::Error);
    statusBar()->showMessage(tr("Failed to delete asset"), 2000);
  }
}

void EditorMainWindow::RenameAsset(const QString &assetId) {
  if (assetId.isEmpty()) {
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  const std::string idStr = assetId.toStdString();
  const auto *entry = registry->FindEntry(idStr);
  if (!entry) {
    statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
    return;
  }

  std::filesystem::path oldPath(entry->path);
  QString oldName = QString::fromStdString(oldPath.stem().string());
  QString extension = QString::fromStdString(oldPath.extension().string());

  bool ok = false;
  QString newName =
      QInputDialog::getText(this, tr("Rename Asset"), tr("Enter new name:"),
                            QLineEdit::Normal, oldName, &ok);

  if (!ok || newName.isEmpty() || newName == oldName) {
    return;
  }

  std::filesystem::path newPath =
      oldPath.parent_path() / (newName.toStdString() + extension.toStdString());

  std::error_code ec;
  if (std::filesystem::exists(newPath, ec)) {
    QMessageBox::warning(this, tr("Rename Failed"),
                         tr("A file with that name already exists."));
    return;
  }

  std::filesystem::rename(oldPath, newPath, ec);
  if (ec) {
    AppendConsole(m_console,
                  tr("Failed to rename asset: %1")
                      .arg(QString::fromStdString(ec.message())),
                  ConsoleSeverity::Error);
    return;
  }

  const std::filesystem::path oldMeta =
      Assets::AssetRegistry::GetMetadataPathForAsset(oldPath);
  const std::filesystem::path newMeta =
      Assets::AssetRegistry::GetMetadataPathForAsset(newPath);
  if (std::filesystem::exists(oldMeta, ec)) {
    std::filesystem::rename(oldMeta, newMeta, ec);
    ec.clear();
  }

  AppendConsole(m_console,
                tr("Renamed asset: %1 -> %2").arg(oldName).arg(newName),
                ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Asset renamed"), 3000);
  RescanAssets();
}

void EditorMainWindow::ShowAssetInExplorer(const QString &assetId) {
  if (assetId.isEmpty()) {
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  const std::string idStr = assetId.toStdString();
  const auto *entry = registry->FindEntry(idStr);
  if (!entry) {
    statusBar()->showMessage(tr("Asset not found: %1").arg(assetId), 2000);
    return;
  }

  QString filePath = QString::fromStdString(entry->path.string());

#ifdef Q_OS_WIN
  QProcess::startDetached("explorer.exe",
                          {"/select,", QDir::toNativeSeparators(filePath)});
#elif defined(Q_OS_MAC)
  QProcess::startDetached("open", {"-R", filePath});
#else
  QDesktopServices::openUrl(QUrl::fromLocalFile(
      QString::fromStdString(entry->path.parent_path().string())));
#endif
}

void EditorMainWindow::DeleteEntity(Aetherion::Core::EntityId id) {
  if (id == 0 || !m_scene) {
    return;
  }

  auto entity = m_scene->GetEntityById(id);
  if (!entity) {
    statusBar()->showMessage(tr("Entity not found"), 2000);
    return;
  }

  QString entityName = QString::fromStdString(entity->GetName());

  QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Delete Entity"),
      tr("Are you sure you want to delete '%1'?").arg(entityName),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    return;
  }

  ExecuteCommand(std::make_unique<DeleteEntityCommand>(m_scene, entity));

  if (m_selection) {
    m_selection->Clear();
  }

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  AppendConsole(m_console, tr("Deleted entity: %1").arg(entityName),
                ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Entity deleted: %1").arg(entityName), 3000);
}

void EditorMainWindow::DuplicateEntity(Aetherion::Core::EntityId id) {
  if (id == 0 || !m_scene) {
    return;
  }

  auto sourceEntity = m_scene->GetEntityById(id);
  if (!sourceEntity) {
    statusBar()->showMessage(tr("Entity not found"), 2000);
    return;
  }

  // Generate a unique entity ID
  Core::EntityId newId = 1;
  for (const auto &entity : m_scene->GetEntities()) {
    if (entity && entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
  }

  std::string newName = sourceEntity->GetName() + " (Copy)";
  auto newEntity = std::make_shared<Scene::Entity>(newId, newName);

  // Copy transform component
  auto sourceTransform =
      sourceEntity->GetComponent<Scene::TransformComponent>();
  if (sourceTransform) {
    auto transform = std::make_shared<Scene::TransformComponent>();
    float px = sourceTransform->GetPositionX();
    float py = sourceTransform->GetPositionY();
    float pz = sourceTransform->GetPositionZ();
    transform->SetPosition(px + 0.5f, py, pz); // Offset slightly
    float sx = sourceTransform->GetScaleX();
    float sy = sourceTransform->GetScaleY();
    float sz = sourceTransform->GetScaleZ();
    transform->SetScale(sx, sy, sz);
    transform->SetRotationDegrees(sourceTransform->GetRotationXDegrees(),
                                  sourceTransform->GetRotationYDegrees(),
                                  sourceTransform->GetRotationZDegrees());
    newEntity->AddComponent(transform);
  }

  // Copy mesh renderer component
  auto sourceMesh = sourceEntity->GetComponent<Scene::MeshRendererComponent>();
  if (sourceMesh) {
    auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
    meshRenderer->SetMeshAssetId(sourceMesh->GetMeshAssetId());
    auto [r, g, b] = sourceMesh->GetColor();
    meshRenderer->SetColor(r, g, b);
    newEntity->AddComponent(meshRenderer);
  }

  auto sourceLight = sourceEntity->GetComponent<Scene::LightComponent>();
  if (sourceLight) {
    auto light = std::make_shared<Scene::LightComponent>();
    light->SetEnabled(sourceLight->IsEnabled());
    const auto lightColor = sourceLight->GetColor();
    light->SetColor(lightColor[0], lightColor[1], lightColor[2]);
    light->SetIntensity(sourceLight->GetIntensity());
    const auto ambient = sourceLight->GetAmbientColor();
    light->SetAmbientColor(ambient[0], ambient[1], ambient[2]);
    newEntity->AddComponent(light);
  }

  m_scene->AddEntity(newEntity);
  SetSceneDirty(true);

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(newId);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  if (m_selection) {
    m_selection->SelectEntity(newEntity);
  }

  AppendConsole(
      m_console,
      tr("Duplicated entity: %1").arg(QString::fromStdString(newName)),
      ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Entity duplicated"), 3000);
}

void EditorMainWindow::RenameEntity(Aetherion::Core::EntityId id) {
  if (id == 0 || !m_scene) {
    return;
  }

  auto entity = m_scene->GetEntityById(id);
  if (!entity) {
    statusBar()->showMessage(tr("Entity not found"), 2000);
    return;
  }

  QString oldName = QString::fromStdString(entity->GetName());

  bool ok = false;
  QString newName =
      QInputDialog::getText(this, tr("Rename Entity"), tr("Enter new name:"),
                            QLineEdit::Normal, oldName, &ok);

  if (!ok || newName.isEmpty() || newName == oldName) {
    return;
  }

  ExecuteCommand(std::make_unique<RenameEntityCommand>(
      entity, oldName.toStdString(), newName.toStdString()));
  // entity->SetName(newName.toStdString()); // Handled by Command
  // SetSceneDirty(true); // Handled by ExecuteCommand

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(id);
  }

  AppendConsole(m_console,
                tr("Renamed entity: %1 -> %2").arg(oldName).arg(newName),
                ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Entity renamed"), 3000);
}

Core::EntityId EditorMainWindow::AllocateEntityId() const {
  Core::EntityId newId = 1;
  if (!m_scene) {
    return newId;
  }

  for (const auto &entity : m_scene->GetEntities()) {
    if (entity && entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
  }

  return newId;
}

void EditorMainWindow::HandleCopilotPrompt(const QString &prompt) {
  if (!m_copilotPanel || !m_copilotProcessor) {
    return;
  }

  m_copilotPanel->SetProcessing(true);

  // Ensure processor has latest context
  m_copilotProcessor->SetScene(m_scene);
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  m_copilotProcessor->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
   if (m_selection) {
    m_copilotProcessor->SetSelectedEntity(m_selection->GetSelectedEntity());
  }

  CopilotResult result = m_copilotProcessor->ProcessPrompt(prompt);

  if (!result.response.isEmpty()) {
    QString responseText = result.response;
    if (result.dryRun && !result.previewActions.empty()) {
      responseText += "\n";
      for (const auto &action : result.previewActions) {
        responseText += "- " + action + "\n";
      }
      const auto decision = QMessageBox::question(
          this, tr("Apply Copilot Actions?"),
          tr("%1\nApply these changes?").arg(responseText.trimmed()),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
      if (decision == QMessageBox::Yes) {
        auto applied = m_copilotProcessor->ProcessPrompt(prompt, false);
        if (!applied.response.isEmpty()) {
          m_copilotPanel->AppendMessage("Copilot", applied.response);
          AppendConsole(m_console, applied.response, ConsoleSeverity::Info);
          statusBar()->showMessage(applied.response, 2500);
        }
        return;
      }
    }
    m_copilotPanel->AppendMessage("Copilot", responseText.trimmed());
    AppendConsole(m_console, result.response, ConsoleSeverity::Info);
    statusBar()->showMessage(result.response, 2500);
  }

  // Select the last created entity if any
  if (!result.createdEntityIds.empty()) {
    Core::EntityId lastId = result.createdEntityIds.back();
    
    if (m_hierarchyPanel) {
      m_hierarchyPanel->BindScene(m_scene);
      m_hierarchyPanel->SetSelectedEntity(lastId);
    }
    if (m_statsPanel) {
      m_statsPanel->RefreshStats();
    }
    
    if (m_selection && m_scene) {
        auto entity = m_scene->GetEntityById(lastId);
        if (entity) {
            m_selection->SelectEntity(entity);
        }
    }
  }

  if (result.requestFocus) {
    FocusCameraOnSelection();
  }

  UpdateAiHudFromSelection();

  m_copilotPanel->SetProcessing(false);
}

void EditorMainWindow::CreateEmptyEntity(Aetherion::Core::EntityId parentId) {
  if (!m_scene) {
    statusBar()->showMessage(tr("No active scene"), 2000);
    return;
  }

  // Generate a unique entity ID
  Core::EntityId newId = 1;
  for (const auto &entity : m_scene->GetEntities()) {
    if (entity && entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
  }

  auto newEntity = std::make_shared<Scene::Entity>(newId, "New Entity");

  auto transform = std::make_shared<Scene::TransformComponent>();
  transform->SetPosition(0.0f, 0.0f, 0.0f);
  transform->SetScale(1.0f, 1.0f, 1.0f);

  if (parentId != 0) {
    transform->SetParent(parentId);
  }

  newEntity->AddComponent(transform);

  ExecuteCommand(std::make_unique<CreateEntityCommand>(m_scene, newEntity));

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(newId);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  if (m_selection) {
    m_selection->SelectEntity(newEntity);
  }

  AppendConsole(m_console, tr("Created new entity"), ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Entity created"), 3000);
}

void EditorMainWindow::CreateLightEntity(Aetherion::Core::EntityId parentId) {
  if (!m_scene) {
    statusBar()->showMessage(tr("No active scene"), 2000);
    return;
  }

  Core::EntityId newId = 1;
  for (const auto &entity : m_scene->GetEntities()) {
    if (entity && entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
  }

  auto newEntity = std::make_shared<Scene::Entity>(newId, "Directional Light");

  auto transform = std::make_shared<Scene::TransformComponent>();
  transform->SetPosition(0.0f, 0.0f, 0.0f);
  transform->SetScale(1.0f, 1.0f, 1.0f);
  transform->SetRotationDegrees(-55.0f, 215.0f, 0.0f);
  if (parentId != 0) {
    transform->SetParent(parentId);
  }

  auto light = std::make_shared<Scene::LightComponent>();
  light->SetType(Scene::LightComponent::LightType::Directional);
  bool hasPrimaryDirectional = false;
  for (const auto &entity : m_scene->GetEntities()) {
    if (entity) {
      if (auto existingLight = entity->GetComponent<Scene::LightComponent>()) {
        if (existingLight->GetType() ==
                Scene::LightComponent::LightType::Directional &&
            existingLight->IsPrimary()) {
          hasPrimaryDirectional = true;
          break;
        }
      }
    }
  }
  light->SetPrimary(!hasPrimaryDirectional);
  newEntity->AddComponent(transform);
  newEntity->AddComponent(light);

  ExecuteCommand(std::make_unique<CreateEntityCommand>(m_scene, newEntity));

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(newId);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  if (m_selection) {
    m_selection->SelectEntity(newEntity);
  }

  AppendConsole(m_console, tr("Created directional light"),
                ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Directional light created"), 3000);
}

void EditorMainWindow::CreateCameraEntity(Aetherion::Core::EntityId parentId) {
  if (!m_scene) {
    statusBar()->showMessage(tr("No active scene"), 2000);
    return;
  }

  Core::EntityId newId = 1;
  bool hasPrimaryCamera = false;
  for (const auto &entity : m_scene->GetEntities()) {
    if (!entity) {
      continue;
    }
    if (entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
    if (auto camera = entity->GetComponent<Scene::CameraComponent>()) {
      if (camera->IsPrimary()) {
        hasPrimaryCamera = true;
      }
    }
  }

  auto newEntity = std::make_shared<Scene::Entity>(newId, "Camera");
  auto transform = std::make_shared<Scene::TransformComponent>();
  transform->SetPosition(0.0f, 0.0f, 5.0f);
  transform->SetRotationDegrees(0.0f, 0.0f, 0.0f);
  transform->SetScale(1.0f, 1.0f, 1.0f);
  if (parentId != 0) {
    transform->SetParent(parentId);
  }

  auto camera = std::make_shared<Scene::CameraComponent>();
  camera->SetPrimary(!hasPrimaryCamera);

  newEntity->AddComponent(transform);
  newEntity->AddComponent(camera);

  ExecuteCommand(std::make_unique<CreateEntityCommand>(m_scene, newEntity));

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(newId);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  if (m_selection) {
    m_selection->SelectEntity(newEntity);
  }

  AppendConsole(m_console, tr("Created camera"), ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Camera created"), 3000);
}

void EditorMainWindow::CreateMeshEntity(Aetherion::Core::EntityId parentId,
                                        const QString &meshAssetId,
                                        const QString &displayName) {
  if (!m_scene) {
    statusBar()->showMessage(tr("No active scene"), 2000);
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  if (!registry) {
    statusBar()->showMessage(tr("Asset registry unavailable"), 2000);
    return;
  }

  const std::string assetRef = meshAssetId.toStdString();
  const auto *entry = registry->FindEntry(assetRef);
  if (!entry || entry->type != Assets::AssetRegistry::AssetType::Mesh) {
    statusBar()->showMessage(tr("Mesh asset not found: %1").arg(meshAssetId),
                             2000);
    return;
  }

  Core::EntityId newId = 1;
  for (const auto &entity : m_scene->GetEntities()) {
    if (entity && entity->GetId() >= newId) {
      newId = entity->GetId() + 1;
    }
  }

  QString entityLabel = displayName.trimmed();
  if (entityLabel.isEmpty()) {
    entityLabel = QString::fromStdString(entry->path.stem().string());
  }
  if (entityLabel.isEmpty()) {
    entityLabel = tr("New Object");
  }

  auto newEntity =
      std::make_shared<Scene::Entity>(newId, entityLabel.toStdString());
  auto transform = std::make_shared<Scene::TransformComponent>();
  transform->SetPosition(0.0f, 0.0f, 0.0f);
  transform->SetScale(1.0f, 1.0f, 1.0f);
  if (parentId != 0) {
    transform->SetParent(parentId);
  }

  auto meshRenderer = std::make_shared<Scene::MeshRendererComponent>();
  meshRenderer->SetMeshAssetId(assetRef);
  meshRenderer->SetColor(1.0f, 1.0f, 1.0f);
  if (const auto *cached = registry->GetMesh(entry->id);
      cached && !cached->materialIds.empty()) {
    meshRenderer->SetMaterialAssetId(cached->materialIds.front());
  }

  newEntity->AddComponent(transform);
  newEntity->AddComponent(meshRenderer);

  ExecuteCommand(std::make_unique<CreateEntityCommand>(m_scene, newEntity));

  if (m_hierarchyPanel) {
    m_hierarchyPanel->BindScene(m_scene);
    m_hierarchyPanel->SetSelectedEntity(newId);
  }
  if (m_statsPanel) {
    m_statsPanel->RefreshStats();
  }

  if (m_selection) {
    m_selection->SelectEntity(newEntity);
  }

  AppendConsole(m_console, tr("Created mesh object '%1'").arg(entityLabel),
                ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Mesh object created"), 3000);
}

void EditorMainWindow::OpenScene() {
  if (!ConfirmSaveIfDirty()) {
    return;
  }

  const std::filesystem::path sceneRoot = GetAssetsRootPath() / "scenes";
  const QString startDir = QString::fromStdString(sceneRoot.generic_string());
  const QString filter = tr("Scene Files (*.json)");
  const QString selected =
      QFileDialog::getOpenFileName(this, tr("Open Scene"), startDir, filter);
  if (selected.isEmpty()) {
    return;
  }

  LoadSceneFromPath(std::filesystem::path(selected.toStdString()));
}

void EditorMainWindow::SaveScene() {
  if (m_scenePath.empty()) {
    m_scenePath = GetDefaultScenePath();
  }
  SaveSceneToPath(m_scenePath);
}

void EditorMainWindow::ReloadScene() {
  if (!ConfirmSaveIfDirty()) {
    return;
  }
  if (m_scenePath.empty()) {
    m_scenePath = GetDefaultScenePath();
  }
  LoadSceneFromPath(m_scenePath);
}

bool EditorMainWindow::ConfirmSaveIfDirty() {
  if (!m_sceneDirty) {
    return true;
  }

  const auto choice = QMessageBox::question(
      this, tr("Unsaved Changes"),
      tr("The current scene has unsaved changes. Save before continuing?"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
  if (choice == QMessageBox::Cancel) {
    return false;
  }
  if (choice == QMessageBox::Yes) {
    return SaveSceneToPath(m_scenePath);
  }
  return true;
}

bool EditorMainWindow::SaveSceneToPath(const std::filesystem::path &path) {
  if (!m_scene || !m_runtimeApp) {
    statusBar()->showMessage(tr("No scene to save"), 2000);
    return false;
  }

  auto ctx = m_runtimeApp->GetContext();
  if (!ctx) {
    statusBar()->showMessage(tr("Runtime context unavailable"), 2000);
    return false;
  }

  const std::filesystem::path target =
      path.empty() ? GetDefaultScenePath() : path;
  Scene::SceneSerializer serializer(*ctx);
  if (!serializer.Save(*m_scene, target)) {
    statusBar()->showMessage(tr("Failed to save scene"), 2000);
    return false;
  }

  m_scenePath = target;
  SetSceneDirty(false);
  m_ignoreNextSceneChange = true;
  statusBar()->showMessage(tr("Scene saved"), 2000);
  return true;
}

bool EditorMainWindow::LoadSceneFromPath(const std::filesystem::path &path) {
  if (!m_runtimeApp) {
    statusBar()->showMessage(tr("Runtime unavailable"), 2000);
    return false;
  }

  auto ctx = m_runtimeApp->GetContext();
  if (!ctx) {
    statusBar()->showMessage(tr("Runtime context unavailable"), 2000);
    return false;
  }

  const std::filesystem::path target =
      path.empty() ? GetDefaultScenePath() : path;
  Scene::SceneSerializer serializer(*ctx);
  auto loaded = serializer.Load(target);
  if (!loaded) {
    statusBar()->showMessage(tr("Failed to load scene"), 2000);
    return false;
  }

  m_scenePath = target;
  m_scene = loaded;
  m_ignoreNextSceneChange = false;
  ClearPlaySessionSnapshot();
  if (m_runtimeApp) {
    m_runtimeApp->SetActiveScene(m_scene);
  }

  if (m_selection) {
    m_selection->SetActiveScene(m_scene);
    if (!m_scene) {
      m_selection->Clear();
    }
  }

  if (m_hierarchyPanel) {
    m_hierarchyPanel->SetSelectionModel(m_selection);
    m_hierarchyPanel->BindScene(m_scene);
  }
  if (m_statsPanel) {
    m_statsPanel->SetScene(m_scene);
  }

  if (m_inspectorPanel) {
    m_inspectorPanel->SetSelectedEntity(
        m_selection ? m_selection->GetSelectedEntity() : nullptr);
  }
  if (m_cameraPreview) {
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    m_cameraPreview->SetScene(m_scene);
    m_cameraPreview->SetRenderViewSource(ctx ? ctx->GetRenderView() : nullptr);
  }

  if (m_commandHistory) {
    m_commandHistory->Clear();
    UpdateUndoRedoState();
  }

  SetSceneDirty(false);
  statusBar()->showMessage(tr("Scene loaded"), 2000);
  return true;
}

void EditorMainWindow::RecreateRuntimeAndRenderer(bool enableValidation) {
  m_validationEnabled = enableValidation;
  m_settings.validationEnabled = enableValidation;

  DestroyViewportRenderer();
  DetachVulkanLogSink();

  if (m_runtimeApp) {
    m_runtimeApp->Shutdown();
  }

  m_runtimeApp = std::make_shared<Runtime::EngineApplication>();
  try {
    m_runtimeApp->Initialize(m_validationEnabled, m_renderLoggingEnabled);
  } catch (const std::exception &ex) {
    AppendConsole(m_console, QString::fromStdString(ex.what()),
                  ConsoleSeverity::Error);
    statusBar()->showMessage(
        tr("Renderer reset failed: %1").arg(QString::fromStdString(ex.what())));
    m_runtimeApp.reset();
    m_scene.reset();
    return;
  }

  if (m_runtimeApp) {
    m_runtimeApp->SetSimulationPlaying(false);
    m_runtimeApp->SetSimulationPaused(false);
  }

  ApplyRuntimeAISettings();

  m_scene = m_runtimeApp->GetActiveScene();
  ClearPlaySessionSnapshot();
  if (m_selection) {
    m_selection->SetActiveScene(m_scene);
  }

  if (m_hierarchyPanel) {
    m_hierarchyPanel->SetSelectionModel(m_selection);
    m_hierarchyPanel->BindScene(m_scene);
  }
  if (m_statsPanel) {
    m_statsPanel->SetScene(m_scene);
  }
  m_scenePath = GetDefaultScenePath();
  SetSceneDirty(false);

  if (m_commandHistory) {
    m_commandHistory->Clear();
    UpdateUndoRedoState();
  }

  if (m_selection) {
    if (!m_scene) {
      m_selection->Clear();
    }
  }

  if (m_inspectorPanel) {
    m_inspectorPanel->SetSelectedEntity(
        m_selection ? m_selection->GetSelectedEntity() : nullptr);
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    m_inspectorPanel->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
  }
  if (m_cameraPreview) {
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    m_cameraPreview->SetScene(m_scene);
    m_cameraPreview->SetAssetRegistry(ctx ? ctx->GetAssetRegistry() : nullptr);
    m_cameraPreview->SetRenderViewSource(ctx ? ctx->GetRenderView() : nullptr);
    m_cameraPreview->SetVulkanContext(ctx ? ctx->GetVulkanContext() : nullptr);
  }

  AttachVulkanLogSink();
  RefreshAssetBrowser();

  if (m_surfaceInitialized && m_surfaceHandle != 0) {
    auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
    auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
    if (vk) {
      try {
        DestroyViewportRenderer();
        auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
        m_vulkanViewport =
            std::make_unique<Rendering::VulkanViewport>(vk, registry);
        m_vulkanViewport->SetLoggingEnabled(m_renderLoggingEnabled);
        m_vulkanViewport->Initialize(reinterpret_cast<void *>(m_surfaceHandle),
                                     m_surfaceSize.width(),
                                     m_surfaceSize.height());

        // Sync camera from viewport widget
        if (m_viewport) {
          m_vulkanViewport->SetCameraPosition(m_viewport->getCameraX(),
                                              m_viewport->getCameraY(),
                                              m_viewport->getCameraZ());
          m_vulkanViewport->SetCameraRotation(m_viewport->getCameraRotationY(),
                                              m_viewport->getCameraRotationX());
          m_vulkanViewport->SetCameraZoom(m_viewport->getCameraZoom());
        }

        if (m_vulkanViewport->IsReady()) {
          m_frameTimer.restart();
          m_renderTimer->start();
          m_fpsFrameCounter = 0;
          m_fpsTimer.start();
        }
      } catch (const std::exception &ex) {
        AppendConsole(m_console, QString::fromStdString(ex.what()),
                      ConsoleSeverity::Error);
        statusBar()->showMessage(tr("Renderer reset failed: %1")
                                     .arg(QString::fromStdString(ex.what())));
      }
    }
  }

  UpdateRenderTimerInterval(m_vulkanViewport && m_vulkanViewport->IsReady());
  statusBar()->showMessage(
      tr("Renderer reset (%1 validation, %2 logging)")
          .arg(m_validationEnabled ? tr("with") : tr("without"))
          .arg(m_renderLoggingEnabled ? tr("verbose") : tr("minimal")));
}

void EditorMainWindow::DestroyViewportRenderer() {
  if (m_renderTimer) {
    m_renderTimer->stop();
  }

  if (m_vulkanViewport) {
    m_vulkanViewport->Shutdown();
    m_vulkanViewport.reset();
  }

  m_frameTimer.invalidate();
  m_fpsTimer.invalidate();
  m_fpsFrameCounter = 0;
}

void EditorMainWindow::AttachVulkanLogSink() {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
  if (!vk) {
    return;
  }

  vk->SetLoggingEnabled(m_renderLoggingEnabled);
  vk->SetLogCallback(
      [this](Rendering::LogSeverity severity, const std::string &message) {
        AppendConsole(m_console, QString::fromStdString(message),
                      ToConsoleSeverity(severity));
      });
}

void EditorMainWindow::DetachVulkanLogSink() {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto vk = ctx ? ctx->GetVulkanContext() : nullptr;
  if (vk) {
    vk->SetLogCallback(nullptr);
  }

  UpdateAiHudFromSelection();
}

void EditorMainWindow::UpdateAiHudFromSelection() {
  if (!m_viewport) {
    return;
  }

  if (!m_aiHudVisible) {
    m_viewport->SetAiHudText({});
    return;
  }

  QString text = tr("AI: --");
  auto entity = m_selection ? m_selection->GetSelectedEntity() : nullptr;
  if (entity) {
    if (auto ai = entity->GetComponent<Scene::AIBehaviorComponent>()) {
      const QString state =
          QString::fromStdString(ai->GetCurrentState().empty()
                                     ? std::string("Idle")
                                     : ai->GetCurrentState());
      const QString reason =
          QString::fromStdString(ai->GetLastReason().empty()
                                     ? std::string("No reason")
                                     : ai->GetLastReason());
      text = tr("AI: %1 | %2").arg(state, reason.left(80));

      const QString historyEntry =
          tr("%1 (%2)").arg(state, reason.left(40));
      if (m_aiHudHistory.empty() || m_aiHudHistory.back() != historyEntry) {
        m_aiHudHistory.push_back(historyEntry);
        constexpr size_t kMaxHistory = 5;
        if (m_aiHudHistory.size() > kMaxHistory) {
          m_aiHudHistory.pop_front();
        }
      }

      QString historyText;
      for (const auto &entry : m_aiHudHistory) {
        if (!historyText.isEmpty()) {
          historyText += tr(" -> ");
        }
        historyText += entry;
      }

      if (!historyText.isEmpty()) {
        text += tr(" | %1").arg(historyText);
      }
    }
  }

  m_viewport->SetAiHudText(text);
}

void EditorMainWindow::LoadLayout() {
  QSettings settings("Aetherion", "Editor");

  // Capture the default/factory layout before applying any saved state.        
  // Used by the "Reset Layout" action.
  if (m_defaultLayoutState.isEmpty()) {
    m_defaultLayoutState = saveState();
    m_defaultLayoutGeometry = saveGeometry();
    if (m_panelManager) {
      m_panelManager->SavePanelState(settings, "layout/defaultPanel");
      m_defaultPanelVerticalState =
          settings.value("layout/defaultPanel/VerticalSplitterState")
              .toByteArray();
      m_defaultPanelHorizontalState =
          settings.value("layout/defaultPanel/HorizontalSplitterState")
              .toByteArray();
      m_defaultLeftTabIndex =
          settings.value("layout/defaultPanel/LeftPanelCurrentIndex", 0)
              .toInt();
      m_defaultRightTabIndex =
          settings.value("layout/defaultPanel/RightPanelCurrentIndex", 0)
              .toInt();
      m_defaultBottomTabIndex =
          settings.value("layout/defaultPanel/BottomPanelCurrentIndex", 0)
              .toInt();
      m_defaultBottomVisible =
          settings.value("layout/defaultPanel/BottomPanelVisible", true)
              .toBool();
    }
  }

  const QByteArray geometry =
      settings.value("layout/mainWindowGeometry").toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  } else {
    settings.setValue("layout/mainWindowGeometry", saveGeometry());
  }

  const QByteArray saved = settings.value("layout/mainWindow").toByteArray();
  if (!saved.isEmpty()) {
    restoreState(saved);
  } else {
    settings.setValue("layout/mainWindow", saveState());
  }

  if (m_panelManager) {
    m_panelManager->RestorePanelState(settings);
    if (m_showBottomPanelAction) {
      m_showBottomPanelAction->blockSignals(true);
      m_showBottomPanelAction->setChecked(
          m_panelManager->IsBottomPanelVisible());
      m_showBottomPanelAction->blockSignals(false);
    }
  }
}

void EditorMainWindow::SaveLayout() const {
  QSettings settings("Aetherion", "Editor");
  settings.setValue("layout/mainWindow", saveState());
  settings.setValue("layout/mainWindowGeometry", saveGeometry());
  if (m_panelManager) {
    m_panelManager->SavePanelState(settings);
  }
}

std::filesystem::path EditorMainWindow::GetBookmarksPath() const {
  return std::filesystem::path(".cache") / "camera_bookmarks.json";
}

void EditorMainWindow::LoadBookmarks() {
  m_bookmarks.clear();
  const std::filesystem::path path = GetBookmarksPath();
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return;
  }

  try {
    std::ifstream input(path);
    if (!input.is_open()) {
      return;
    }
    nlohmann::json root;
    input >> root;
    if (!root.is_array()) {
      return;
    }
    for (const auto &entry : root) {
      if (!entry.is_object()) {
        continue;
      }
      CameraBookmark bm;
      bm.name = QString::fromStdString(
          entry.value("name", std::string("Bookmark")));
      bm.posX = entry.value("posX", 0.0f);
      bm.posY = entry.value("posY", 0.0f);
      bm.posZ = entry.value("posZ", 0.0f);
      bm.rotY = entry.value("rotY", 0.0f);
      bm.rotX = entry.value("rotX", 0.0f);
      bm.zoom = entry.value("zoom", 1.0f);
      m_bookmarks.push_back(std::move(bm));
    }
  } catch (...) {
    // ignore malformed bookmark file
  }
}

void EditorMainWindow::SaveBookmarks() const {
  const std::filesystem::path path = GetBookmarksPath();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);

  nlohmann::json root = nlohmann::json::array();
  for (const auto &bm : m_bookmarks) {
    nlohmann::json entry;
    entry["name"] = bm.name.toStdString();
    entry["posX"] = bm.posX;
    entry["posY"] = bm.posY;
    entry["posZ"] = bm.posZ;
    entry["rotY"] = bm.rotY;
    entry["rotX"] = bm.rotX;
    entry["zoom"] = bm.zoom;
    root.push_back(entry);
  }

  std::ofstream output(path, std::ios::trunc);
  if (output.is_open()) {
    output << root.dump(2);
  }
}

void EditorMainWindow::RefreshBookmarksList() {
  if (!m_bookmarkList) {
    return;
  }
  m_bookmarkList->clear();
  for (const auto &bm : m_bookmarks) {
    auto *item = new QListWidgetItem(bm.name, m_bookmarkList);
    item->setToolTip(
      tr("Pos: %1, %2, %3 | Rot: %4, %5 | Zoom: %6")
        .arg(bm.posX, 0, 'f', 2)
        .arg(bm.posY, 0, 'f', 2)
        .arg(bm.posZ, 0, 'f', 2)
        .arg(bm.rotY, 0, 'f', 1)
        .arg(bm.rotX, 0, 'f', 1)
        .arg(bm.zoom, 0, 'f', 2));
    m_bookmarkList->addItem(item);
  }
  if (m_renameBookmarkBtn)
    m_renameBookmarkBtn->setEnabled(!m_bookmarks.empty());
  if (m_deleteBookmarkBtn)
    m_deleteBookmarkBtn->setEnabled(!m_bookmarks.empty());
}

void EditorMainWindow::AddBookmarkFromCamera() {
  if (!m_viewport) {
    return;
  }
  CameraBookmark bm{};
  bm.posX = m_viewport->getCameraX();
  bm.posY = m_viewport->getCameraY();
  bm.posZ = m_viewport->getCameraZ();
  bm.rotY = m_viewport->getCameraRotationY();
  bm.rotX = m_viewport->getCameraRotationX();
  bm.zoom = m_viewport->getCameraZoom();

  const int nextIdx = static_cast<int>(m_bookmarks.size()) + 1;
  bm.name = tr("Bookmark %1").arg(nextIdx);
  m_bookmarks.push_back(bm);
  SaveBookmarks();
  RefreshBookmarksList();
}

void EditorMainWindow::RenameBookmark() {
  if (!m_bookmarkList) {
    return;
  }
  auto *item = m_bookmarkList->currentItem();
  const int row = item ? m_bookmarkList->row(item) : -1;
  if (row < 0 || row >= static_cast<int>(m_bookmarks.size())) {
    return;
  }

  bool ok = false;
  const QString newName = QInputDialog::getText(
      this, tr("Rename Bookmark"), tr("Name:"), QLineEdit::Normal,
      m_bookmarks[static_cast<size_t>(row)].name, &ok);
  if (!ok) {
    return;
  }
  const QString trimmed = newName.trimmed();
  if (trimmed.isEmpty()) {
    return;
  }
  m_bookmarks[static_cast<size_t>(row)].name = trimmed;
  SaveBookmarks();
  RefreshBookmarksList();
  if (m_bookmarkList && row < m_bookmarkList->count()) {
    m_bookmarkList->setCurrentRow(row);
  }
}

void EditorMainWindow::DeleteBookmark() {
  if (!m_bookmarkList) {
    return;
  }
  auto *item = m_bookmarkList->currentItem();
  const int row = item ? m_bookmarkList->row(item) : -1;
  if (row < 0 || row >= static_cast<int>(m_bookmarks.size())) {
    return;
  }
  m_bookmarks.erase(m_bookmarks.begin() + row);
  SaveBookmarks();
  RefreshBookmarksList();
}

void EditorMainWindow::ApplyBookmark(int row) {
  if (row < 0 || row >= static_cast<int>(m_bookmarks.size())) {
    return;
  }
  const auto &bm = m_bookmarks[static_cast<size_t>(row)];
  if (m_viewport) {
    m_viewport->SetCameraState(bm.posX, bm.posY, bm.posZ, bm.rotY, bm.rotX,
                               bm.zoom);
  }
  if (m_vulkanViewport) {
    m_vulkanViewport->SetCameraPosition(bm.posX, bm.posY, bm.posZ);
    m_vulkanViewport->SetCameraRotation(bm.rotY, bm.rotX);
    m_vulkanViewport->SetCameraZoom(bm.zoom);
  }
  statusBar()->showMessage(
      tr("Camera moved to %1").arg(bm.name), 2000);
}

void EditorMainWindow::closeEvent(QCloseEvent *event) {
  if (!ConfirmSaveIfDirty()) {
    event->ignore();
    return;
  }
  SaveLayout();
  QMainWindow::closeEvent(event);
}

void EditorMainWindow::CreateTabPanels() {
  if (!m_panelManager) {
    return;
  }

  m_panelManager->CreatePanelGroups(this);

  // Left panel tabs
  m_assetBrowser = new EditorAssetBrowser(m_panelManager);
  m_hierarchyPanel = new EditorHierarchyPanel(m_panelManager);
  m_panelManager->AddToLeftPanel(m_assetBrowser, tr("Assets"));
  m_panelManager->AddToLeftPanel(m_hierarchyPanel, tr("Hierarchy"));

  // Bookmarks tab (right side)
  auto *bookmarkContainer = new QWidget(m_panelManager);
  auto *bookmarkLayout = new QVBoxLayout(bookmarkContainer);
  bookmarkLayout->setContentsMargins(6, 6, 6, 6);
  bookmarkLayout->setSpacing(6);

  m_bookmarkList = new QListWidget(bookmarkContainer);
  m_bookmarkList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_bookmarkList->setContextMenuPolicy(Qt::CustomContextMenu);
  bookmarkLayout->addWidget(m_bookmarkList, 1);

  auto *btnRow = new QWidget(bookmarkContainer);
  auto *btnLayout = new QHBoxLayout(btnRow);
  btnLayout->setContentsMargins(0, 0, 0, 0);
  btnLayout->setSpacing(4);

  m_addBookmarkBtn = new QPushButton(tr("Add"), btnRow);
  m_renameBookmarkBtn = new QPushButton(tr("Rename"), btnRow);
  m_deleteBookmarkBtn = new QPushButton(tr("Delete"), btnRow);
  btnLayout->addWidget(m_addBookmarkBtn);
  btnLayout->addWidget(m_renameBookmarkBtn);
  btnLayout->addWidget(m_deleteBookmarkBtn);
  btnLayout->addStretch(1);
  bookmarkLayout->addWidget(btnRow);
  bookmarkContainer->setLayout(bookmarkLayout);

  // Right panel tabs
  m_inspectorPanel = new EditorInspectorPanel(m_panelManager);
  m_inspectorPanel->SetCommandExecutor(
      [this](std::unique_ptr<Command> cmd) { ExecuteCommand(std::move(cmd)); });

  m_meshPreview = new EditorMeshPreview(m_panelManager);
  m_cameraPreview = new EditorCameraPreview(m_panelManager);
  m_copilotPanel = new AICopilotPanel(m_panelManager);
  m_auxPanel = new EditorAuxPanel(m_panelManager);

  // Initialize Logic Copilot for code generation
  m_logicCopilot = std::make_unique<Scripting::LogicCopilot>();
  m_logicCopilot->SetOutputDirectory(std::filesystem::path("Engine/Generated"));
  m_logicCopilot->SetProjectRoot(std::filesystem::path("."));
  
  m_logicCopilotPanel = new EditorLogicCopilotPanel(m_panelManager);
  m_logicCopilotPanel->SetLogicCopilot(m_logicCopilot.get());
  
  m_assetGenPanel = new EditorAssetGenerationPanel(m_panelManager);
  m_assetGenPanel->setObjectName("AssetGenerationPanel");
  m_assetGenPanel->setFeatures(QDockWidget::NoDockWidgetFeatures);
  if (m_runtimeApp) {
    auto context = m_runtimeApp->GetContext();
    if (context) {
      m_assetGenPanel->SetAssetRegistry(context->GetAssetRegistry());
    }
  }
  m_assetGenPanel->ConfigureLLMGenerator(m_settings.llm);

  m_statsPanel = new EditorStatisticsPanel(m_panelManager);
  m_statsPanel->setObjectName("StatisticsPanel");
  if (m_scene) {
    m_statsPanel->SetScene(m_scene);
  }

  m_panelManager->AddToRightPanel(m_inspectorPanel, tr("Inspector"));
  m_panelManager->AddToRightPanel(m_meshPreview, tr("Mesh Preview"));
  m_panelManager->AddToRightPanel(m_cameraPreview, tr("Camera Preview"));
  m_panelManager->AddToRightPanel(m_copilotPanel, tr("AI Copilot"));
  m_panelManager->AddToRightPanel(m_logicCopilotPanel, tr("Logic Copilot"));
  m_panelManager->AddToRightPanel(m_assetGenPanel, tr("Asset Generation"));
  m_panelManager->AddToRightPanel(bookmarkContainer, tr("Bookmarks"));
  m_panelManager->AddToRightPanel(m_auxPanel, tr("Quick Info"));

  // Bottom panel tabs
  m_console = new EditorConsole(m_panelManager);
  m_panelManager->AddToBottomPanel(m_console, tr("Console"));
  m_panelManager->AddToBottomPanel(m_statsPanel, tr("Statistics"));

  connect(m_addBookmarkBtn, &QPushButton::clicked, this,
          &EditorMainWindow::AddBookmarkFromCamera);
  connect(m_renameBookmarkBtn, &QPushButton::clicked, this,
          &EditorMainWindow::RenameBookmark);
  connect(m_deleteBookmarkBtn, &QPushButton::clicked, this,
          &EditorMainWindow::DeleteBookmark);
  connect(m_bookmarkList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            ApplyBookmark(m_bookmarkList->row(item));
          });
  connect(m_bookmarkList, &QListWidget::itemSelectionChanged, this, [this]() {
    const bool hasSel =
        m_bookmarkList && !m_bookmarkList->selectedItems().isEmpty();
    if (m_renameBookmarkBtn)
      m_renameBookmarkBtn->setEnabled(hasSel);
    if (m_deleteBookmarkBtn)
      m_deleteBookmarkBtn->setEnabled(hasSel);
  });
  if (m_renameBookmarkBtn)
    m_renameBookmarkBtn->setEnabled(false);
  if (m_deleteBookmarkBtn)
    m_deleteBookmarkBtn->setEnabled(false);

  connect(m_copilotPanel, &AICopilotPanel::PromptSubmitted, this,
          [this](const QString &prompt) {
            m_copilotPanel->SetProcessing(true);
            HandleCopilotPrompt(prompt);
            m_copilotPanel->SetProcessing(false);
          });

  connect(m_assetGenPanel, &EditorAssetGenerationPanel::requestAssetBrowserRefresh,
          this, &EditorMainWindow::RefreshAssetBrowser);
  connect(m_assetGenPanel, &EditorAssetGenerationPanel::assetGenerated,
          this, [this](const QString &assetId, const QString &path) {
            if (m_console) {
              m_console->AppendMessage(QString("Generated asset: %1 -> %2").arg(assetId, path), ConsoleSeverity::Info);
            }
          });
  connect(m_assetGenPanel, &EditorAssetGenerationPanel::generationFailed,
          this, [this](const QString &assetId, const QString &error) {
            if (m_console) {
              m_console->AppendMessage(QString("Generation failed: %1 - %2").arg(assetId, error), ConsoleSeverity::Error);
            }
          });
}

void EditorMainWindow::ConfigureStatusBar() {
  statusBar()->showMessage(tr("Aetherion scaffolding - runtime disconnected"));
  if (!m_fpsLabel) {
    m_fpsLabel = new QLabel(tr("FPS: --"), this);
    m_fpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(m_fpsLabel);
  }
}

void EditorMainWindow::UpdateWindowTitle() {
  QString title = tr("Aetherion Editor");
  if (m_scene) {
    const std::string name =
        m_scene->GetName().empty() ? std::string("Scene") : m_scene->GetName();
    title = tr("Aetherion Editor - %1").arg(QString::fromStdString(name));
  }
  if (m_sceneDirty) {
    title += tr(" *");
  }
  setWindowTitle(title);
}

void EditorMainWindow::SetSceneDirty(bool dirty) {
  if (m_sceneDirty == dirty) {
    return;
  }
  m_sceneDirty = dirty;
  UpdateWindowTitle();
  if (dirty) {
    statusBar()->showMessage(tr("Scene modified"), 2000);
  }
}

std::filesystem::path EditorMainWindow::GetAssetsRootPath() const {
  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  auto root = registry ? registry->GetRootPath() : std::filesystem::path();
  if (root.empty()) {
    root = std::filesystem::path("assets");
  }
  return root;
}

std::filesystem::path EditorMainWindow::GetDefaultScenePath() const {
  return GetAssetsRootPath() / "scenes" / "bootstrap_scene.json";
}

void EditorMainWindow::UpdateRuntimeControlStates() {
  if (m_playAction) {
    const bool showStop = m_isPlaying && !m_isPaused;
    m_playAction->blockSignals(true);
    m_playAction->setChecked(showStop);
    m_playAction->setText(showStop ? tr("Stop") : tr("Play"));
    m_playAction->blockSignals(false);
  }

  if (m_pauseAction) {
    m_pauseAction->blockSignals(true);
    m_pauseAction->setEnabled(m_isPlaying);
    m_pauseAction->setChecked(m_isPaused);
    m_pauseAction->setText(m_isPaused ? tr("Resume") : tr("Pause"));
    m_pauseAction->blockSignals(false);
  }

  if (m_stepAction) {
    m_stepAction->setEnabled(m_isPlaying && m_isPaused);
  }

  if (m_resetAction) {
    m_resetAction->setEnabled(m_isPlaying && m_playSessionSnapshotValid);
  }
}

void EditorMainWindow::StartOrStopPlaySession() {
  if (m_isPlaying) {
    m_isPlaying = false;
    m_isPaused = false;
    if (m_runtimeApp) {
      m_runtimeApp->SetSimulationPlaying(false);
    }
    ClearPlaySessionSnapshot();
    AppendConsole(m_console, tr("Stopped play session"), ConsoleSeverity::Info);
    statusBar()->showMessage(tr("Stopped play session"), 2000);
    UpdateRuntimeControlStates();
    return;
  }

  CapturePlaySessionSnapshot();
  m_isPlaying = true;
  m_isPaused = false;
  if (m_runtimeApp) {
    m_runtimeApp->SetSimulationPlaying(true);
    m_runtimeApp->SetSimulationPaused(false);
  }
  AppendConsole(m_console, tr("Started play session"), ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Play session started"), 2000);
  UpdateRuntimeControlStates();
}

void EditorMainWindow::TogglePauseSession() {
  if (!m_isPlaying) {
    statusBar()->showMessage(tr("No active play session"), 2000);
    return;
  }

  m_isPaused = !m_isPaused;
  if (m_runtimeApp) {
    m_runtimeApp->SetSimulationPaused(m_isPaused);
  }
  AppendConsole(m_console,
                m_isPaused ? tr("Paused session") : tr("Resumed session"),
                ConsoleSeverity::Info);
  statusBar()->showMessage(
      m_isPaused ? tr("Session paused") : tr("Session resumed"), 2000);
  UpdateRuntimeControlStates();
}

void EditorMainWindow::StepSimulationOnce() {
  if (!m_isPlaying || !m_isPaused) {
    statusBar()->showMessage(tr("Step is available while paused"), 2500);
    return;
  }

  if (m_runtimeApp) {
    m_runtimeApp->StepSimulationOnce();
  }
  AppendConsole(m_console, tr("Stepped simulation once"),
                ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Stepped simulation"), 2000);
}

void EditorMainWindow::ResetPlaySession() {
  if (!m_isPlaying) {
    statusBar()->showMessage(tr("No active play session"), 2000);
    return;
  }

  if (!m_scene || !m_playSessionSnapshotValid) {
    statusBar()->showMessage(tr("No reset snapshot available"), 2000);
    return;
  }

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto physicsWorld = ctx ? ctx->GetPhysicsSystem() : nullptr;

  const auto &entities = m_scene->GetEntities();
  for (const auto &entity : entities) {
    if (!entity) {
      continue;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform) {
      continue;
    }

    const auto it = m_playSessionSnapshot.find(entity->GetId());
    if (it == m_playSessionSnapshot.end()) {
      continue;
    }

    const TransformData &data = it->second;
    transform->SetPosition(data.position[0], data.position[1],
                           data.position[2]);
    transform->SetRotationDegrees(data.rotation[0], data.rotation[1],
                                  data.rotation[2]);
    transform->SetScale(data.scale[0], data.scale[1], data.scale[2]);

    if (physicsWorld) {
      auto rigidbody = entity->GetComponent<Scene::RigidbodyComponent>();
      if (rigidbody) {
        const auto handle = rigidbody->GetBodyHandle();
        if (handle.IsValid()) {
          Physics::BodyTransform bodyTransform{};
          bodyTransform.position = data.position;
          bodyTransform.rotation = EulerDegreesToQuaternion(data.rotation);
          physicsWorld->SetBodyTransform(handle, bodyTransform);

          if (rigidbody->GetMotionType() ==
              Scene::RigidbodyComponent::MotionType::Dynamic) {
            physicsWorld->SetLinearVelocity(handle, {0.0f, 0.0f, 0.0f});
            physicsWorld->SetAngularVelocity(handle, {0.0f, 0.0f, 0.0f});
          }
        }
      }
    }
  }

  RefreshSelectedEntityUi();
  AppendConsole(m_console, tr("Reset play session"), ConsoleSeverity::Info);
  statusBar()->showMessage(tr("Scene reset to play start"), 2000);
}

void EditorMainWindow::CapturePlaySessionSnapshot() {
  m_playSessionSnapshot.clear();
  m_playSessionSnapshotValid = false;

  if (!m_scene) {
    return;
  }

  const auto &entities = m_scene->GetEntities();
  for (const auto &entity : entities) {
    if (!entity) {
      continue;
    }

    auto transform = entity->GetComponent<Scene::TransformComponent>();
    if (!transform) {
      continue;
    }

    TransformData data{};
    data.position = transform->GetPosition();
    data.rotation = transform->GetRotationDegrees();
    data.scale = transform->GetScale();
    m_playSessionSnapshot[entity->GetId()] = data;
  }

  m_playSessionSnapshotValid = !m_playSessionSnapshot.empty();
  UpdateRuntimeControlStates();
}

void EditorMainWindow::ClearPlaySessionSnapshot() {
  m_playSessionSnapshot.clear();
  m_playSessionSnapshotValid = false;
  UpdateRuntimeControlStates();
}

void EditorMainWindow::ActivateModeTab(int index) {
  if (index < 0 || index > 2) {
    return;
  }

  // Reflect checked state on actions
  if (m_modeEditAction && m_modePlaytestAction && m_modeUILayoutAction) {
    m_modeEditAction->blockSignals(true);
    m_modePlaytestAction->blockSignals(true);
    m_modeUILayoutAction->blockSignals(true);
    m_modeEditAction->setChecked(index == 0);
    m_modePlaytestAction->setChecked(index == 1);
    m_modeUILayoutAction->setChecked(index == 2);
    m_modeEditAction->blockSignals(false);
    m_modePlaytestAction->blockSignals(false);
    m_modeUILayoutAction->blockSignals(false);
  }

  const QString label = (index == 0)   ? tr("Edit")
                        : (index == 1) ? tr("Playtest")
                                       : tr("UI Layout");
  QString detail;
  if (label == tr("Edit")) {
    detail = tr("Edit workspace (placeholder) ready for scene tools");
  } else if (label == tr("Playtest")) {
    detail = tr("Future in-editor runtime preview will appear here");
  } else {
    detail = tr("UI layout customization placeholder");
  }

  AppendConsole(m_console, tr("Switched to '%1' tab: %2").arg(label, detail),
                ConsoleSeverity::Info);
  statusBar()->showMessage(detail, 2500);
}

bool EditorMainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_viewport ||
      (m_viewport && watched == m_viewport->surfaceWidget())) {
    if (event->type() == QEvent::MouseButtonPress) {
      QMouseEvent *me = static_cast<QMouseEvent *>(event);
      const QPoint pos = me->position().toPoint();
      if (me->button() == Qt::LeftButton) {
        m_dragStartMouseX = pos.x();
        m_dragStartMouseY = pos.y();
        m_activeGizmoAxis = GizmoAxis::None;
        m_requestPickOnRelease = false;

        if (m_selection && m_selection->GetSelectedEntity() &&
            m_gizmoMode == GizmoMode::Translate) {
          auto entity = m_selection->GetSelectedEntity();
          auto transform = entity->GetComponent<Scene::TransformComponent>();
          if (transform) {
            Vec3 origin = {transform->GetPositionX(), transform->GetPositionY(),
                           transform->GetPositionZ()};
            if (m_scene) {
              const auto world = GetWorldMatrix(*m_scene, entity->GetId());
              origin = {world[12], world[13], world[14]};
            }
            Vec3 rayOrigin = GetCameraEye(m_viewport);
            Vec3 rayDir = GetCameraRayDir(m_viewport, pos.x(), pos.y(),
                                          m_viewport->width(),
                                          m_viewport->height());

            const float axisLen = 2.0f;
            // Heuristic threshold for picking
            float minDist = 0.25f * m_viewport->getCameraZoom();

            float tX = ClosestPointLineLine(origin, {1.0f, 0.0f, 0.0f},
                                            rayOrigin, rayDir);
            float dX =
                DistLineLine(rayOrigin, rayDir, origin, {1.0f, 0.0f, 0.0f});
            if (dX < minDist && tX > -0.2f &&
                tX < axisLen) // Allow slight back-pick for origin
            {
              minDist = dX;
              m_activeGizmoAxis = GizmoAxis::X;
            }

            float tY = ClosestPointLineLine(origin, {0.0f, 1.0f, 0.0f},
                                            rayOrigin, rayDir);
            float dY =
                DistLineLine(rayOrigin, rayDir, origin, {0.0f, 1.0f, 0.0f});
            if (dY < minDist && tY > -0.2f && tY < axisLen) {
              minDist = dY;
              m_activeGizmoAxis = GizmoAxis::Y;
            }

            float tZ = ClosestPointLineLine(origin, {0.0f, 0.0f, 1.0f},
                                            rayOrigin, rayDir);
            float dZ =
                DistLineLine(rayOrigin, rayDir, origin, {0.0f, 0.0f, 1.0f});
            if (dZ < minDist && tZ > -0.2f && tZ < axisLen) {
              m_activeGizmoAxis = GizmoAxis::Z;
            }

            if (m_activeGizmoAxis != GizmoAxis::None) {
              // Consume event? No, allow viewport to process drag, but we set
              // state.
            }
          }
        }

        if (m_activeGizmoAxis == GizmoAxis::None) {
          m_requestPickOnRelease = true;
        }
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        if (m_requestPickOnRelease && m_vulkanViewport &&
            m_vulkanViewport->IsReady()) {
          QMouseEvent *me = static_cast<QMouseEvent *>(event);
          const QPoint pos = me->position().toPoint();
          const int dx = std::abs(pos.x() - m_dragStartMouseX);
          const int dy = std::abs(pos.y() - m_dragStartMouseY);
          if (dx <= 3 && dy <= 3) {
            QPoint pickPos = me->pos();
            if (m_viewport && m_viewport->surfaceWidget() &&
                watched == m_viewport) {
              pickPos =
                  m_viewport->surfaceWidget()->mapFrom(m_viewport, pickPos);
            }
            m_vulkanViewport->RequestPick(static_cast<uint32_t>(pickPos.x()),
                                          static_cast<uint32_t>(pickPos.y()));
          }
        }
        m_requestPickOnRelease = false;
        m_activeGizmoAxis = GizmoAxis::None;
      }
    } else if (event->type() == QEvent::KeyPress) {
      auto *keyEvent = static_cast<QKeyEvent *>(event);
      if (keyEvent->isAutoRepeat()) {
        return false;
      }

      // If the viewport or its native surface has focus, allow it to handle
      // movement keys (W/A/S/D/Q/E)
      if (m_viewport && (m_viewport->hasFocus() ||
                         (m_viewport->surfaceWidget() &&
                          m_viewport->surfaceWidget()->hasFocus()))) {
        const int k = keyEvent->key();
        if (k == Qt::Key_W || k == Qt::Key_A || k == Qt::Key_S ||
            k == Qt::Key_D || k == Qt::Key_Q || k == Qt::Key_E) {
          // Let the viewport handle these keys for camera movement
          return false;
        }
      }

      const bool snapping =
          m_snapToggleAction ? m_snapToggleAction->isChecked() : false;
      const float moveStep = snapping ? m_snapTranslateStep : 0.05f;
      const float rotateStep = snapping ? m_snapRotateStep : 5.0f;
      const float scaleStep = snapping ? m_snapScaleStep : 0.01f;

      switch (keyEvent->key()) {
      case Qt::Key_Escape:
        if (m_selection) {
          m_selection->Clear();
        }
        if (m_assetBrowser) {
          m_assetBrowser->ClearSelection();
        }
        statusBar()->showMessage(tr("Selection cleared"), 1200);
        return true;
      case Qt::Key_W:
        if (m_gizmoTranslateAction) {
          m_gizmoTranslateAction->trigger();
        }
        return true;
      case Qt::Key_E:
        if (m_gizmoRotateAction) {
          m_gizmoRotateAction->trigger();
        }
        return true;
      case Qt::Key_R:
        if (m_gizmoScaleAction) {
          m_gizmoScaleAction->trigger();
        }
        return true;
      case Qt::Key_Left:
        if (m_gizmoMode == GizmoMode::Translate) {
          ApplyTranslationDelta(-moveStep, 0.0f, 0.0f);
        } else if (m_gizmoMode == GizmoMode::Rotate) {
          ApplyRotationDelta(-rotateStep);
        } else if (m_gizmoMode == GizmoMode::Scale) {
          ApplyScaleDelta(-scaleStep);
        }
        return true;
      case Qt::Key_Right:
        if (m_gizmoMode == GizmoMode::Translate) {
          ApplyTranslationDelta(moveStep, 0.0f, 0.0f);
        } else if (m_gizmoMode == GizmoMode::Rotate) {
          ApplyRotationDelta(rotateStep);
        } else if (m_gizmoMode == GizmoMode::Scale) {
          ApplyScaleDelta(scaleStep);
        }
        return true;
      case Qt::Key_Up:
        if (m_gizmoMode == GizmoMode::Translate) {
          ApplyTranslationDelta(0.0f, moveStep, 0.0f);
        } else if (m_gizmoMode == GizmoMode::Scale) {
          ApplyScaleDelta(scaleStep);
        }
        return true;
      case Qt::Key_Down:
        if (m_gizmoMode == GizmoMode::Translate) {
          ApplyTranslationDelta(0.0f, -moveStep, 0.0f);
        } else if (m_gizmoMode == GizmoMode::Scale) {
          ApplyScaleDelta(-scaleStep);
        }
        return true;
      default:
        break;
      }
    }
  }

  return QMainWindow::eventFilter(watched, event);
}

void EditorMainWindow::ApplyTranslationDelta(float dx, float dy, float dz) {
  if (!m_selection)
    return;
  auto entity = m_selection->GetSelectedEntity();
  if (!entity)
    return;
  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform)
    return;

  TransformData oldData;
  oldData.position = {transform->GetPositionX(), transform->GetPositionY(),
                      transform->GetPositionZ()};
  oldData.rotation = {transform->GetRotationXDegrees(),
                      transform->GetRotationYDegrees(),
                      transform->GetRotationZDegrees()};
  oldData.scale = {transform->GetScaleX(), transform->GetScaleY(),
                   transform->GetScaleZ()};

  TransformData newData = oldData;
  newData.position[0] += dx;
  newData.position[1] += dy;
  newData.position[2] += dz;

  ExecuteCommand(std::make_unique<TransformCommand>(entity, oldData, newData));
}

void EditorMainWindow::ApplyRotationDelta(float deltaDeg) {
  if (!m_selection)
    return;
  auto entity = m_selection->GetSelectedEntity();
  if (!entity)
    return;
  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform)
    return;

  TransformData oldData;
  oldData.position = {transform->GetPositionX(), transform->GetPositionY(),
                      transform->GetPositionZ()};
  oldData.rotation = {transform->GetRotationXDegrees(),
                      transform->GetRotationYDegrees(),
                      transform->GetRotationZDegrees()};
  oldData.scale = {transform->GetScaleX(), transform->GetScaleY(),
                   transform->GetScaleZ()};

  TransformData newData = oldData;
  newData.rotation[2] += deltaDeg; // Z-axis rotation as per original code

  ExecuteCommand(std::make_unique<TransformCommand>(entity, oldData, newData));
}

void EditorMainWindow::ApplyScaleDelta(float deltaUniform) {
  if (!m_selection)
    return;
  auto entity = m_selection->GetSelectedEntity();
  if (!entity)
    return;
  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform)
    return;

  TransformData oldData;
  oldData.position = {transform->GetPositionX(), transform->GetPositionY(),
                      transform->GetPositionZ()};
  oldData.rotation = {transform->GetRotationXDegrees(),
                      transform->GetRotationYDegrees(),
                      transform->GetRotationZDegrees()};
  oldData.scale = {transform->GetScaleX(), transform->GetScaleY(),
                   transform->GetScaleZ()};

  TransformData newData = oldData;
  newData.scale[0] = std::max(0.001f, newData.scale[0] + deltaUniform);
  newData.scale[1] = std::max(0.001f, newData.scale[1] + deltaUniform);
  newData.scale[2] = std::max(0.001f, newData.scale[2] + deltaUniform);

  ExecuteCommand(std::make_unique<TransformCommand>(entity, oldData, newData));
}

void EditorMainWindow::BeginInteractiveTransform() {
  if (!m_selection)
    return;
  auto entity = m_selection->GetSelectedEntity();
  if (!entity)
    return;

  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform)
    return;

  m_interactiveEntity = entity;
  m_interactiveTransformActive = true;

  m_interactiveOldData.position = {transform->GetPositionX(),
                                   transform->GetPositionY(),
                                   transform->GetPositionZ()};
  m_interactiveOldData.rotation = {transform->GetRotationXDegrees(),
                                   transform->GetRotationYDegrees(),
                                   transform->GetRotationZDegrees()};
  m_interactiveOldData.scale = {transform->GetScaleX(), transform->GetScaleY(),
                                transform->GetScaleZ()};

  m_interactiveCurrentData = m_interactiveOldData;
  m_interactiveTargetData = m_interactiveOldData;
}

void EditorMainWindow::UpdateInteractiveTransformTarget(float dx, float dy,
                                                        float dz) {
  if (!m_interactiveTransformActive || !m_interactiveEntity)
    return;

  m_interactiveTargetData.position[0] += dx;
  m_interactiveTargetData.position[1] += dy;
  m_interactiveTargetData.position[2] += dz;
}

void EditorMainWindow::EndInteractiveTransform() {
  if (!m_interactiveTransformActive || !m_interactiveEntity)
    return;

  // Ensure final target applied before creating command
  auto entity = m_interactiveEntity;
  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (transform) {
    transform->SetPosition(m_interactiveTargetData.position[0],
                           m_interactiveTargetData.position[1],
                           m_interactiveTargetData.position[2]);
    transform->SetRotationDegrees(m_interactiveTargetData.rotation[0],
                                  m_interactiveTargetData.rotation[1],
                                  m_interactiveTargetData.rotation[2]);
    transform->SetScale(m_interactiveTargetData.scale[0],
                        m_interactiveTargetData.scale[1],
                        m_interactiveTargetData.scale[2]);
  }

  // Push a single command representing the full change
  ExecuteCommand(std::make_unique<TransformCommand>(
      entity, m_interactiveOldData, m_interactiveTargetData));

  m_interactiveTransformActive = false;
  m_interactiveEntity.reset();
}

void EditorMainWindow::UpdateInteractiveTransform(float deltaTime) {
  if (!m_interactiveTransformActive || !m_interactiveEntity)
    return;

  auto entity = m_interactiveEntity;
  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform)
    return;

  // Interpolate current towards target for smoothing
  const float rate = 15.0f; // higher = faster follow
  const float alpha = 1.0f - std::exp(-rate * deltaTime);

  for (int i = 0; i < 3; ++i) {
    m_interactiveCurrentData.position[i] =
        m_interactiveCurrentData.position[i] +
        (m_interactiveTargetData.position[i] -
         m_interactiveCurrentData.position[i]) *
            alpha;
    m_interactiveCurrentData.rotation[i] =
        m_interactiveCurrentData.rotation[i] +
        (m_interactiveTargetData.rotation[i] -
         m_interactiveCurrentData.rotation[i]) *
            alpha;
    m_interactiveCurrentData.scale[i] =
        m_interactiveCurrentData.scale[i] +
        (m_interactiveTargetData.scale[i] - m_interactiveCurrentData.scale[i]) *
            alpha;
  }

  transform->SetPosition(m_interactiveCurrentData.position[0],
                         m_interactiveCurrentData.position[1],
                         m_interactiveCurrentData.position[2]);
  transform->SetRotationDegrees(m_interactiveCurrentData.rotation[0],
                                m_interactiveCurrentData.rotation[1],
                                m_interactiveCurrentData.rotation[2]);
  transform->SetScale(m_interactiveCurrentData.scale[0],
                      m_interactiveCurrentData.scale[1],
                      m_interactiveCurrentData.scale[2]);
}

void EditorMainWindow::RefreshSelectedEntityUi() {
  if (m_inspectorPanel) {
    m_inspectorPanel->SetSelectedEntity(
        m_selection ? m_selection->GetSelectedEntity() : nullptr);
  }
  if (m_hierarchyPanel && m_selection) {
    auto entity = m_selection->GetSelectedEntity();
    if (entity) {
      m_hierarchyPanel->SetSelectedEntity(entity->GetId());
    }
  }
  if (m_cameraPreview) {
    auto entity = m_selection ? m_selection->GetSelectedEntity() : nullptr;
    if (entity && entity->GetComponent<Scene::CameraComponent>()) {
      m_cameraPreview->SetSelectedCameraId(entity->GetId());
    } else {
      m_cameraPreview->SetSelectedCameraId(0);
    }
  }
}

void EditorMainWindow::FocusCameraOnSelection() {
  if (!m_selection) {
    statusBar()->showMessage(tr("No entity selected"), 2000);
    return;
  }

  auto entity = m_selection->GetSelectedEntity();
  if (!entity) {
    statusBar()->showMessage(tr("No entity selected"), 2000);
    return;
  }

  auto transform = entity->GetComponent<Scene::TransformComponent>();
  if (!transform) {
    statusBar()->showMessage(tr("Selected entity has no transform"), 2000);
    return;
  }

  float targetX = transform->GetPositionX();
  float targetY = transform->GetPositionY();
  float targetZ = transform->GetPositionZ();
  float radius = 0.5f;

  auto ctx = m_runtimeApp ? m_runtimeApp->GetContext() : nullptr;
  auto registry = ctx ? ctx->GetAssetRegistry() : nullptr;
  auto mesh = entity->GetComponent<Scene::MeshRendererComponent>();
  if (mesh && registry && !mesh->GetMeshAssetId().empty() && m_scene) {
    if (const auto *meshData = registry->LoadMeshData(mesh->GetMeshAssetId())) {
      const auto world = GetWorldMatrix(*m_scene, entity->GetId());
      const auto worldCenter = TransformPoint(world, meshData->boundsCenter);
      const float maxScale = ExtractMaxScale(world);
      targetX = worldCenter[0];
      targetY = worldCenter[1];
      targetZ = worldCenter[2];
      radius = std::max(meshData->boundsRadius * maxScale, 0.01f);
    }
  }

  if (m_viewport) {
    m_viewport->SetCameraTarget(targetX, targetY, targetZ);
  }
  if (m_vulkanViewport) {
    m_vulkanViewport->FocusOnBounds(targetX, targetY, targetZ, radius);
  }

  statusBar()->showMessage(
      tr("Focused on '%1'").arg(QString::fromStdString(entity->GetName())),
      2000);
}

void EditorMainWindow::ExecuteCommand(std::unique_ptr<Command> cmd) {
  if (!m_commandHistory || !cmd)
    return;

  m_commandHistory->Push(std::move(cmd));
  SetSceneDirty(true);
  UpdateUndoRedoState();
  RefreshSelectedEntityUi();
}

void EditorMainWindow::Undo() {
  if (m_commandHistory && m_commandHistory->CanUndo()) {
    m_commandHistory->Undo();
    SetSceneDirty(true);
    UpdateUndoRedoState();
    RefreshSelectedEntityUi();
  }
}

void EditorMainWindow::Redo() {
  if (m_commandHistory && m_commandHistory->CanRedo()) {
    m_commandHistory->Redo();
    SetSceneDirty(true);
    UpdateUndoRedoState();
    RefreshSelectedEntityUi();
  }
}

void EditorMainWindow::UpdateUndoRedoState() {
  if (m_undoAction)
    m_undoAction->setEnabled(m_commandHistory && m_commandHistory->CanUndo());
  if (m_redoAction)
    m_redoAction->setEnabled(m_commandHistory && m_commandHistory->CanRedo());
}

} // namespace Aetherion::Editor
