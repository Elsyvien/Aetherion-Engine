#pragma once

#include <QHash>
#include <QWidget>

#include "Aetherion/Core/Types.h"

class QTreeWidgetItem;

class HierarchyTreeWidget;

namespace Aetherion::Scene
{
class Scene;
} // namespace Aetherion::Scene

namespace Aetherion::Editor
{
class EditorSelection;

class EditorHierarchyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorHierarchyPanel(QWidget* parent = nullptr);
    ~EditorHierarchyPanel() override = default;

    void BindScene(std::shared_ptr<Scene::Scene> scene);
    void SetSelectionModel(EditorSelection* selection);
    void SetSelectedEntity(Aetherion::Core::EntityId id);

signals:
    void entitySelected(Aetherion::Core::EntityId id);
    void entityActivated(Aetherion::Core::EntityId id);
    void entityReparentRequested(Aetherion::Core::EntityId childId, Aetherion::Core::EntityId newParentId);

private:
    HierarchyTreeWidget* m_tree = nullptr;
    std::shared_ptr<Scene::Scene> m_scene;
    EditorSelection* m_selection = nullptr;
    QHash<qulonglong, QTreeWidgetItem*> m_itemLookup;
    bool m_updatingSelection = false;
};
} // namespace Aetherion::Editor
