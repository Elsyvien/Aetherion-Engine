#include "Aetherion/Editor/EditorInspectorPanel.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Editor/Commands/ComponentCommands.h"
#include "Aetherion/Editor/Commands/TransformCommand.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/ColliderComponent.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/RigidbodyComponent.h"
#include "Aetherion/Scene/TransformComponent.h"
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
        "QPushButton { text-align: left; font-weight: bold; padding: 5px; "
        "background-color: #404040; border: none; } QPushButton:checked { "
        "background-color: #505050; }");

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
  case AssetType::Scene:
    return "Scene";
  case AssetType::Shader:
    return "Shader";
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
            form->addRow(
                tr("Materials"),
                new QLabel(QString::number(static_cast<long long>(
                              cachedMesh->materialIds.size())),
                           formHost));
            if (!cachedMesh->materialIds.empty()) {
              auto *materialLabel =
                  new QLabel(FormatIdList(cachedMesh->materialIds), formHost);
              materialLabel->setWordWrap(true);
              form->addRow(tr("Material IDs"), materialLabel);
            }

            form->addRow(
                tr("Textures"),
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

          auto applyImportSettings =
              [this, meshId, importScale, importNormals, importTangents,
               importFlipUvs, importFlipWinding, importOptimize, importCenter]() {
                if (!m_assetRegistry) {
                  return;
                }

                Assets::AssetRegistry::MeshImportSettings settings{};
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
                  this, [applyImportSettings](double) {
                    applyImportSettings();
                  });
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

  if (!transform && !mesh && !light && !camera && !rigidbody && !collider) {
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
        "background-color: #353535; border-radius: 4px;");

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
        "QPushButton { border-radius: 10px; color: #aaa; } QPushButton:hover { "
        "background-color: #c00; color: white; }");
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

  auto makeVectorRow = [this](const QString& label, QDoubleSpinBox* x, QDoubleSpinBox* y, QDoubleSpinBox* z, float resetVal) {
      auto* container = new QWidget(m_content);
      auto* layout = new QHBoxLayout(container);
      layout->setContentsMargins(0, 0, 0, 0);
      layout->setSpacing(2);

      layout->addWidget(new QLabel("X", container));
      layout->addWidget(x, 1);
      layout->addWidget(new QLabel("Y", container));
      layout->addWidget(y, 1);
      layout->addWidget(new QLabel("Z", container));
      layout->addWidget(z, 1);

      auto* resetBtn = new QToolButton(container);
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

    form->addRow(tr("Position"), makeVectorRow(tr("Position"), m_posX, m_posY, m_posZ, 0.0f));
    form->addRow(tr("Rotation"), makeVectorRow(tr("Rotation"), m_rotX, m_rotY, m_rotZ, 0.0f));
    form->addRow(tr("Scale"), makeVectorRow(tr("Scale"), m_scaleX, m_scaleY, m_scaleZ, 1.0f));

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
        QString::fromStdString(mesh->GetAlbedoTextureId());
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

      const auto *entry = m_assetRegistry->FindEntry(normalized.toStdString());
      if (!entry) {
        setPreviewText(tr("Not found"));
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
        mesh->SetAlbedoTextureId(normalized.toStdString());
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
    hintLabel->setStyleSheet("color: #8a8a8a;");
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
