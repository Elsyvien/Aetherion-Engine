#pragma once

#include <QWidget>

#include <memory>

#include <QString>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Entity;
} // namespace Aetherion::Scene
namespace Aetherion::Assets
{
class AssetRegistry;
} // namespace Aetherion::Assets

class QScrollArea;
class QVBoxLayout;
class QDoubleSpinBox;
class QWidget;
class QComboBox;
class QCheckBox;

namespace Aetherion::Editor
{
class EditorInspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorInspectorPanel(QWidget* parent = nullptr);
    ~EditorInspectorPanel() override = default;

    void SetSelectedEntity(std::shared_ptr<Scene::Entity> entity);
    void SetSelectedAsset(QString assetId);
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);

signals:
    void transformChanged(Aetherion::Core::EntityId entityId,
                          float posX,
                          float posY,
                          float posZ,
                          float rotDegX,
                          float rotDegY,
                          float rotDegZ,
                          float scaleX,
                          float scaleY,
                          float scaleZ);
    void sceneModified();

private:
    void RebuildUi();

    std::shared_ptr<Scene::Entity> m_entity;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    QString m_assetId;
    bool m_showingAsset = false;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;

    QDoubleSpinBox* m_posX = nullptr;
    QDoubleSpinBox* m_posY = nullptr;
    QDoubleSpinBox* m_posZ = nullptr;
    QDoubleSpinBox* m_rotX = nullptr;
    QDoubleSpinBox* m_rotY = nullptr;
    QDoubleSpinBox* m_rotZ = nullptr;
    QDoubleSpinBox* m_scaleX = nullptr;
    QDoubleSpinBox* m_scaleY = nullptr;
    QDoubleSpinBox* m_scaleZ = nullptr;
    QDoubleSpinBox* m_colorR = nullptr;
    QDoubleSpinBox* m_colorG = nullptr;
    QDoubleSpinBox* m_colorB = nullptr;
    QDoubleSpinBox* m_meshRotationSpeed = nullptr;
    QComboBox* m_meshAsset = nullptr;
    QComboBox* m_meshTexture = nullptr;
    QCheckBox* m_lightEnabled = nullptr;
    QDoubleSpinBox* m_lightColorR = nullptr;
    QDoubleSpinBox* m_lightColorG = nullptr;
    QDoubleSpinBox* m_lightColorB = nullptr;
    QDoubleSpinBox* m_lightIntensity = nullptr;
    QDoubleSpinBox* m_lightAmbientR = nullptr;
    QDoubleSpinBox* m_lightAmbientG = nullptr;
    QDoubleSpinBox* m_lightAmbientB = nullptr;

    bool m_buildingUi = false;
};
} // namespace Aetherion::Editor
