#pragma once

#include <QWidget>

#include "Aetherion/Core/Types.h"

class QTreeWidget;

namespace Aetherion::Scene
{
class Scene;
} // namespace Aetherion::Scene

namespace Aetherion::Editor
{
class EditorHierarchyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorHierarchyPanel(QWidget* parent = nullptr);
    ~EditorHierarchyPanel() override = default;

    void BindScene(std::shared_ptr<Scene::Scene> scene);

signals:
    void entitySelected(Aetherion::Core::EntityId id);

private:
    QTreeWidget* m_tree = nullptr;
    std::shared_ptr<Scene::Scene> m_scene;
};
} // namespace Aetherion::Editor
