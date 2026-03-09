#include "Aetherion/Editor/EditorInspectorPanel.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Editor/Commands/ComponentCommands.h"
#include "Aetherion/Editor/Commands/TransformCommand.h"
#include "Aetherion/Runtime/EngineContext.h"
#include "Aetherion/Scene/AIBehaviorComponent.h"
#include "Aetherion/Scene/AudioSourceComponent.h"
#include "Aetherion/Scene/AnimatorComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/ParticleEmitterComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/ScriptComponent.h"
#include "Aetherion/Scene/TransformComponent.h"
#include "Aetherion/Scripting/ScriptingPlaceholder.h"
#include <QToolButton>

namespace {
// Helper class for collapsible headers
class CollapsibleHeader : public QWidget {
public:
  CollapsibleHeader(const QString &title, QWidget *content,
                    QWidget *parent = nullptr)
      : QWidget(parent), m_content(content) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *headerBtn = new QPushButton(title, this);
    headerBtn->setCheckable(true);
    headerBtn->setChecked(true);
    headerBtn->setStyleSheet(
        "QPushButton { text-align: left; font-weight: 600; padding: 6px 8px; "
        "background-color: #1b2230; border: 1px solid #2a3140; "
        "border-radius: 4px; color: #e6e3dc; }"
        "QPushButton:checked { background-color: #2a3344; "
        "border-color: #3a465b; }");

    layout->addWidget(headerBtn);
    layout->addWidget(m_content);

    connect(headerBtn, &QPushButton::toggled, this,
            [this](bool checked) { m_content->setVisible(checked); });
  }

private:
  QWidget *m_content;
};

QString FormatBytes(long long bytes) {
  if (bytes < 0) {
    return {};
  }
  static const char *units[] = {"B", "KB", "MB", "GB"};
  double value = static_cast<double>(bytes);
  int unitIndex = 0;
  while (value >= 1024.0 && unitIndex < 3) {
    value /= 1024.0;
    ++unitIndex;
  }
  return QString::number(value, 'f', unitIndex == 0 ? 0 : 1) + " " +
         units[unitIndex];
}

QString FormatVec3(const std::array<float, 3> &value, int precision = 3) {
  return QString("%1, %2, %3")
      .arg(QString::number(value[0], 'f', precision))
      .arg(QString::number(value[1], 'f', precision))
      .arg(QString::number(value[2], 'f', precision));
}

QString FormatIdList(const std::vector<std::string> &ids, int maxItems = 6) {
  QStringList lines;
  const int count = static_cast<int>(ids.size());
  const int limit = std::min(count, maxItems);
  for (int i = 0; i < limit; ++i) {
    lines << QString::fromStdString(ids[static_cast<size_t>(i)]);
  }
  if (count > maxItems) {
    lines << QString("... (+%1 more)").arg(count - maxItems);
  }
  return lines.join("\n");
}

long long
EstimateMeshBytes(const Aetherion::Assets::AssetRegistry::MeshData &meshData) {
  const size_t vertexCount = meshData.positions.size();
  const size_t indexCount = meshData.indices.size();
  const size_t vertexStride = (3 + 3 + 4 + 2 + 4) * sizeof(float);
  const size_t vertexBytes = vertexCount * vertexStride;
  const size_t indexBytes = indexCount * sizeof(std::uint32_t);
  return static_cast<long long>(vertexBytes + indexBytes);
}

QString AssetTypeLabel(Aetherion::Assets::AssetRegistry::AssetType type) {
  using AssetType = Aetherion::Assets::AssetRegistry::AssetType;
  switch (type) {
  case AssetType::Texture:
    return "Texture";
  case AssetType::Mesh:
    return "Mesh";
  case AssetType::Audio:
    return "Audio";
  case AssetType::Script:
    return "Script";
  case AssetType::BehaviorPrompt:
    return "Behavior Prompt";
  case AssetType::Scene:
    return "Scene";
  case AssetType::Shader:
    return "Shader";
  case AssetType::Animation:
    return "Animation";
  case AssetType::Skeleton:
    return "Skeleton";
  default:
    return "Other";
  }
}
} // namespace

namespace Aetherion::Editor {
EditorInspectorPanel::EditorInspectorPanel(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);

  auto *header = new QLabel(tr("Inspector"), this);

  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_content = new QWidget(m_scrollArea);
  m_contentLayout = new QVBoxLayout(m_content);
  m_content->setLayout(m_contentLayout);
  m_scrollArea->setWidget(m_content);

  layout->addWidget(header);
  layout->addWidget(m_scrollArea, 1);
  setLayout(layout);

  RebuildUi();
}

void EditorInspectorPanel::SetSelectedEntity(
    std::shared_ptr<Scene::Entity> entity) {
  m_entity = std::move(entity);
  m_showingAsset = false;
  m_assetId.clear();
  RebuildUi();
}

void EditorInspectorPanel::UpdateAIStatus() {
  if (!m_entity) {
    return;
  }
  auto aiBehavior = m_entity->GetComponent<Scene::AIBehaviorComponent>();
  if (!aiBehavior) {
    return;
  }

  if (m_aiStateLabel) {
    m_aiStateLabel->setText(
        tr("State: %1")
            .arg(QString::fromStdString(aiBehavior->GetCurrentState())));
  }
  if (m_aiReasonLabel) {
    m_aiReasonLabel->setText(
        tr("Reason: %1")
            .arg(QString::fromStdString(aiBehavior->GetLastReason())));
  }
  if (m_aiInferenceLabel) {
    m_aiInferenceLabel->setText(
        tr("Inference: %1")
            .arg(QString::fromStdString(aiBehavior->GetLastInferenceSource())));
  }
  if (m_aiLatencyLabel) {
    m_aiLatencyLabel->setText(
        tr("Latency: %1 ms")
            .arg(static_cast<qulonglong>(
                aiBehavior->GetLastInferenceLatencyMs())));
  }
  if (m_aiBudgetLabel) {
    m_aiBudgetLabel->setText(
        tr("Budget Remaining: %1")
            .arg(aiBehavior->GetLastBudgetRemaining()));
  }
  UpdateAIScriptStatus();
}

void EditorInspectorPanel::UpdateAIScriptStatus() {
  if (!m_aiScriptPathLabel || !m_aiScriptDiagLabel || !m_entity) {
    return;
  }
  auto aiBehavior = m_entity->GetComponent<Scene::AIBehaviorComponent>();
  if (!aiBehavior) {
    return;
  }

  QString pathText = tr("Generated Script: <none>");
  QString diagText = tr("Diagnostics: <none>");

  auto *scene = m_entity->GetScene();
  auto *context = scene ? scene->GetContext() : nullptr;
  auto scripting = context ? context->GetScriptingRuntime() : nullptr;

  if (!scripting) {
    diagText = tr("Diagnostics: Scripting runtime unavailable.");
  } else {
    std::string scriptId = aiBehavior->GetPromptAssetId();
    if (scriptId.empty() && !aiBehavior->GetInlinePrompt().empty()) {
      scriptId = "inline_behavior";
    }
    if (scriptId.empty()) {
      diagText = tr("Diagnostics: No prompt set.");
    } else if (auto script = scripting->GetScript(scriptId)) {
      if (!script->generatedPath.empty()) {
        pathText = tr("Generated Script: %1")
                       .arg(QString::fromStdString(
                           script->generatedPath.string()));
      } else {
        pathText = tr("Generated Script: <in-memory>");
      }
      if (!script->diagnostics.empty()) {
        diagText = tr("Diagnostics: %1")
                       .arg(QString::fromStdString(script->diagnostics));
      } else {
        diagText = tr("Diagnostics: None");
      }
    } else {
      diagText = tr("Diagnostics: Script not generated yet.");
    }
  }

  m_aiScriptPathLabel->setText(pathText);
  m_aiScriptDiagLabel->setText(diagText);
}

void EditorInspectorPanel::SetSelectedAsset(QString assetId) {
  m_entity.reset();
  m_showingAsset = true;
  m_assetId = std::move(assetId);
  RebuildUi();
}

void EditorInspectorPanel::SetAssetRegistry(
    std::shared_ptr<Assets::AssetRegistry> registry) {
  m_assetRegistry = std::move(registry);
  if (m_showingAsset || m_entity) {
    RebuildUi();
  }
}

void EditorInspectorPanel::RebuildUi() {
  if (!m_contentLayout) {
    return;
  }

  m_buildingUi = true;

  while (auto *item = m_contentLayout->takeAt(0)) {
    if (auto *w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }

  m_posX = nullptr;
  m_posY = nullptr;
  m_rotZ = nullptr;
  m_scaleX = nullptr;
  m_scaleY = nullptr;
  m_colorR = nullptr;
  m_colorG = nullptr;
  m_colorB = nullptr;
  m_meshRotationSpeed = nullptr;
  m_meshAsset = nullptr;
  m_meshTexture = nullptr;
  m_meshVisible = nullptr;
  m_lightEnabled = nullptr;
  m_lightColorR = nullptr;
  m_lightColorG = nullptr;
  m_lightColorB = nullptr;
  m_lightIntensity = nullptr;
  m_lightAmbientR = nullptr;
  m_lightAmbientG = nullptr;
  m_lightAmbientB = nullptr;
  m_lightPrimary = nullptr;

  m_audioPath = nullptr;
  m_audioVolume = nullptr;
  m_audioPitch = nullptr;
  m_audioLoop = nullptr;
  m_audioSpatial = nullptr;
  m_audioPlayOnAwake = nullptr;
  m_aiMode = nullptr;
  m_aiPromptAsset = nullptr;
  m_aiPersonality = nullptr;
  m_aiKnowledge = nullptr;
  m_aiContext = nullptr;
  m_aiInlinePrompt = nullptr;
  m_aiDecisionInterval = nullptr;
  m_aiStateLabel = nullptr;
  m_aiReasonLabel = nullptr;
  m_aiInferenceLabel = nullptr;
  m_aiLatencyLabel = nullptr;
  m_aiBudgetLabel = nullptr;
  m_aiScriptPathLabel = nullptr;
  m_aiScriptDiagLabel = nullptr;

  auto addMeshStatsRows = [this](
                              QFormLayout *form, QWidget *parent,
                              const Assets::AssetRegistry::MeshData &meshData) {
    const long long vertexCount =
        static_cast<long long>(meshData.positions.size());
    const long long indexCount =
        static_cast<long long>(meshData.indices.size());
    const long long triangleCount = indexCount / 3;

    form->addRow(tr("Vertices"),
                 new QLabel(QString::number(vertexCount), parent));
    form->addRow(tr("Indices"),
                 new QLabel(QString::number(indexCount), parent));
    form->addRow(tr("Triangles"),
                 new QLabel(QString::number(triangleCount), parent));
    form->addRow(tr("Bounds Min"),
                 new QLabel(FormatVec3(meshData.boundsMin), parent));
    form->addRow(tr("Bounds Max"),
                 new QLabel(FormatVec3(meshData.boundsMax), parent));
    form->addRow(tr("Bounds Center"),
                 new QLabel(FormatVec3(meshData.boundsCenter), parent));
    form->addRow(
        tr("Bounds Radius"),
        new QLabel(QString::number(meshData.boundsRadius, 'f', 3), parent));
    form->addRow(tr("CPU Memory (approx.)"),
                 new QLabel(FormatBytes(EstimateMeshBytes(meshData)), parent));
  };

  if (!m_entity) {
    if (m_showingAsset) {
      const QString assetIdText =
          m_assetId.trimmed().isEmpty() ? tr("Asset") : m_assetId.trimmed();

      const auto registry = m_assetRegistry;
      const Assets::AssetRegistry::AssetEntry *entry = nullptr;
      if (registry && !assetIdText.endsWith('/')) {
        entry = registry->FindEntry(assetIdText.toStdString());
      }

      QString displayName = assetIdText;
      if (entry) {
        displayName = QString::fromStdString(entry->path.filename().string());
      }

      auto *title = new QLabel(displayName, m_content);
      title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
      m_contentLayout->addWidget(title);

      auto *formHost = new QWidget(m_content);
      auto *form = new QFormLayout(formHost);
      form->setLabelAlignment(Qt::AlignLeft);

      const bool isFolder = assetIdText.endsWith('/');
      const QString type = isFolder ? tr("Folder") : tr("Asset");
      form->addRow(tr("Type"), new QLabel(type, formHost));

      const QString normalized =
          isFolder ? assetIdText.left(assetIdText.size() - 1) : assetIdText;
      form->addRow(tr("Id"), new QLabel(normalized, formHost));

      QLabel *previewLabel = nullptr;
      if (!isFolder && registry && !entry) {
        const std::string id = normalized.toStdString();
        entry = registry->FindEntry(id);
      }

      if (isFolder) {
        form->addRow(tr("Status"), new QLabel(tr("Category"), formHost));
      } else if (entry) {
        const QString pathLabel =
            QString::fromStdString(entry->path.generic_string());
        form->addRow(tr("Category"),
                     new QLabel(AssetTypeLabel(entry->type), formHost));
        form->addRow(tr("Path"), new QLabel(pathLabel, formHost));

        QFileInfo fileInfo(QString::fromStdString(entry->path.string()));
        if (fileInfo.exists()) {
          form->addRow(tr("Size"),
                       new QLabel(FormatBytes(fileInfo.size()), formHost));
          form->addRow(tr("Modified"),
                       new QLabel(fileInfo.lastModified().toString(Qt::ISODate),
                                  formHost));
        }
        form->addRow(tr("Status"), new QLabel(tr("Registered"), formHost));

        if (entry->type == Assets::AssetRegistry::AssetType::Texture &&
            fileInfo.exists()) {
          QImageReader reader(fileInfo.absoluteFilePath());
          reader.setAutoTransform(true);
          const QSize imageSize = reader.size();
          if (imageSize.isValid()) {
            form->addRow(tr("Dimensions"),
                         new QLabel(tr("%1 x %2")
                                        .arg(imageSize.width())
                                        .arg(imageSize.height()),
                                    formHost));
          }

          const QImage image = reader.read();
          if (!image.isNull()) {
            const int previewMax = 256;
            const QPixmap preview = QPixmap::fromImage(image).scaled(
                previewMax, previewMax, Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            previewLabel = new QLabel(m_content);
            previewLabel->setPixmap(preview);
            previewLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
          }
        }

        if (registry && entry->type == Assets::AssetRegistry::AssetType::Mesh) {
          const auto *meshData = registry->LoadMeshData(entry->id);
          if (meshData) {
            form->addRow(tr("Geometry"), new QLabel(tr("Loaded"), formHost));
            addMeshStatsRows(form, formHost, *meshData);
          } else {
            form->addRow(tr("Geometry"),
                         new QLabel(tr("Not loaded"), formHost));
          }

          if (const auto *cachedMesh = registry->GetMesh(entry->id)) {
            form->addRow(tr("Materials"),
                         new QLabel(QString::number(static_cast<long long>(
                                        cachedMesh->materialIds.size())),
                                    formHost));
            if (!cachedMesh->materialIds.empty()) {
              auto *materialLabel =
                  new QLabel(FormatIdList(cachedMesh->materialIds), formHost);
              materialLabel->setWordWrap(true);
              form->addRow(tr("Material IDs"), materialLabel);
            }

            form->addRow(tr("Textures"),
                         new QLabel(QString::number(static_cast<long long>(
                                        cachedMesh->textureIds.size())),
                                    formHost));
            if (!cachedMesh->textureIds.empty()) {
              auto *textureLabel =
                  new QLabel(FormatIdList(cachedMesh->textureIds), formHost);
              textureLabel->setWordWrap(true);
              form->addRow(tr("Texture IDs"), textureLabel);
            }
          } else {
            form->addRow(tr("Materials"), new QLabel(tr("N/A"), formHost));
            form->addRow(tr("Textures"), new QLabel(tr("N/A"), formHost));
          }

          const std::string meshId = entry->id;
          const auto importSettings = registry->GetMeshImportSettings(meshId);

          auto *importHeader = new QLabel(tr("Import Settings"), formHost);
          QFont importFont = importHeader->font();
          importFont.setBold(true);
          importHeader->setFont(importFont);
          form->addRow(importHeader);

          auto *importScale = new QDoubleSpinBox(formHost);
          importScale->setRange(0.001, 1000.0);
          importScale->setDecimals(3);
          importScale->setSingleStep(0.1);
          importScale->setValue(importSettings.scale);

          auto *importCenter = new QCheckBox(formHost);
          importCenter->setChecked(importSettings.centerMesh);

          auto *importNormals = new QCheckBox(formHost);
          importNormals->setChecked(importSettings.generateNormals);
          auto *importTangents = new QCheckBox(formHost);
          importTangents->setChecked(importSettings.generateTangents);
          auto *importFlipUvs = new QCheckBox(formHost);
          importFlipUvs->setChecked(importSettings.flipUVs);
          auto *importFlipWinding = new QCheckBox(formHost);
          importFlipWinding->setChecked(importSettings.flipWinding);
          auto *importOptimize = new QCheckBox(formHost);
          importOptimize->setChecked(importSettings.optimize);

          importCenter->setToolTip(tr("Recenters the mesh to its bounds"));
          importOptimize->setToolTip(tr("Compacts unused vertices"));
          importFlipWinding->setToolTip(
              tr("Reverses triangle winding for backface culling"));

          form->addRow(tr("Scale"), importScale);
          form->addRow(tr("Center Mesh"), importCenter);
          form->addRow(tr("Generate Normals"), importNormals);
          form->addRow(tr("Generate Tangents"), importTangents);
          form->addRow(tr("Flip UVs"), importFlipUvs);
          form->addRow(tr("Flip Winding"), importFlipWinding);
          form->addRow(tr("Optimize"), importOptimize);

          auto applyImportSettings = [this, meshId, importScale, importNormals,
                                      importTangents, importFlipUvs,
                                      importFlipWinding, importOptimize,
                                      importCenter]() {
            if (!m_assetRegistry) {
              return;
            }

            Assets::AssetRegistry::MeshImportSettings settings =
                m_assetRegistry->GetMeshImportSettings(meshId);
            settings.scale = static_cast<float>(importScale->value());
            settings.centerMesh = importCenter->isChecked();
            settings.generateNormals = importNormals->isChecked();
            settings.generateTangents = importTangents->isChecked();
            settings.flipUVs = importFlipUvs->isChecked();
            settings.flipWinding = importFlipWinding->isChecked();
            settings.optimize = importOptimize->isChecked();

            m_assetRegistry->SetMeshImportSettings(meshId, settings);
          };

          connect(importScale, qOverload<double>(&QDoubleSpinBox::valueChanged),
                  this,
                  [applyImportSettings](double) { applyImportSettings(); });
          connect(importCenter, &QCheckBox::toggled, this,
                  [applyImportSettings](bool) { applyImportSettings(); });
          connect(importNormals, &QCheckBox::toggled, this,
                  [applyImportSettings](bool) { applyImportSettings(); });
          connect(importTangents, &QCheckBox::toggled, this,
                  [applyImportSettings](bool) { applyImportSettings(); });
          connect(importFlipUvs, &QCheckBox::toggled, this,
                  [applyImportSettings](bool) { applyImportSettings(); });
          connect(importFlipWinding, &QCheckBox::toggled, this,
                  [applyImportSettings](bool) { applyImportSettings(); });
          connect(importOptimize, &QCheckBox::toggled, this,
                  [applyImportSettings](bool) { applyImportSettings(); });

          auto *reimportButton = new QPushButton(tr("Reimport"), formHost);
          form->addRow(reimportButton);

          connect(reimportButton, &QPushButton::clicked, this,
                  [this, meshId, applyImportSettings]() {
                    if (!m_assetRegistry) {
                      return;
                    }

                    applyImportSettings();

                    std::string message;
                    const bool success =
                        m_assetRegistry->ReimportMeshAsset(meshId, &message);
                    if (!success) {
                      QMessageBox::warning(this, tr("Mesh Reimport Failed"),
                                           QString::fromStdString(message));
                      return;
                    }

                    RebuildUi();
                  });
        }
      } else {
        const QString status = registry ? tr("Not found in registry")
                                        : tr("Asset registry unavailable");
        form->addRow(tr("Status"), new QLabel(status, formHost));
      }

      formHost->setLayout(form);
      m_contentLayout->addWidget(formHost);
      if (previewLabel) {
        m_contentLayout->addWidget(previewLabel);
      }
      m_contentLayout->addStretch(1);
      m_buildingUi = false;
      return;
    }

    auto *placeholder =
        new QLabel(tr("Select an entity to view details"), m_content);
    placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_contentLayout->addWidget(placeholder);
    m_contentLayout->addStretch(1);
    m_buildingUi = false;
    return;
  }

  auto *title = new QLabel(QString::fromStdString(m_entity->GetName().empty()
                                                      ? std::string("Entity")
                                                      : m_entity->GetName()),
                           m_content);
  title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_contentLayout->addWidget(title);

  auto transform = m_entity->GetComponent<Scene::TransformComponent>();
  auto mesh = m_entity->GetComponent<Scene::MeshRendererComponent>();
  auto light = m_entity->GetComponent<Scene::LightComponent>();
  auto camera = m_entity->GetComponent<Scene::CameraComponent>();
  auto rigidbody = m_entity->GetComponent<Scene::RigidbodyComponent>();
  auto collider = m_entity->GetComponent<Scene::ColliderComponent>();
  auto animator = m_entity->GetComponent<Scene::AnimatorComponent>();
  auto audioSource = m_entity->GetComponent<Scene::AudioSourceComponent>();
  auto aiBehavior = m_entity->GetComponent<Scene::AIBehaviorComponent>();
  auto particleEmitter =
      m_entity->GetComponent<Scene::ParticleEmitterComponent>();
  auto scriptComponent = m_entity->GetComponent<Scene::ScriptComponent>();

  if (!transform && !mesh && !light && !camera && !rigidbody && !collider &&
      !animator && !audioSource && !aiBehavior && !particleEmitter &&
      !scriptComponent) {
    auto *noEditable =
        new QLabel(tr("No editable components on selected entity."), m_content);
    noEditable->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_contentLayout->addWidget(noEditable);
    m_contentLayout->addStretch(1);
    m_buildingUi = false;
    return;
  }

  auto makeSpin = [this](double min, double max, double step) {
    auto *s = new QDoubleSpinBox(m_content);
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(3);
    return s;
  };

  auto makeComponentHeader = [this](const QString &title,
                                    std::shared_ptr<Scene::Component> component,
                                    QWidget *contentWidget) {
    // Header container with title and remove button
    auto *headerContainer = new QWidget();
    auto *headerLayout = new QHBoxLayout(headerContainer);
    headerLayout->setContentsMargins(5, 5, 5, 5);
    headerContainer->setStyleSheet(
        "background-color: #1b2230; border: 1px solid #2a3140; "
        "border-radius: 6px;");

    auto *toggleBtn = new QToolButton(headerContainer);
    toggleBtn->setArrowType(Qt::DownArrow);
    toggleBtn->setStyleSheet("border: none;");
    toggleBtn->setCheckable(true);
    toggleBtn->setChecked(true);

    auto *label = new QLabel(title, headerContainer);
    label->setStyleSheet("font-weight: bold;");

    auto *removeBtn = new QPushButton("X", headerContainer);
    removeBtn->setFixedSize(20, 20);
    removeBtn->setStyleSheet(
        "QPushButton { border-radius: 10px; color: #aeb6c2; "
        "background-color: #1b2230; border: 1px solid #2a3140; }"
        "QPushButton:hover { background-color: #f65b5b; color: #141824; "
        "border-color: #f65b5b; }");
    removeBtn->setToolTip(tr("Remove Component"));

    headerLayout->addWidget(toggleBtn);
    headerLayout->addWidget(label, 1);
    headerLayout->addWidget(removeBtn);

    connect(removeBtn, &QPushButton::clicked, this, [this, component] {
      if (m_commandExecutor) {
        m_commandExecutor(
            std::make_unique<RemoveComponentCommand>(m_entity, component));
      }
    });

    connect(toggleBtn, &QToolButton::toggled, contentWidget,
            [contentWidget, toggleBtn](bool checked) {
              contentWidget->setVisible(checked);
              toggleBtn->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
            });

    auto *container = new QWidget(m_content);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 4);
    containerLayout->setSpacing(2);
    containerLayout->addWidget(headerContainer);
    containerLayout->addWidget(contentWidget);

    return container;
  };

  auto makeVectorRow = [this](const QString &label, QDoubleSpinBox *x,
                              QDoubleSpinBox *y, QDoubleSpinBox *z,
                              float resetVal) {
    auto *container = new QWidget(m_content);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    layout->addWidget(new QLabel("X", container));
    layout->addWidget(x, 1);
    layout->addWidget(new QLabel("Y", container));
    layout->addWidget(y, 1);
    layout->addWidget(new QLabel("Z", container));
    layout->addWidget(z, 1);

    auto *resetBtn = new QToolButton(container);
    resetBtn->setText("R");
    resetBtn->setToolTip(tr("Reset %1").arg(label));
    resetBtn->setFixedSize(20, 20); // Small button
    connect(resetBtn, &QToolButton::clicked, this, [x, y, z, resetVal] {
      x->setValue(resetVal);
      y->setValue(resetVal);
      z->setValue(resetVal);
    });
    layout->addWidget(resetBtn);

    return container;
  };

  if (transform) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    m_posX = makeSpin(-10000.0, 10000.0, 0.1);
    m_posY = makeSpin(-10000.0, 10000.0, 0.1);
    m_posZ = makeSpin(-10000.0, 10000.0, 0.1);
    m_rotX = makeSpin(-360.0, 360.0, 1.0);
    m_rotY = makeSpin(-360.0, 360.0, 1.0);
    m_rotZ = makeSpin(-360.0, 360.0, 1.0);
    m_scaleX = makeSpin(0.001, 10000.0, 0.1);
    m_scaleY = makeSpin(0.001, 10000.0, 0.1);
    m_scaleZ = makeSpin(0.001, 10000.0, 0.1);

    m_posX->setValue(transform->GetPositionX());
    m_posY->setValue(transform->GetPositionY());
    m_posZ->setValue(transform->GetPositionZ());
    m_rotX->setValue(transform->GetRotationXDegrees());
    m_rotY->setValue(transform->GetRotationYDegrees());
    m_rotZ->setValue(transform->GetRotationZDegrees());
    m_scaleX->setValue(transform->GetScaleX());
    m_scaleY->setValue(transform->GetScaleY());
    m_scaleZ->setValue(transform->GetScaleZ());

    form->addRow(tr("Position"),
                 makeVectorRow(tr("Position"), m_posX, m_posY, m_posZ, 0.0f));
    form->addRow(tr("Rotation"),
                 makeVectorRow(tr("Rotation"), m_rotX, m_rotY, m_rotZ, 0.0f));
    form->addRow(tr("Scale"), makeVectorRow(tr("Scale"), m_scaleX, m_scaleY,
                                            m_scaleZ, 1.0f));

    auto applyAndEmit = [this, transform]() {
      if (m_buildingUi || !m_entity) {
        return;
      }

      // Capture old state from component
      TransformData oldData;
      oldData.position = {transform->GetPositionX(), transform->GetPositionY(),
                          transform->GetPositionZ()};
      oldData.rotation = {transform->GetRotationXDegrees(),
                          transform->GetRotationYDegrees(),
                          transform->GetRotationZDegrees()};
      oldData.scale = {transform->GetScaleX(), transform->GetScaleY(),
                       transform->GetScaleZ()};

      // Calculate new state from UI
      TransformData newData;
      newData.position = {static_cast<float>(m_posX->value()),
                          static_cast<float>(m_posY->value()),
                          static_cast<float>(m_posZ->value())};
      newData.rotation = {static_cast<float>(m_rotX->value()),
                          static_cast<float>(m_rotY->value()),
                          static_cast<float>(m_rotZ->value())};
      newData.scale = {static_cast<float>(m_scaleX->value()),
                       static_cast<float>(m_scaleY->value()),
                       static_cast<float>(m_scaleZ->value())};

      // Use Command if available
      if (m_commandExecutor) {
        m_commandExecutor(
            std::make_unique<TransformCommand>(m_entity, oldData, newData));
      } else {
        // Fallback direct application
        transform->SetPosition(newData.position[0], newData.position[1],
                               newData.position[2]);
        transform->SetRotationDegrees(newData.rotation[0], newData.rotation[1],
                                      newData.rotation[2]);
        transform->SetScale(newData.scale[0], newData.scale[1],
                            newData.scale[2]);
        emit sceneModified();
      }

      emit transformChanged(m_entity->GetId(), newData.position[0],
                            newData.position[1], newData.position[2],
                            newData.rotation[0], newData.rotation[1],
                            newData.rotation[2], newData.scale[0],
                            newData.scale[1], newData.scale[2]);
    };

    connect(m_posX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_posY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_posZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_rotX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_rotY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_rotZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_scaleX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_scaleY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });
    connect(m_scaleZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAndEmit](double) { applyAndEmit(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Transform"), transform, formHost));
  }

  if (mesh) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    m_colorR = makeSpin(0.0, 1.0, 0.01);
    m_colorG = makeSpin(0.0, 1.0, 0.01);
    m_colorB = makeSpin(0.0, 1.0, 0.01);
    m_meshRotationSpeed = makeSpin(-720.0, 720.0, 1.0);
    m_meshAsset = new QComboBox(m_content);
    m_meshAsset->setEditable(true);
    m_meshAsset->setInsertPolicy(QComboBox::NoInsert);
    m_meshTexture = new QComboBox(m_content);
    m_meshTexture->setEditable(true);
    m_meshTexture->setInsertPolicy(QComboBox::NoInsert);
    m_meshVisible = new QCheckBox(m_content);

    auto color = mesh->GetColor();
    m_colorR->setValue(color[0]);
    m_colorG->setValue(color[1]);
    m_colorB->setValue(color[2]);
    m_meshRotationSpeed->setValue(mesh->GetRotationSpeedDegPerSec());
    m_meshVisible->setChecked(mesh->IsVisible());

    form->addRow(tr("Color R"), m_colorR);
    form->addRow(tr("Color G"), m_colorG);
    form->addRow(tr("Color B"), m_colorB);
    form->addRow(tr("Rotation Speed (deg/s)"), m_meshRotationSpeed);
    form->addRow(tr("Visible"), m_meshVisible);
    form->addRow(tr("Mesh Asset"), m_meshAsset);
    form->addRow(tr("Albedo Texture"), m_meshTexture);

    auto *meshTexturePreview = new QLabel(tr("No texture"), formHost);
    meshTexturePreview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->addRow(tr("Texture Preview"), meshTexturePreview);

    m_meshAsset->addItem(tr("(None)"), QString());
    m_meshTexture->addItem(tr("(None)"), QString());
    if (m_assetRegistry) {
      for (const auto &entry : m_assetRegistry->GetEntries()) {
        if (entry.type != Assets::AssetRegistry::AssetType::Mesh) {
          continue;
        }
        const QString id = QString::fromStdString(entry.id);
        m_meshAsset->addItem(id, id);
      }

      for (const auto &entry : m_assetRegistry->GetEntries()) {
        if (entry.type != Assets::AssetRegistry::AssetType::Texture) {
          continue;
        }
        const QString id = QString::fromStdString(entry.id);
        m_meshTexture->addItem(id, id);
      }
    }

    const QString currentMeshId =
        QString::fromStdString(mesh->GetMeshAssetId());
    const int meshIndex = m_meshAsset->findData(currentMeshId);
    if (meshIndex >= 0) {
      m_meshAsset->setCurrentIndex(meshIndex);
    } else if (!currentMeshId.isEmpty()) {
      m_meshAsset->addItem(currentMeshId, currentMeshId);
      m_meshAsset->setCurrentIndex(m_meshAsset->count() - 1);
    }

    const QString currentTextureId =
        QString::fromStdString(mesh->GetMaterialAssetId());
    const int textureIndex = m_meshTexture->findData(currentTextureId);
    if (textureIndex >= 0) {
      m_meshTexture->setCurrentIndex(textureIndex);
    } else if (!currentTextureId.isEmpty()) {
      m_meshTexture->addItem(currentTextureId, currentTextureId);
      m_meshTexture->setCurrentIndex(m_meshTexture->count() - 1);
    }

    auto *meshVerticesLabel = new QLabel(tr("N/A"), formHost);
    auto *meshIndicesLabel = new QLabel(tr("N/A"), formHost);
    auto *meshTrianglesLabel = new QLabel(tr("N/A"), formHost);
    auto *meshBoundsMinLabel = new QLabel(tr("N/A"), formHost);
    auto *meshBoundsMaxLabel = new QLabel(tr("N/A"), formHost);
    auto *meshBoundsCenterLabel = new QLabel(tr("N/A"), formHost);
    auto *meshBoundsRadiusLabel = new QLabel(tr("N/A"), formHost);
    auto *meshMemoryLabel = new QLabel(tr("N/A"), formHost);

    form->addRow(tr("Vertices"), meshVerticesLabel);
    form->addRow(tr("Indices"), meshIndicesLabel);
    form->addRow(tr("Triangles"), meshTrianglesLabel);
    form->addRow(tr("Bounds Min"), meshBoundsMinLabel);
    form->addRow(tr("Bounds Max"), meshBoundsMaxLabel);
    form->addRow(tr("Bounds Center"), meshBoundsCenterLabel);
    form->addRow(tr("Bounds Radius"), meshBoundsRadiusLabel);
    form->addRow(tr("CPU Memory (approx.)"), meshMemoryLabel);

    auto updateTexturePreview = [this, meshTexturePreview]() {
      auto setPreviewText = [&](const QString &text) {
        meshTexturePreview->setPixmap(QPixmap());
        meshTexturePreview->setText(text);
        meshTexturePreview->setToolTip(QString());
      };

      if (!m_assetRegistry || !m_meshTexture) {
        setPreviewText(tr("N/A"));
        return;
      }

      const QString texId = m_meshTexture->currentText().trimmed();
      const QString normalized = (texId == tr("(None)")) ? QString() : texId;
      if (normalized.isEmpty()) {
        setPreviewText(tr("No texture"));
        return;
      }

      std::string previewAssetId = normalized.toStdString();
      if (m_assetRegistry) {
        // Check if it is a material and resolve albedo
        const auto *mat = m_assetRegistry->GetMaterial(previewAssetId);
        if (mat) {
          previewAssetId = mat->GetAlbedoMapId();
        }
      }

      if (previewAssetId.empty()) {
        setPreviewText(tr("No Albedo"));
        return;
      }

      const auto *entry = m_assetRegistry->FindEntry(previewAssetId);
      if (!entry) {
        setPreviewText(tr("Texture not found"));
        return;
      }

      QFileInfo fileInfo(QString::fromStdString(entry->path.string()));
      if (!fileInfo.exists()) {
        setPreviewText(tr("Missing file"));
        return;
      }

      QImageReader reader(fileInfo.absoluteFilePath());
      reader.setAutoTransform(true);
      const QSize imageSize = reader.size();
      const int previewMax = 128;
      if (imageSize.isValid()) {
        reader.setScaledSize(
            imageSize.scaled(previewMax, previewMax, Qt::KeepAspectRatio));
      }
      const QImage image = reader.read();
      if (image.isNull()) {
        setPreviewText(tr("Preview unavailable"));
        return;
      }

      meshTexturePreview->setPixmap(QPixmap::fromImage(image));
      meshTexturePreview->setText(QString());
      if (imageSize.isValid()) {
        meshTexturePreview->setToolTip(tr("%1 (%2 x %3)")
                                           .arg(fileInfo.fileName())
                                           .arg(imageSize.width())
                                           .arg(imageSize.height()));
      } else {
        meshTexturePreview->setToolTip(fileInfo.fileName());
      }
    };

    auto updateMeshStats = [this, mesh, meshVerticesLabel, meshIndicesLabel,
                            meshTrianglesLabel, meshBoundsMinLabel,
                            meshBoundsMaxLabel, meshBoundsCenterLabel,
                            meshBoundsRadiusLabel, meshMemoryLabel]() {
      auto setAll = [&](const QString &value) {
        meshVerticesLabel->setText(value);
        meshIndicesLabel->setText(value);
        meshTrianglesLabel->setText(value);
        meshBoundsMinLabel->setText(value);
        meshBoundsMaxLabel->setText(value);
        meshBoundsCenterLabel->setText(value);
        meshBoundsRadiusLabel->setText(value);
        meshMemoryLabel->setText(value);
      };

      if (!m_assetRegistry) {
        setAll(tr("N/A"));
        return;
      }

      const std::string meshId = mesh ? mesh->GetMeshAssetId() : std::string();
      if (meshId.empty()) {
        setAll(tr("N/A"));
        return;
      }

      const auto *meshData = m_assetRegistry->LoadMeshData(meshId);
      if (!meshData) {
        setAll(tr("Not loaded"));
        return;
      }

      const long long vertexCount =
          static_cast<long long>(meshData->positions.size());
      const long long indexCount =
          static_cast<long long>(meshData->indices.size());
      const long long triangleCount = indexCount / 3;

      meshVerticesLabel->setText(QString::number(vertexCount));
      meshIndicesLabel->setText(QString::number(indexCount));
      meshTrianglesLabel->setText(QString::number(triangleCount));
      meshBoundsMinLabel->setText(FormatVec3(meshData->boundsMin));
      meshBoundsMaxLabel->setText(FormatVec3(meshData->boundsMax));
      meshBoundsCenterLabel->setText(FormatVec3(meshData->boundsCenter));
      meshBoundsRadiusLabel->setText(
          QString::number(meshData->boundsRadius, 'f', 3));
      meshMemoryLabel->setText(FormatBytes(EstimateMeshBytes(*meshData)));
    };

    auto updateMesh = [this, mesh, updateMeshStats, updateTexturePreview]() {
      if (m_buildingUi || !m_entity) {
        return;
      }

      mesh->SetColor(static_cast<float>(m_colorR->value()),
                     static_cast<float>(m_colorG->value()),
                     static_cast<float>(m_colorB->value()));
      mesh->SetRotationSpeedDegPerSec(
          static_cast<float>(m_meshRotationSpeed->value()));
      if (m_meshVisible) {
        mesh->SetVisible(m_meshVisible->isChecked());
      }
      if (m_meshAsset) {
        const QString meshId = m_meshAsset->currentText().trimmed();
        const QString normalized =
            (meshId == tr("(None)")) ? QString() : meshId;
        mesh->SetMeshAssetId(normalized.toStdString());
      }
      if (m_meshTexture) {
        const QString texId = m_meshTexture->currentText().trimmed();
        const QString normalized = (texId == tr("(None)")) ? QString() : texId;
        mesh->SetMaterialAssetId(normalized.toStdString());
      }
      emit sceneModified();
      updateMeshStats();
      updateTexturePreview();
    };

    connect(m_colorR, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateMesh](double) { updateMesh(); });
    connect(m_colorG, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateMesh](double) { updateMesh(); });
    connect(m_colorB, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateMesh](double) { updateMesh(); });
    connect(m_meshRotationSpeed,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateMesh](double) { updateMesh(); });
    if (m_meshVisible) {
      connect(m_meshVisible, &QCheckBox::toggled, this,
              [updateMesh](bool) { updateMesh(); });
    }
    if (m_meshAsset) {
      connect(m_meshAsset, &QComboBox::currentTextChanged, this,
              [updateMesh](const QString &) { updateMesh(); });
    }
    if (m_meshTexture) {
      connect(m_meshTexture, &QComboBox::currentTextChanged, this,
              [updateMesh](const QString &) { updateMesh(); });
    }

    updateMeshStats();
    updateTexturePreview();

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Mesh Renderer"), mesh, formHost));
  }

  if (light) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    auto *hintLabel = new QLabel(
        tr("Direction uses Transform rotation (X=Pitch, Y=Yaw)."), m_content);
    hintLabel->setStyleSheet("color: #aeb6c2;");
    form->addRow(hintLabel);

    m_lightEnabled = new QCheckBox(m_content);
    m_lightEnabled->setChecked(light->IsEnabled());
    m_lightType = new QComboBox(m_content);
    m_lightType->addItem(
        tr("Directional"),
        static_cast<int>(Scene::LightComponent::LightType::Directional));
    m_lightType->addItem(
        tr("Point"), static_cast<int>(Scene::LightComponent::LightType::Point));
    m_lightType->addItem(
        tr("Spot"), static_cast<int>(Scene::LightComponent::LightType::Spot));
    m_lightColorR = makeSpin(0.0, 1.0, 0.01);
    m_lightColorG = makeSpin(0.0, 1.0, 0.01);
    m_lightColorB = makeSpin(0.0, 1.0, 0.01);
    m_lightIntensity = makeSpin(0.0, 10.0, 0.1);
    m_lightRange = makeSpin(0.01, 10000.0, 0.1);
    m_lightInnerAngle = makeSpin(0.0, 179.0, 1.0);
    m_lightOuterAngle = makeSpin(0.0, 179.0, 1.0);
    m_lightAmbientR = makeSpin(0.0, 1.0, 0.01);
    m_lightAmbientG = makeSpin(0.0, 1.0, 0.01);
    m_lightAmbientB = makeSpin(0.0, 1.0, 0.01);
    m_lightPrimary = new QCheckBox(m_content);

    const auto color = light->GetColor();
    const auto ambient = light->GetAmbientColor();
    if (m_lightType) {
      m_lightType->setCurrentIndex(static_cast<int>(light->GetType()));
    }
    m_lightColorR->setValue(color[0]);
    m_lightColorG->setValue(color[1]);
    m_lightColorB->setValue(color[2]);
    m_lightIntensity->setValue(light->GetIntensity());
    if (m_lightRange) {
      m_lightRange->setValue(light->GetRange());
    }
    if (m_lightInnerAngle) {
      m_lightInnerAngle->setValue(light->GetInnerConeAngle());
    }
    if (m_lightOuterAngle) {
      m_lightOuterAngle->setValue(light->GetOuterConeAngle());
    }
    m_lightAmbientR->setValue(ambient[0]);
    m_lightAmbientG->setValue(ambient[1]);
    m_lightAmbientB->setValue(ambient[2]);
    if (m_lightPrimary) {
      m_lightPrimary->setChecked(light->IsPrimary());
    }

    form->addRow(tr("Enabled"), m_lightEnabled);
    form->addRow(tr("Type"), m_lightType);
    form->addRow(tr("Color R"), m_lightColorR);
    form->addRow(tr("Color G"), m_lightColorG);
    form->addRow(tr("Color B"), m_lightColorB);
    form->addRow(tr("Intensity"), m_lightIntensity);
    form->addRow(tr("Range"), m_lightRange);
    form->addRow(tr("Inner Angle"), m_lightInnerAngle);
    form->addRow(tr("Outer Angle"), m_lightOuterAngle);
    form->addRow(tr("Primary"), m_lightPrimary);
    form->addRow(tr("Ambient R"), m_lightAmbientR);
    form->addRow(tr("Ambient G"), m_lightAmbientG);
    form->addRow(tr("Ambient B"), m_lightAmbientB);

    auto updateLightVisibility = [this]() {
      if (!m_lightType) {
        return;
      }
      const auto type = static_cast<Scene::LightComponent::LightType>(
          m_lightType->currentData().toInt());
      const bool isDirectional =
          type == Scene::LightComponent::LightType::Directional;
      const bool isSpot = type == Scene::LightComponent::LightType::Spot;

      if (m_lightRange)
        m_lightRange->setEnabled(!isDirectional);
      if (m_lightInnerAngle)
        m_lightInnerAngle->setEnabled(isSpot);
      if (m_lightOuterAngle)
        m_lightOuterAngle->setEnabled(isSpot);
      if (m_lightAmbientR)
        m_lightAmbientR->setEnabled(isDirectional);
      if (m_lightAmbientG)
        m_lightAmbientG->setEnabled(isDirectional);
      if (m_lightAmbientB)
        m_lightAmbientB->setEnabled(isDirectional);
      if (m_lightPrimary)
        m_lightPrimary->setEnabled(isDirectional);
    };
    updateLightVisibility();

    auto updateLight = [this, light, updateLightVisibility]() {
      if (m_buildingUi || !m_entity) {
        return;
      }

      if (m_lightEnabled) {
        light->SetEnabled(m_lightEnabled->isChecked());
      }
      if (m_lightType) {
        light->SetType(static_cast<Scene::LightComponent::LightType>(
            m_lightType->currentData().toInt()));
      }
      light->SetColor(static_cast<float>(m_lightColorR->value()),
                      static_cast<float>(m_lightColorG->value()),
                      static_cast<float>(m_lightColorB->value()));
      light->SetIntensity(static_cast<float>(m_lightIntensity->value()));
      if (m_lightRange) {
        light->SetRange(static_cast<float>(m_lightRange->value()));
      }
      if (m_lightInnerAngle) {
        light->SetInnerConeAngle(
            static_cast<float>(m_lightInnerAngle->value()));
      }
      if (m_lightOuterAngle) {
        light->SetOuterConeAngle(
            static_cast<float>(m_lightOuterAngle->value()));
      }
      light->SetAmbientColor(static_cast<float>(m_lightAmbientR->value()),
                             static_cast<float>(m_lightAmbientG->value()),
                             static_cast<float>(m_lightAmbientB->value()));
      if (m_lightPrimary) {
        light->SetPrimary(m_lightPrimary->isChecked());
      }
      emit sceneModified();
      updateLightVisibility();
    };

    if (m_lightEnabled) {
      connect(m_lightEnabled, &QCheckBox::toggled, this,
              [updateLight](bool) { updateLight(); });
    }
    if (m_lightType) {
      connect(m_lightType, qOverload<int>(&QComboBox::currentIndexChanged),
              this, [updateLight](int) { updateLight(); });
    }
    connect(m_lightColorR, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    connect(m_lightColorG, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    connect(m_lightColorB, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    connect(m_lightIntensity, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    if (m_lightRange) {
      connect(m_lightRange, qOverload<double>(&QDoubleSpinBox::valueChanged),
              this, [updateLight](double) { updateLight(); });
    }
    if (m_lightInnerAngle) {
      connect(m_lightInnerAngle,
              qOverload<double>(&QDoubleSpinBox::valueChanged), this,
              [updateLight](double) { updateLight(); });
    }
    if (m_lightOuterAngle) {
      connect(m_lightOuterAngle,
              qOverload<double>(&QDoubleSpinBox::valueChanged), this,
              [updateLight](double) { updateLight(); });
    }
    connect(m_lightAmbientR, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    connect(m_lightAmbientG, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    connect(m_lightAmbientB, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateLight](double) { updateLight(); });
    if (m_lightPrimary) {
      connect(m_lightPrimary, &QCheckBox::toggled, this,
              [updateLight](bool) { updateLight(); });
    }

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Light"), light, formHost));
  }

  if (camera) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    auto *projCombo = new QComboBox(m_content);
    projCombo->addItem(tr("Perspective"), 0);
    projCombo->addItem(tr("Orthographic"), 1);
    projCombo->setCurrentIndex(static_cast<int>(camera->GetProjectionType()));

    auto *fovSpin = makeSpin(1.0, 179.0, 1.0);
    fovSpin->setValue(camera->GetVerticalFov());

    auto *nearSpin = makeSpin(0.001, 10000.0, 0.1);
    nearSpin->setValue(camera->GetNearClip());

    auto *farSpin = makeSpin(0.001, 10000.0, 10.0);
    farSpin->setValue(camera->GetFarClip());

    auto *orthoSizeSpin = makeSpin(0.1, 10000.0, 1.0);
    orthoSizeSpin->setValue(camera->GetOrthographicSize());

    auto *primaryCheck = new QCheckBox(m_content);
    primaryCheck->setChecked(camera->IsPrimary());

    form->addRow(tr("Projection"), projCombo);
    form->addRow(tr("Vertical FOV"), fovSpin);
    form->addRow(tr("Near Clip"), nearSpin);
    form->addRow(tr("Far Clip"), farSpin);
    form->addRow(tr("Ortho Size"), orthoSizeSpin);
    form->addRow(tr("Primary"), primaryCheck);

    auto updateVisibility = [fovSpin, orthoSizeSpin, projCombo]() {
      bool isPersp = (projCombo->currentIndex() == 0);
      fovSpin->setEnabled(isPersp);
      orthoSizeSpin->setEnabled(!isPersp);
    };
    updateVisibility();

    auto updateCamera = [this, camera, projCombo, fovSpin, nearSpin, farSpin,
                         orthoSizeSpin, primaryCheck, updateVisibility]() {
      if (m_buildingUi || !m_entity)
        return;

      camera->SetProjectionType(
          static_cast<Scene::CameraComponent::ProjectionType>(
              projCombo->currentData().toInt()));
      camera->SetVerticalFov(static_cast<float>(fovSpin->value()));
      camera->SetNearClip(static_cast<float>(nearSpin->value()));
      camera->SetFarClip(static_cast<float>(farSpin->value()));
      camera->SetOrthographicSize(static_cast<float>(orthoSizeSpin->value()));
      camera->SetPrimary(primaryCheck->isChecked());

      updateVisibility();
      emit sceneModified();
    };

    connect(projCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [updateCamera](int) { updateCamera(); });
    connect(fovSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCamera](double) { updateCamera(); });
    connect(nearSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCamera](double) { updateCamera(); });
    connect(farSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCamera](double) { updateCamera(); });
    connect(orthoSizeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateCamera](double) { updateCamera(); });
    connect(primaryCheck, &QCheckBox::toggled, this,
            [updateCamera](bool) { updateCamera(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Camera"), camera, formHost));
  }

  // Rigidbody Component UI
  if (rigidbody) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    auto *motionTypeCombo = new QComboBox(m_content);
    motionTypeCombo->addItem(tr("Static"), 0);
    motionTypeCombo->addItem(tr("Kinematic"), 1);
    motionTypeCombo->addItem(tr("Dynamic"), 2);
    motionTypeCombo->setCurrentIndex(
        static_cast<int>(rigidbody->GetMotionType()));

    auto *massSpin = makeSpin(0.001, 10000.0, 0.1);
    massSpin->setValue(rigidbody->GetMass());

    auto *linearDampSpin = makeSpin(0.0, 10.0, 0.01);
    linearDampSpin->setValue(rigidbody->GetLinearDamping());

    auto *angularDampSpin = makeSpin(0.0, 10.0, 0.01);
    angularDampSpin->setValue(rigidbody->GetAngularDamping());

    auto *useGravityCheck = new QCheckBox(m_content);
    useGravityCheck->setChecked(rigidbody->UseGravity());

    auto *frictionSpin = makeSpin(0.0, 2.0, 0.05);
    frictionSpin->setValue(rigidbody->GetFriction());

    auto *restitutionSpin = makeSpin(0.0, 1.0, 0.05);
    restitutionSpin->setValue(rigidbody->GetRestitution());

    form->addRow(tr("Motion Type"), motionTypeCombo);
    form->addRow(tr("Mass"), massSpin);
    form->addRow(tr("Linear Damping"), linearDampSpin);
    form->addRow(tr("Angular Damping"), angularDampSpin);
    form->addRow(tr("Use Gravity"), useGravityCheck);
    form->addRow(tr("Friction"), frictionSpin);
    form->addRow(tr("Restitution"), restitutionSpin);

    auto updateRigidbody = [this, rigidbody, motionTypeCombo, massSpin,
                            linearDampSpin, angularDampSpin, useGravityCheck,
                            frictionSpin, restitutionSpin]() {
      if (m_buildingUi || !m_entity)
        return;

      rigidbody->SetMotionType(
          static_cast<Scene::RigidbodyComponent::MotionType>(
              motionTypeCombo->currentData().toInt()));
      rigidbody->SetMass(static_cast<float>(massSpin->value()));
      rigidbody->SetLinearDamping(static_cast<float>(linearDampSpin->value()));
      rigidbody->SetAngularDamping(
          static_cast<float>(angularDampSpin->value()));
      rigidbody->SetUseGravity(useGravityCheck->isChecked());
      rigidbody->SetFriction(static_cast<float>(frictionSpin->value()));
      rigidbody->SetRestitution(static_cast<float>(restitutionSpin->value()));
      emit sceneModified();
    };

    connect(motionTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [updateRigidbody](int) { updateRigidbody(); });
    connect(massSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateRigidbody](double) { updateRigidbody(); });
    connect(linearDampSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateRigidbody](double) { updateRigidbody(); });
    connect(angularDampSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateRigidbody](double) { updateRigidbody(); });
    connect(useGravityCheck, &QCheckBox::toggled, this,
            [updateRigidbody](bool) { updateRigidbody(); });
    connect(frictionSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateRigidbody](double) { updateRigidbody(); });
    connect(restitutionSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateRigidbody](double) { updateRigidbody(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Rigidbody"), rigidbody, formHost));
  }

  // AI Behavior Component UI
  if (aiBehavior) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setSpacing(6);

    m_aiMode = new QComboBox(m_content);
    m_aiMode->addItem(
        tr("Stub"),
        static_cast<int>(Scene::AIBehaviorComponent::ExecutionMode::Stub));
    m_aiMode->addItem(
        tr("Local Model"),
        static_cast<int>(
            Scene::AIBehaviorComponent::ExecutionMode::LocalModel));
    m_aiMode->addItem(
        tr("On-Device"),
        static_cast<int>(
            Scene::AIBehaviorComponent::ExecutionMode::OnDevice));
    m_aiMode->addItem(
        tr("Remote Service"),
        static_cast<int>(
            Scene::AIBehaviorComponent::ExecutionMode::RemoteService));
    const int modeIdx =
        m_aiMode->findData(static_cast<int>(aiBehavior->GetExecutionMode()));
    if (modeIdx >= 0) {
      m_aiMode->setCurrentIndex(modeIdx);
    }

    m_aiPromptAsset = new QComboBox(m_content);
    m_aiPromptAsset->addItem(tr("None"), QString());
    if (m_assetRegistry) {
      for (const auto &entry : m_assetRegistry->GetEntries()) {
        if (entry.type == Assets::AssetRegistry::AssetType::Script ||
            entry.type == Assets::AssetRegistry::AssetType::BehaviorPrompt ||
            entry.type == Assets::AssetRegistry::AssetType::Other) {
          const QString id = QString::fromStdString(entry.id);
          QString label =
              QString::fromStdString(entry.path.filename().string());
          if (m_assetRegistry->IsVirtualAsset(entry.id)) {
            label += tr(" (virtual)");
          }
          m_aiPromptAsset->addItem(label, id);
        }
      }
    }
    const QString promptId =
        QString::fromStdString(aiBehavior->GetPromptAssetId());
    int promptIdx = m_aiPromptAsset->findData(promptId);
    if (promptIdx >= 0) {
      m_aiPromptAsset->setCurrentIndex(promptIdx);
    }

    m_aiPersonality = new QLineEdit(
        QString::fromStdString(aiBehavior->GetPersonality()), m_content);
    m_aiKnowledge = new QLineEdit(
        QString::fromStdString(aiBehavior->GetKnowledgeBase()), m_content);
    m_aiContext = new QTextEdit(m_content);
    m_aiContext->setPlaceholderText(
        tr("Scene context, observations, goals..."));
    m_aiContext->setPlainText(QString::fromStdString(aiBehavior->GetContext()));

    m_aiInlinePrompt = new QTextEdit(m_content);
    m_aiInlinePrompt->setPlaceholderText(
        tr("Inline behavior prompt (used if no prompt asset is set)"));
    m_aiInlinePrompt->setPlainText(
        QString::fromStdString(aiBehavior->GetInlinePrompt()));

    m_aiDecisionInterval = makeSpin(0.05, 5.0, 0.05);
    m_aiDecisionInterval->setDecimals(2);
    m_aiDecisionInterval->setValue(aiBehavior->GetDecisionInterval());

    m_aiStateLabel = new QLabel(
        tr("State: %1")
            .arg(QString::fromStdString(aiBehavior->GetCurrentState())),
        m_content);
    m_aiReasonLabel = new QLabel(
        tr("Reason: %1")
            .arg(QString::fromStdString(aiBehavior->GetLastReason())),
        m_content);
    m_aiReasonLabel->setWordWrap(true);
    m_aiInferenceLabel = new QLabel(
        tr("Inference: %1")
            .arg(QString::fromStdString(
                aiBehavior->GetLastInferenceSource())),
        m_content);
    m_aiLatencyLabel = new QLabel(
        tr("Latency: %1 ms")
            .arg(static_cast<qulonglong>(
                aiBehavior->GetLastInferenceLatencyMs())),
        m_content);
    m_aiBudgetLabel = new QLabel(
        tr("Budget Remaining: %1")
            .arg(aiBehavior->GetLastBudgetRemaining()),
        m_content);
    m_aiScriptPathLabel = new QLabel(m_content);
    m_aiScriptPathLabel->setTextFormat(Qt::PlainText);
    m_aiScriptPathLabel->setWordWrap(true);
    m_aiScriptDiagLabel = new QLabel(m_content);
    m_aiScriptDiagLabel->setTextFormat(Qt::PlainText);
    m_aiScriptDiagLabel->setWordWrap(true);

    form->addRow(tr("Execution"), m_aiMode);
    form->addRow(tr("Prompt Asset"), m_aiPromptAsset);
    form->addRow(tr("Inline Prompt"), m_aiInlinePrompt);
    form->addRow(tr("Personality"), m_aiPersonality);
    form->addRow(tr("Knowledge Base"), m_aiKnowledge);
    form->addRow(tr("Context"), m_aiContext);
    form->addRow(tr("Decision Interval (s)"), m_aiDecisionInterval);
    form->addRow(m_aiStateLabel);
    form->addRow(m_aiReasonLabel);
    form->addRow(m_aiInferenceLabel);
    form->addRow(m_aiLatencyLabel);
    form->addRow(m_aiBudgetLabel);
    form->addRow(m_aiScriptPathLabel);
    form->addRow(m_aiScriptDiagLabel);

    UpdateAIScriptStatus();

    auto updateBehavior = [this, aiBehavior]() {
      aiBehavior->SetPersonality(m_aiPersonality->text().toStdString());
      aiBehavior->SetKnowledgeBase(m_aiKnowledge->text().toStdString());
      aiBehavior->SetContext(m_aiContext->toPlainText().toStdString());
      aiBehavior->SetInlinePrompt(
          m_aiInlinePrompt->toPlainText().toStdString());
      aiBehavior->SetDecisionInterval(
          static_cast<float>(m_aiDecisionInterval->value()));
      emit sceneModified();
    };

    connect(
        m_aiMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [aiBehavior, this](int idx) {
          const int value = m_aiMode->itemData(idx).toInt();
          aiBehavior->SetExecutionMode(
              static_cast<Scene::AIBehaviorComponent::ExecutionMode>(value));
          emit sceneModified();
        });
    connect(m_aiPromptAsset, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [aiBehavior, this](int idx) {
              const QString id = m_aiPromptAsset->itemData(idx).toString();
              aiBehavior->SetPromptAssetId(id.toStdString());
              emit sceneModified();
            });
    connect(m_aiPersonality, &QLineEdit::editingFinished, this,
            [updateBehavior]() { updateBehavior(); });
    connect(m_aiKnowledge, &QLineEdit::editingFinished, this,
            [updateBehavior]() { updateBehavior(); });
    connect(m_aiContext, &QTextEdit::textChanged, this,
            [updateBehavior]() { updateBehavior(); });
    connect(m_aiInlinePrompt, &QTextEdit::textChanged, this,
            [updateBehavior]() { updateBehavior(); });
    connect(m_aiDecisionInterval,
            qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateBehavior](double) { updateBehavior(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("AI Behavior"), aiBehavior, formHost));
  }

  // Particle Emitter Component UI
  if (particleEmitter) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setSpacing(6);

    // Preset selector
    auto *presetCombo = new QComboBox(m_content);
    presetCombo->addItem(tr("(Custom)"), QString());
    presetCombo->addItem(tr("Fire"), "fire");
    presetCombo->addItem(tr("Smoke"), "smoke");
    presetCombo->addItem(tr("Sparks"), "sparks");
    presetCombo->addItem(tr("Rain"), "rain");
    presetCombo->addItem(tr("Snow"), "snow");
    presetCombo->addItem(tr("Magic"), "magic");
    form->addRow(tr("Preset"), presetCombo);

    // Playback controls
    auto *playbackRow = new QWidget(m_content);
    auto *playbackLayout = new QHBoxLayout(playbackRow);
    playbackLayout->setContentsMargins(0, 0, 0, 0);
    auto *playBtn = new QPushButton(tr("Play"), playbackRow);
    auto *stopBtn = new QPushButton(tr("Stop"), playbackRow);
    auto *burstBtn = new QPushButton(tr("Burst 10"), playbackRow);
    playbackLayout->addWidget(playBtn);
    playbackLayout->addWidget(stopBtn);
    playbackLayout->addWidget(burstBtn);
    form->addRow(tr("Playback"), playbackRow);

    // Status label
    auto *statusLabel =
        new QLabel(particleEmitter->IsPlaying() ? tr("Playing") : tr("Stopped"),
                   m_content);
    form->addRow(tr("Status"), statusLabel);

    // Emission settings
    auto *emissionRate = makeSpin(0.0, 1000.0, 1.0);
    emissionRate->setValue(particleEmitter->GetEmissionRate());
    form->addRow(tr("Emission Rate"), emissionRate);

    auto *maxParticles = new QSpinBox(m_content);
    maxParticles->setRange(1, 100000);
    maxParticles->setValue(
        static_cast<int>(particleEmitter->GetMaxParticles()));
    form->addRow(tr("Max Particles"), maxParticles);

    auto *looping = new QCheckBox(m_content);
    looping->setChecked(particleEmitter->IsLooping());
    form->addRow(tr("Looping"), looping);

    auto *playOnAwake = new QCheckBox(m_content);
    playOnAwake->setChecked(particleEmitter->GetPlayOnAwake());
    form->addRow(tr("Play on Awake"), playOnAwake);

    // Lifetime
    auto *minLifetime = makeSpin(0.01, 60.0, 0.1);
    minLifetime->setValue(particleEmitter->GetMinLifetime());
    auto *maxLifetime = makeSpin(0.01, 60.0, 0.1);
    maxLifetime->setValue(particleEmitter->GetMaxLifetime());
    form->addRow(tr("Lifetime Min"), minLifetime);
    form->addRow(tr("Lifetime Max"), maxLifetime);

    // Speed
    auto *minSpeed = makeSpin(0.0, 100.0, 0.1);
    minSpeed->setValue(particleEmitter->GetMinSpeed());
    auto *maxSpeed = makeSpin(0.0, 100.0, 0.1);
    maxSpeed->setValue(particleEmitter->GetMaxSpeed());
    form->addRow(tr("Speed Min"), minSpeed);
    form->addRow(tr("Speed Max"), maxSpeed);

    // Size
    auto *startSize = makeSpin(0.001, 10.0, 0.01);
    startSize->setValue(particleEmitter->GetStartSize());
    auto *endSize = makeSpin(0.0, 10.0, 0.01);
    endSize->setValue(particleEmitter->GetEndSize());
    form->addRow(tr("Start Size"), startSize);
    form->addRow(tr("End Size"), endSize);

    // Gravity
    auto *gravity = makeSpin(-10.0, 10.0, 0.1);
    gravity->setValue(particleEmitter->GetGravityMultiplier());
    form->addRow(tr("Gravity"), gravity);

    // Blend mode
    auto *blendMode = new QComboBox(m_content);
    blendMode->addItem(tr("Alpha"), 0);
    blendMode->addItem(tr("Additive"), 1);
    blendMode->setCurrentIndex(
        static_cast<int>(particleEmitter->GetBlendMode()));
    form->addRow(tr("Blend Mode"), blendMode);

    // AI Integration - Prompt Hint
    auto *promptHint = new QLineEdit(
        QString::fromStdString(particleEmitter->GetPromptHint()), m_content);
    promptHint->setPlaceholderText(tr("e.g., 'campfire', 'magic sparkles'"));
    form->addRow(tr("AI Hint"), promptHint);

    // Active particle count label
    auto *particleCount = new QLabel(
        tr("%1 active").arg(particleEmitter->GetActiveParticleCount()),
        m_content);
    form->addRow(tr("Particles"), particleCount);

    // Connect preset
    connect(presetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, particleEmitter, presetCombo, emissionRate, minLifetime,
             maxLifetime, minSpeed, maxSpeed, startSize, endSize, gravity,
             blendMode](int idx) {
              const QString preset = presetCombo->itemData(idx).toString();
              if (!preset.isEmpty()) {
                particleEmitter->SetPreset(preset.toStdString());
                // Update UI to reflect new values
                emissionRate->setValue(particleEmitter->GetEmissionRate());
                minLifetime->setValue(particleEmitter->GetMinLifetime());
                maxLifetime->setValue(particleEmitter->GetMaxLifetime());
                minSpeed->setValue(particleEmitter->GetMinSpeed());
                maxSpeed->setValue(particleEmitter->GetMaxSpeed());
                startSize->setValue(particleEmitter->GetStartSize());
                endSize->setValue(particleEmitter->GetEndSize());
                gravity->setValue(particleEmitter->GetGravityMultiplier());
                blendMode->setCurrentIndex(
                    static_cast<int>(particleEmitter->GetBlendMode()));
                emit sceneModified();
              }
            });

    // Connect playback buttons
    connect(playBtn, &QPushButton::clicked, this,
            [particleEmitter, statusLabel]() {
              particleEmitter->Play();
              statusLabel->setText(tr("Playing"));
            });
    connect(stopBtn, &QPushButton::clicked, this,
            [particleEmitter, statusLabel]() {
              particleEmitter->Stop();
              statusLabel->setText(tr("Stopped"));
            });
    connect(burstBtn, &QPushButton::clicked, this,
            [particleEmitter]() { particleEmitter->Burst(10); });

    // Connect property changes
    auto updateEmitter = [this, particleEmitter, emissionRate, maxParticles,
                          looping, playOnAwake, minLifetime, maxLifetime,
                          minSpeed, maxSpeed, startSize, endSize, gravity,
                          blendMode, promptHint]() {
      particleEmitter->SetEmissionRate(
          static_cast<float>(emissionRate->value()));
      particleEmitter->SetMaxParticles(
          static_cast<uint32_t>(maxParticles->value()));
      particleEmitter->SetLooping(looping->isChecked());
      particleEmitter->SetPlayOnAwake(playOnAwake->isChecked());
      particleEmitter->SetLifetimeRange(
          static_cast<float>(minLifetime->value()),
          static_cast<float>(maxLifetime->value()));
      particleEmitter->SetSpeedRange(static_cast<float>(minSpeed->value()),
                                     static_cast<float>(maxSpeed->value()));
      particleEmitter->SetSizeRange(static_cast<float>(startSize->value()),
                                    static_cast<float>(endSize->value()));
      particleEmitter->SetGravityMultiplier(
          static_cast<float>(gravity->value()));
      particleEmitter->SetBlendMode(
          static_cast<Scene::ParticleBlendMode>(blendMode->currentIndex()));
      particleEmitter->SetPromptHint(promptHint->text().toStdString());
      emit sceneModified();
    };

    connect(emissionRate, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateEmitter](double) { updateEmitter(); });
    connect(maxParticles, qOverload<int>(&QSpinBox::valueChanged), this,
            [updateEmitter](int) { updateEmitter(); });
    connect(looping, &QCheckBox::toggled, this,
            [updateEmitter](bool) { updateEmitter(); });
    connect(playOnAwake, &QCheckBox::toggled, this,
            [updateEmitter](bool) { updateEmitter(); });
    connect(minLifetime, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(maxLifetime, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(minSpeed, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(maxSpeed, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(startSize, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(endSize, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(gravity, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateEmitter](double) { updateEmitter(); });
    connect(blendMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [updateEmitter](int) { updateEmitter(); });
    connect(promptHint, &QLineEdit::editingFinished, this,
            [updateEmitter]() { updateEmitter(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Particle Emitter"), particleEmitter, formHost));
  }

  // Collider Component UI
  if (collider) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    auto *shapeTypeCombo = new QComboBox(m_content);
    shapeTypeCombo->addItem(tr("Box"), 0);
    shapeTypeCombo->addItem(tr("Sphere"), 1);
    shapeTypeCombo->addItem(tr("Capsule"), 2);
    shapeTypeCombo->setCurrentIndex(static_cast<int>(collider->GetShapeType()));

    auto halfExtents = collider->GetHalfExtents();
    auto *halfExtXSpin = makeSpin(0.001, 1000.0, 0.1);
    halfExtXSpin->setValue(halfExtents[0]);
    auto *halfExtYSpin = makeSpin(0.001, 1000.0, 0.1);
    halfExtYSpin->setValue(halfExtents[1]);
    auto *halfExtZSpin = makeSpin(0.001, 1000.0, 0.1);
    halfExtZSpin->setValue(halfExtents[2]);

    auto *radiusSpin = makeSpin(0.001, 1000.0, 0.1);
    radiusSpin->setValue(collider->GetRadius());

    auto *heightSpin = makeSpin(0.001, 1000.0, 0.1);
    heightSpin->setValue(collider->GetHeight());

    auto *triggerCheck = new QCheckBox(m_content);
    triggerCheck->setChecked(collider->IsTrigger());

    auto offset = collider->GetOffset();
    auto *offsetXSpin = makeSpin(-1000.0, 1000.0, 0.1);
    offsetXSpin->setValue(offset[0]);
    auto *offsetYSpin = makeSpin(-1000.0, 1000.0, 0.1);
    offsetYSpin->setValue(offset[1]);
    auto *offsetZSpin = makeSpin(-1000.0, 1000.0, 0.1);
    offsetZSpin->setValue(offset[2]);

    form->addRow(tr("Shape Type"), shapeTypeCombo);
    form->addRow(tr("Half Extent X"), halfExtXSpin);
    form->addRow(tr("Half Extent Y"), halfExtYSpin);
    form->addRow(tr("Half Extent Z"), halfExtZSpin);
    form->addRow(tr("Radius"), radiusSpin);
    form->addRow(tr("Height"), heightSpin);
    form->addRow(tr("Is Trigger"), triggerCheck);
    form->addRow(tr("Offset X"), offsetXSpin);
    form->addRow(tr("Offset Y"), offsetYSpin);
    form->addRow(tr("Offset Z"), offsetZSpin);

    // Update visibility based on shape type
    auto updateFieldVisibility = [shapeTypeCombo, halfExtXSpin, halfExtYSpin,
                                  halfExtZSpin, radiusSpin, heightSpin]() {
      int idx = shapeTypeCombo->currentIndex();
      bool isBox = (idx == 0);
      bool isSphere = (idx == 1);
      bool isCapsule = (idx == 2);
      halfExtXSpin->setEnabled(isBox);
      halfExtYSpin->setEnabled(isBox);
      halfExtZSpin->setEnabled(isBox);
      radiusSpin->setEnabled(isSphere || isCapsule);
      heightSpin->setEnabled(isCapsule);
    };
    updateFieldVisibility();

    auto updateCollider = [this, collider, shapeTypeCombo, halfExtXSpin,
                           halfExtYSpin, halfExtZSpin, radiusSpin, heightSpin,
                           triggerCheck, offsetXSpin, offsetYSpin, offsetZSpin,
                           updateFieldVisibility]() {
      if (m_buildingUi || !m_entity)
        return;

      collider->SetShapeType(static_cast<Scene::ColliderComponent::ShapeType>(
          shapeTypeCombo->currentData().toInt()));
      collider->SetHalfExtents(static_cast<float>(halfExtXSpin->value()),
                               static_cast<float>(halfExtYSpin->value()),
                               static_cast<float>(halfExtZSpin->value()));
      collider->SetRadius(static_cast<float>(radiusSpin->value()));
      collider->SetHeight(static_cast<float>(heightSpin->value()));
      collider->SetTrigger(triggerCheck->isChecked());
      collider->SetOffset(static_cast<float>(offsetXSpin->value()),
                          static_cast<float>(offsetYSpin->value()),
                          static_cast<float>(offsetZSpin->value()));
      updateFieldVisibility();
      emit sceneModified();
    };

    connect(shapeTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [updateCollider](int) { updateCollider(); });
    connect(halfExtXSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateCollider](double) { updateCollider(); });
    connect(halfExtYSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateCollider](double) { updateCollider(); });
    connect(halfExtZSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateCollider](double) { updateCollider(); });
    connect(radiusSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCollider](double) { updateCollider(); });
    connect(heightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCollider](double) { updateCollider(); });
    connect(triggerCheck, &QCheckBox::toggled, this,
            [updateCollider](bool) { updateCollider(); });
    connect(offsetXSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCollider](double) { updateCollider(); });
    connect(offsetYSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCollider](double) { updateCollider(); });
    connect(offsetZSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [updateCollider](double) { updateCollider(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Collider"), collider, formHost));
  }

  // Animator Component UI
  if (animator) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    auto *speedSpin = makeSpin(0.01, 10.0, 0.1);
    speedSpin->setValue(animator->GetSpeed());
    form->addRow(tr("Speed"), speedSpin);

    auto *rootMotionCheck = new QCheckBox(m_content);
    rootMotionCheck->setChecked(animator->IsRootMotionEnabled());
    form->addRow(tr("Root Motion"), rootMotionCheck);

    auto *clipCombo = new QComboBox(m_content);
    const auto &clipSources = animator->GetClipSources();
    for (const auto &entry : animator->GetClips()) {
      QString clipName = QString::fromStdString(entry.first);
      clipCombo->addItem(clipName);
      auto sourceIt = clipSources.find(entry.first);
      if (sourceIt != clipSources.end() && !sourceIt->second.empty()) {
        clipCombo->setItemData(
            clipCombo->count() - 1,
            QString::fromStdString(sourceIt->second), Qt::ToolTipRole);
      }
    }

    auto *clipRow = new QWidget(m_content);
    auto *clipLayout = new QHBoxLayout(clipRow);
    clipLayout->setContentsMargins(0, 0, 0, 0);
    clipLayout->setSpacing(4);

    auto *addClipBtn = new QPushButton(tr("Add"), m_content);
    auto *removeClipBtn = new QPushButton(tr("Remove"), m_content);

    clipLayout->addWidget(clipCombo, 1);
    clipLayout->addWidget(addClipBtn);
    clipLayout->addWidget(removeClipBtn);
    form->addRow(tr("Clips"), clipRow);

    auto applyAnimator = [this, animator, speedSpin, rootMotionCheck]() {
      if (m_buildingUi || !m_entity) {
        return;
      }
      animator->SetSpeed(static_cast<float>(speedSpin->value()));
      animator->SetRootMotionEnabled(rootMotionCheck->isChecked());
      emit sceneModified();
    };

    connect(speedSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyAnimator](double) { applyAnimator(); });
    connect(rootMotionCheck, &QCheckBox::toggled, this,
            [applyAnimator](bool) { applyAnimator(); });

    connect(addClipBtn, &QPushButton::clicked, this,
            [this, animator]() {
              const QString filter =
                  tr("Animation (*.anim.json);;JSON (*.json);;All Files (*)");
              QString selected = QFileDialog::getOpenFileName(
                  this, tr("Load Animation Clip"), QString(), filter);
              if (selected.isEmpty()) {
                return;
              }

              if (!animator->AddClipFromFile(std::string(),
                                             selected.toStdString())) {
                QMessageBox::warning(
                    this, tr("Animation Load Failed"),
                    tr("Failed to load animation:\n%1").arg(selected));
                return;
              }

              emit sceneModified();
              RebuildUi();
            });

    connect(removeClipBtn, &QPushButton::clicked, this,
            [this, animator, clipCombo]() {
              const int index = clipCombo->currentIndex();
              if (index < 0) {
                return;
              }
              const QString clipName = clipCombo->itemText(index);
              if (clipName.isEmpty()) {
                return;
              }

              animator->RemoveClip(clipName.toStdString());
              emit sceneModified();
              RebuildUi();
            });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Animator"), animator, formHost));
  }

  // Audio Source Component UI
  if (audioSource) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    m_audioPath = new QLineEdit(m_content);
    m_audioPath->setText(QString::fromStdString(audioSource->GetSoundPath()));

    m_audioVolume = makeSpin(0.0, 10.0, 0.1);
    m_audioVolume->setValue(audioSource->GetVolume());

    m_audioPitch = makeSpin(0.1, 5.0, 0.1);
    m_audioPitch->setValue(audioSource->GetPitch());

    m_audioLoop = new QCheckBox(m_content);
    m_audioLoop->setChecked(audioSource->GetLoop());

    m_audioSpatial = new QCheckBox(m_content);
    m_audioSpatial->setChecked(audioSource->GetSpatial());

    m_audioPlayOnAwake = new QCheckBox(m_content);
    m_audioPlayOnAwake->setChecked(audioSource->GetPlayOnAwake());

    form->addRow(tr("Sound Path"), m_audioPath);
    form->addRow(tr("Volume"), m_audioVolume);
    form->addRow(tr("Pitch"), m_audioPitch);
    form->addRow(tr("Loop"), m_audioLoop);
    form->addRow(tr("Spatial"), m_audioSpatial);
    form->addRow(tr("Play On Awake"), m_audioPlayOnAwake);

    auto *playBtn = new QPushButton(tr("Test Play"), formHost);
    form->addRow(playBtn);

    auto updateAudio = [this, audioSource]() {
      if (m_buildingUi || !m_entity)
        return;

      audioSource->SetSoundPath(m_audioPath->text().toStdString());
      audioSource->SetVolume(static_cast<float>(m_audioVolume->value()));
      audioSource->SetPitch(static_cast<float>(m_audioPitch->value()));
      audioSource->SetLoop(m_audioLoop->isChecked());
      audioSource->SetSpatial(m_audioSpatial->isChecked());
      audioSource->SetPlayOnAwake(m_audioPlayOnAwake->isChecked());
      emit sceneModified();
    };

    connect(m_audioPath, &QLineEdit::editingFinished, this,
            [updateAudio]() { updateAudio(); });
    connect(m_audioVolume, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateAudio](double) { updateAudio(); });
    connect(m_audioPitch, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [updateAudio](double) { updateAudio(); });
    connect(m_audioLoop, &QCheckBox::toggled, this,
            [updateAudio](bool) { updateAudio(); });
    connect(m_audioSpatial, &QCheckBox::toggled, this,
            [updateAudio](bool) { updateAudio(); });
    connect(m_audioPlayOnAwake, &QCheckBox::toggled, this,
            [updateAudio](bool) { updateAudio(); });

    connect(playBtn, &QPushButton::clicked, this,
            [audioSource]() { audioSource->Play(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Audio Source"), audioSource, formHost));
  }

  // ==================== Script Component ====================
  auto scriptComp = m_entity->GetComponent<Scene::ScriptComponent>();
  if (scriptComp) {
    auto *formHost = new QWidget(m_content);
    auto *form = new QFormLayout();
    form->setContentsMargins(10, 10, 10, 10);

    // Script asset ID
    std::string scriptAssetValue = scriptComp->GetScriptAssetId();
    std::string scriptCodeValue = scriptComp->GetScriptSource();
    if (scriptComp->GetSourceMode() ==
            Scene::ScriptComponent::SourceMode::FileReference &&
        scriptAssetValue.empty() && !scriptCodeValue.empty()) {
      scriptAssetValue = scriptCodeValue;
      scriptCodeValue.clear();
    }

    auto *scriptAssetEdit = new QLineEdit(formHost);
    scriptAssetEdit->setText(QString::fromStdString(scriptAssetValue));
    scriptAssetEdit->setPlaceholderText(tr("scripts/my_script.lua"));
    form->addRow(tr("Script Asset:"), scriptAssetEdit);

    // Script source code editor
    auto *codeLabel = new QLabel(tr("Script Code:"), formHost);
    form->addRow(codeLabel);

    auto *codeEditor = new QTextEdit(formHost);
    codeEditor->setPlainText(QString::fromStdString(scriptCodeValue));
    codeEditor->setMinimumHeight(200);
    codeEditor->setFont(QFont("Consolas", 10));
    codeEditor->setStyleSheet(
        "QTextEdit { background-color: #0f131b; color: #f4f3ef; "
        "border: 1px solid #2a3140; border-radius: 6px; "
        "font-family: Consolas, 'Courier New', monospace; }");
    codeEditor->setTabStopDistance(QFontMetricsF(codeEditor->font()).horizontalAdvance(' ') * 4);
    codeEditor->setLineWrapMode(QTextEdit::NoWrap);
    form->addRow(codeEditor);

    // Apply button
    auto *applyBtn = new QPushButton(tr("Apply Script"), formHost);
    applyBtn->setStyleSheet(
        "QPushButton { background-color: #ff6b3d; color: #141824; "
        "border-radius: 6px; padding: 6px 12px; font-weight: 600; "
        "border: 1px solid #ff8b66; }"
        "QPushButton:hover { background-color: #ff7b52; }"
        "QPushButton:pressed { background-color: #e3572f; }");
    form->addRow(applyBtn);

    auto updateScriptEditorState = [scriptComp, scriptAssetEdit, codeEditor]() {
      const bool fileBacked =
          scriptComp->GetSourceMode() ==
              Scene::ScriptComponent::SourceMode::FileReference ||
          !scriptAssetEdit->text().trimmed().isEmpty();
      codeEditor->setEnabled(!fileBacked);
      if (fileBacked) {
        codeEditor->setPlaceholderText(
            QObject::tr("File-backed scripts are edited in the referenced asset."));
      } else {
        codeEditor->setPlaceholderText(
            QObject::tr("Enter inline Lua source here."));
      }
    };

    auto applyScriptSelection = [this, scriptComp, codeEditor, scriptAssetEdit]() {
      if (m_buildingUi || !m_entity) {
        return;
      }

      const std::string assetValue =
          scriptAssetEdit->text().trimmed().toStdString();
      const std::string codeValue = codeEditor->toPlainText().toStdString();

      if (!assetValue.empty()) {
        scriptComp->SetSourceMode(Scene::ScriptComponent::SourceMode::FileReference);
        scriptComp->SetScriptAssetId(assetValue);
        scriptComp->SetScriptSource({});
      } else {
        scriptComp->SetSourceMode(Scene::ScriptComponent::SourceMode::InlineCode);
        scriptComp->SetScriptAssetId({});
        scriptComp->SetScriptSource(codeValue);
      }

      emit sceneModified();
    };

    updateScriptEditorState();

    connect(scriptAssetEdit, &QLineEdit::editingFinished, this,
            [applyScriptSelection]() { applyScriptSelection(); });
    connect(scriptAssetEdit, &QLineEdit::textChanged, this,
            [updateScriptEditorState](const QString &) {
              updateScriptEditorState();
            });

    connect(applyBtn, &QPushButton::clicked, this,
            [applyScriptSelection]() { applyScriptSelection(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(
        makeComponentHeader(tr("Script"), scriptComp, formHost));
  }

  m_contentLayout->addStretch(1);

  auto *addCompBtn = new QPushButton(tr("Add Component"), m_content);
  connect(addCompBtn, &QPushButton::clicked, this, [this] {
    if (!m_entity)
      return;

    QMenu menu;
    if (!m_entity->GetComponent<Scene::TransformComponent>()) {
      menu.addAction(tr("Transform"), [this] {
        auto comp = std::make_shared<Scene::TransformComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::MeshRendererComponent>()) {
      menu.addAction(tr("Mesh Renderer"), [this] {
        auto comp = std::make_shared<Scene::MeshRendererComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::LightComponent>()) {
      menu.addAction(tr("Light"), [this] {
        auto comp = std::make_shared<Scene::LightComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::CameraComponent>()) {
      menu.addAction(tr("Camera"), [this] {
        auto comp = std::make_shared<Scene::CameraComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::RigidbodyComponent>()) {
      menu.addAction(tr("Rigidbody"), [this] {
        auto comp = std::make_shared<Scene::RigidbodyComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::ColliderComponent>()) {
      menu.addAction(tr("Collider"), [this] {
        auto comp = std::make_shared<Scene::ColliderComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::AnimatorComponent>()) {
      menu.addAction(tr("Animator"), [this] {
        auto comp = std::make_shared<Scene::AnimatorComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::AudioSourceComponent>()) {
      menu.addAction(tr("Audio Source"), [this] {
        auto comp = std::make_shared<Scene::AudioSourceComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::AIBehaviorComponent>()) {
      menu.addAction(tr("AI Behavior"), [this] {
        auto comp = std::make_shared<Scene::AIBehaviorComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::ParticleEmitterComponent>()) {
      menu.addAction(tr("Particle Emitter"), [this] {
        auto comp = std::make_shared<Scene::ParticleEmitterComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
      });
    }
    if (!m_entity->GetComponent<Scene::ScriptComponent>()) {
      menu.addAction(tr("Script"), [this] {
        auto comp = std::make_shared<Scene::ScriptComponent>();
        if (m_commandExecutor)
          m_commandExecutor(
              std::make_unique<AddComponentCommand>(m_entity, comp));
        RebuildUi();
      });
    }

    if (!menu.isEmpty()) {
      menu.exec(QCursor::pos());
    }
  });
  m_contentLayout->addWidget(addCompBtn);

  m_buildingUi = false;

  if (transform) {
    // Push initial values out to listeners (renderer).
    emit transformChanged(m_entity->GetId(),
                          static_cast<float>(m_posX->value()),
                          static_cast<float>(m_posY->value()),
                          static_cast<float>(m_posZ->value()),
                          static_cast<float>(m_rotX->value()),
                          static_cast<float>(m_rotY->value()),
                          static_cast<float>(m_rotZ->value()),
                          static_cast<float>(m_scaleX->value()),
                          static_cast<float>(m_scaleY->value()),
                          static_cast<float>(m_scaleZ->value()));
  }
}
} // namespace Aetherion::Editor
