#include "Aetherion/Editor/EditorInspectorPanel.h"

#include <QLabel>
#include <algorithm>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace
{
QString FormatBytes(long long bytes)
{
    if (bytes < 0)
    {
        return {};
    }
    static const char* units[] = {"B", "KB", "MB", "GB"};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 3)
    {
        value /= 1024.0;
        ++unitIndex;
    }
    return QString::number(value, 'f', unitIndex == 0 ? 0 : 1) + " " + units[unitIndex];
}

QString AssetTypeLabel(Aetherion::Assets::AssetRegistry::AssetType type)
{
    using AssetType = Aetherion::Assets::AssetRegistry::AssetType;
    switch (type)
    {
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

namespace Aetherion::Editor
{
EditorInspectorPanel::EditorInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Inspector"), this);

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

void EditorInspectorPanel::SetSelectedEntity(std::shared_ptr<Scene::Entity> entity)
{
    m_entity = std::move(entity);
    m_showingAsset = false;
    m_assetId.clear();
    RebuildUi();
}

void EditorInspectorPanel::SetSelectedAsset(QString assetId)
{
    m_entity.reset();
    m_showingAsset = true;
    m_assetId = std::move(assetId);
    RebuildUi();
}

void EditorInspectorPanel::SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry)
{
    m_assetRegistry = std::move(registry);
    if (m_showingAsset || m_entity)
    {
        RebuildUi();
    }
}

void EditorInspectorPanel::RebuildUi()
{
    if (!m_contentLayout)
    {
        return;
    }

    m_buildingUi = true;

    while (auto* item = m_contentLayout->takeAt(0))
    {
        if (auto* w = item->widget())
        {
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

    if (!m_entity)
    {
        if (m_showingAsset)
        {
            const QString displayName = m_assetId.trimmed().isEmpty() ? tr("Asset") : m_assetId.trimmed();

            auto* title = new QLabel(displayName, m_content);
            title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
            m_contentLayout->addWidget(title);

            auto* formHost = new QWidget(m_content);
            auto* form = new QFormLayout(formHost);
            form->setLabelAlignment(Qt::AlignLeft);

            const bool isFolder = displayName.endsWith('/');
            const QString type = isFolder ? tr("Folder") : tr("Asset");
            form->addRow(tr("Type"), new QLabel(type, formHost));

            const QString normalized = isFolder ? displayName.left(displayName.size() - 1) : displayName;
            form->addRow(tr("Id"), new QLabel(normalized, formHost));

            const auto registry = m_assetRegistry;
            const Assets::AssetRegistry::AssetEntry* entry = nullptr;
            QLabel* previewLabel = nullptr;
            if (!isFolder && registry)
            {
                const std::string id = normalized.toStdString();
                entry = registry->FindEntry(id);
            }

            if (isFolder)
            {
                form->addRow(tr("Status"), new QLabel(tr("Category"), formHost));
            }
            else if (entry)
            {
                const QString pathLabel = QString::fromStdString(entry->path.generic_string());
                form->addRow(tr("Category"), new QLabel(AssetTypeLabel(entry->type), formHost));
                form->addRow(tr("Path"), new QLabel(pathLabel, formHost));

                QFileInfo fileInfo(QString::fromStdString(entry->path.string()));
                if (fileInfo.exists())
                {
                    form->addRow(tr("Size"), new QLabel(FormatBytes(fileInfo.size()), formHost));
                    form->addRow(tr("Modified"), new QLabel(fileInfo.lastModified().toString(Qt::ISODate), formHost));
                }
                form->addRow(tr("Status"), new QLabel(tr("Registered"), formHost));

                if (entry->type == Assets::AssetRegistry::AssetType::Texture && fileInfo.exists())
                {
                    QImageReader reader(fileInfo.absoluteFilePath());
                    reader.setAutoTransform(true);
                    const QSize imageSize = reader.size();
                    if (imageSize.isValid())
                    {
                        form->addRow(tr("Dimensions"),
                                     new QLabel(tr("%1 x %2").arg(imageSize.width()).arg(imageSize.height()), formHost));
                    }

                    const QImage image = reader.read();
                    if (!image.isNull())
                    {
                        const int previewMax = 256;
                        const QPixmap preview =
                            QPixmap::fromImage(image)
                                .scaled(previewMax, previewMax, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        previewLabel = new QLabel(m_content);
                        previewLabel->setPixmap(preview);
                        previewLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
                    }
                }

                if (registry && entry->type == Assets::AssetRegistry::AssetType::Mesh)
                {
                    const auto* meshData = registry->LoadMeshData(entry->id);
                    if (meshData)
                    {
                        form->addRow(tr("Geometry"), new QLabel(tr("Loaded"), formHost));
                        form->addRow(tr("Vertices"),
                                     new QLabel(QString::number(static_cast<long long>(meshData->positions.size())),
                                                formHost));
                        form->addRow(tr("Indices"),
                                     new QLabel(QString::number(static_cast<long long>(meshData->indices.size())),
                                                formHost));
                    }
                    else
                    {
                        form->addRow(tr("Geometry"), new QLabel(tr("Not loaded"), formHost));
                    }
                }
            }
            else
            {
                const QString status = registry ? tr("Not found in registry") : tr("Asset registry unavailable");
                form->addRow(tr("Status"), new QLabel(status, formHost));
            }

            formHost->setLayout(form);
            m_contentLayout->addWidget(formHost);
            if (previewLabel)
            {
                m_contentLayout->addWidget(previewLabel);
            }
            m_contentLayout->addStretch(1);
            m_buildingUi = false;
            return;
        }

        auto* placeholder = new QLabel(tr("Select an entity to view details"), m_content);
        placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_contentLayout->addWidget(placeholder);
        m_contentLayout->addStretch(1);
        m_buildingUi = false;
        return;
    }

    auto* title = new QLabel(QString::fromStdString(m_entity->GetName().empty() ? std::string("Entity") : m_entity->GetName()), m_content);
    title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_contentLayout->addWidget(title);

    auto transform = m_entity->GetComponent<Scene::TransformComponent>();
    auto mesh = m_entity->GetComponent<Scene::MeshRendererComponent>();

    if (!transform && !mesh)
    {
        auto* noEditable = new QLabel(tr("No editable components on selected entity."), m_content);
        noEditable->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_contentLayout->addWidget(noEditable);
        m_contentLayout->addStretch(1);
        m_buildingUi = false;
        return;
    }

    auto makeSpin = [this](double min, double max, double step) {
        auto* s = new QDoubleSpinBox(m_content);
        s->setRange(min, max);
        s->setSingleStep(step);
        s->setDecimals(3);
        return s;
    };

    if (transform)
    {
        auto* transformLabel = new QLabel(tr("Transform"), m_content);
        transformLabel->setStyleSheet("font-weight: bold;");
        m_contentLayout->addWidget(transformLabel);

        auto* formHost = new QWidget(m_content);
        auto* form = new QFormLayout(formHost);
        form->setLabelAlignment(Qt::AlignLeft);

        m_posX = makeSpin(-10.0, 10.0, 0.01);
        m_posY = makeSpin(-10.0, 10.0, 0.01);
        m_rotZ = makeSpin(-180.0, 180.0, 1.0);
        m_scaleX = makeSpin(0.001, 10.0, 0.01);
        m_scaleY = makeSpin(0.001, 10.0, 0.01);

        m_posX->setValue(transform->GetPositionX());
        m_posY->setValue(transform->GetPositionY());
        m_rotZ->setValue(transform->GetRotationZDegrees());
        m_scaleX->setValue(transform->GetScaleX());
        m_scaleY->setValue(transform->GetScaleY());

        form->addRow(tr("Position X"), m_posX);
        form->addRow(tr("Position Y"), m_posY);
        form->addRow(tr("Rotation Z (deg)"), m_rotZ);
        form->addRow(tr("Scale X"), m_scaleX);
        form->addRow(tr("Scale Y"), m_scaleY);

        auto applyAndEmit = [this, transform]() {
            if (m_buildingUi || !m_entity)
            {
                return;
            }

            transform->SetPosition(static_cast<float>(m_posX->value()), static_cast<float>(m_posY->value()));
            transform->SetRotationZDegrees(static_cast<float>(m_rotZ->value()));
            transform->SetScale(static_cast<float>(m_scaleX->value()), static_cast<float>(m_scaleY->value()));

            emit transformChanged(m_entity->GetId(),
                                  static_cast<float>(m_posX->value()),
                                  static_cast<float>(m_posY->value()),
                                  static_cast<float>(m_rotZ->value()),
                                  static_cast<float>(m_scaleX->value()),
                                  static_cast<float>(m_scaleY->value()));
            emit sceneModified();
        };

        connect(m_posX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
        connect(m_posY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
        connect(m_rotZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
        connect(m_scaleX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
        connect(m_scaleY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });

        formHost->setLayout(form);
        m_contentLayout->addWidget(formHost);
    }

    if (mesh)
    {
        auto* meshLabel = new QLabel(tr("Mesh Renderer"), m_content);
        meshLabel->setStyleSheet("font-weight: bold;");
        m_contentLayout->addWidget(meshLabel);

        auto* formHost = new QWidget(m_content);
        auto* form = new QFormLayout(formHost);
        form->setLabelAlignment(Qt::AlignLeft);

        m_colorR = makeSpin(0.0, 1.0, 0.01);
        m_colorG = makeSpin(0.0, 1.0, 0.01);
        m_colorB = makeSpin(0.0, 1.0, 0.01);
        m_meshRotationSpeed = makeSpin(-720.0, 720.0, 1.0);
        m_meshAsset = new QComboBox(m_content);
        m_meshAsset->setEditable(true);
        m_meshAsset->setInsertPolicy(QComboBox::NoInsert);

        auto color = mesh->GetColor();
        m_colorR->setValue(color[0]);
        m_colorG->setValue(color[1]);
        m_colorB->setValue(color[2]);
        m_meshRotationSpeed->setValue(mesh->GetRotationSpeedDegPerSec());

        form->addRow(tr("Color R"), m_colorR);
        form->addRow(tr("Color G"), m_colorG);
        form->addRow(tr("Color B"), m_colorB);
        form->addRow(tr("Rotation Speed (deg/s)"), m_meshRotationSpeed);
        form->addRow(tr("Mesh Asset"), m_meshAsset);

        m_meshAsset->addItem(tr("(None)"), QString());
        if (m_assetRegistry)
        {
            for (const auto& entry : m_assetRegistry->GetEntries())
            {
                if (entry.type != Assets::AssetRegistry::AssetType::Mesh)
                {
                    continue;
                }
                const QString id = QString::fromStdString(entry.id);
                m_meshAsset->addItem(id, id);
            }
        }

        const QString currentMeshId = QString::fromStdString(mesh->GetMeshAssetId());
        const int meshIndex = m_meshAsset->findData(currentMeshId);
        if (meshIndex >= 0)
        {
            m_meshAsset->setCurrentIndex(meshIndex);
        }
        else if (!currentMeshId.isEmpty())
        {
            m_meshAsset->addItem(currentMeshId, currentMeshId);
            m_meshAsset->setCurrentIndex(m_meshAsset->count() - 1);
        }

        auto updateMesh = [this, mesh]() {
            if (m_buildingUi || !m_entity)
            {
                return;
            }

            mesh->SetColor(static_cast<float>(m_colorR->value()),
                           static_cast<float>(m_colorG->value()),
                           static_cast<float>(m_colorB->value()));
            mesh->SetRotationSpeedDegPerSec(static_cast<float>(m_meshRotationSpeed->value()));
            if (m_meshAsset)
            {
                const QString meshId = m_meshAsset->currentText().trimmed();
                const QString normalized = (meshId == tr("(None)")) ? QString() : meshId;
                mesh->SetMeshAssetId(normalized.toStdString());
            }
            emit sceneModified();
        };

        connect(m_colorR, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [updateMesh](double) { updateMesh(); });
        connect(m_colorG, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [updateMesh](double) { updateMesh(); });
        connect(m_colorB, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [updateMesh](double) { updateMesh(); });
        connect(m_meshRotationSpeed, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [updateMesh](double) { updateMesh(); });
        if (m_meshAsset)
        {
            connect(m_meshAsset, &QComboBox::currentTextChanged, this, [updateMesh](const QString&) { updateMesh(); });
        }

        formHost->setLayout(form);
        m_contentLayout->addWidget(formHost);
    }

    m_contentLayout->addStretch(1);
    m_buildingUi = false;

    if (transform)
    {
        // Push initial values out to listeners (renderer).
        emit transformChanged(m_entity->GetId(),
                              static_cast<float>(m_posX->value()),
                              static_cast<float>(m_posY->value()),
                              static_cast<float>(m_rotZ->value()),
                              static_cast<float>(m_scaleX->value()),
                              static_cast<float>(m_scaleY->value()));
    }
}
} // namespace Aetherion::Editor
