#pragma once

#include <QWidget>

#include <memory>

#include "Aetherion/Core/Types.h"

namespace Aetherion::Scene
{
class Entity;
} // namespace Aetherion::Scene

class QScrollArea;
class QVBoxLayout;
class QDoubleSpinBox;
class QWidget;

namespace Aetherion::Editor
{
class EditorInspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorInspectorPanel(QWidget* parent = nullptr);
    ~EditorInspectorPanel() override = default;

    void SetSelectedEntity(std::shared_ptr<Scene::Entity> entity);

signals:
    void transformChanged(Aetherion::Core::EntityId entityId, float posX, float posY, float rotDegZ, float scaleX, float scaleY);

private:
    void RebuildUi();

    std::shared_ptr<Scene::Entity> m_entity;

    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;

    QDoubleSpinBox* m_posX = nullptr;
    QDoubleSpinBox* m_posY = nullptr;
    QDoubleSpinBox* m_rotZ = nullptr;
    QDoubleSpinBox* m_scaleX = nullptr;
    QDoubleSpinBox* m_scaleY = nullptr;

    bool m_buildingUi = false;
};
} // namespace Aetherion::Editor
