#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

#include "Aetherion/Core/Types.h"

class QTreeWidgetItem;
class QMenu;

namespace Aetherion::Scene
{
class Scene;
} // namespace Aetherion::Scene

namespace Aetherion::Editor
{
class EditorSelection;
class HierarchyTreeWidget;

class EditorHierarchyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorHierarchyPanel(QWidget* parent = nullptr);
    ~EditorHierarchyPanel() override = default;

    void BindScene(std::shared_ptr<Scene::Scene> scene);
    void SetSelectionModel(EditorSelection* selection);
    void SetSelectedEntity(Aetherion::Core::EntityId id);
    Aetherion::Core::EntityId GetSelectedEntityId() const;

signals:
    void entitySelected(Aetherion::Core::EntityId id);
    void entityActivated(Aetherion::Core::EntityId id);
    void entityReparentRequested(Aetherion::Core::EntityId childId, Aetherion::Core::EntityId newParentId);
    void entityDeleteRequested(Aetherion::Core::EntityId id);
    void entityDuplicateRequested(Aetherion::Core::EntityId id);
    void entityRenameRequested(Aetherion::Core::EntityId id);
    void createEmptyEntityRequested(Aetherion::Core::EntityId parentId);
    void createEmptyEntityAtRootRequested();
    void createLightEntityRequested(Aetherion::Core::EntityId parentId);
    void createCameraEntityRequested(Aetherion::Core::EntityId parentId);
    void createMeshEntityRequested(Aetherion::Core::EntityId parentId,
                                   const QString& meshAssetId,
                                   const QString& displayName);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void showContextMenu(const QPoint& pos);

private:
    void setupContextMenu();

    HierarchyTreeWidget* m_tree = nullptr;
    QMenu* m_contextMenu = nullptr;
    std::shared_ptr<Scene::Scene> m_scene;
    EditorSelection* m_selection = nullptr;
    QHash<qulonglong, QTreeWidgetItem*> m_itemLookup;
    bool m_updatingSelection = false;
};
} // namespace Aetherion::Editor
