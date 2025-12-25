#include "Aetherion/Editor/EditorHierarchyPanel.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <array>

#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/LightComponent.h"
#include "Aetherion/Scene/CameraComponent.h"
#include "Aetherion/Scene/MeshRendererComponent.h"
#include "Aetherion/Scene/Scene.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Editor
{
class HierarchyTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit HierarchyTreeWidget(QWidget* parent = nullptr)
        : QTreeWidget(parent)
    {
        setHeaderHidden(true);
        setIndentation(14);
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDefaultDropAction(Qt::MoveAction);
        setDragDropMode(QAbstractItemView::InternalMove);
    }

signals:
    void ReparentRequested(qulonglong childId, qulonglong newParentId);

protected:
    void dropEvent(QDropEvent* event) override
    {
        QTreeWidgetItem* dragged = currentItem();
        const qulonglong childId = dragged ? dragged->data(0, Qt::UserRole).toULongLong() : 0;

        QTreeWidgetItem* dropTarget = itemAt(event->position().toPoint());
        QTreeWidgetItem* newParentItem = nullptr;

        switch (dropIndicatorPosition())
        {
        case QAbstractItemView::OnItem:
            newParentItem = dropTarget;
            break;
        case QAbstractItemView::AboveItem:
        case QAbstractItemView::BelowItem:
            newParentItem = dropTarget ? dropTarget->parent() : nullptr;
            break;
        default:
            newParentItem = nullptr;
            break;
        }

        qulonglong newParentId = newParentItem ? newParentItem->data(0, Qt::UserRole).toULongLong() : 0;

        QTreeWidget::dropEvent(event);

        if (childId != 0 && childId != newParentId)
        {
            emit ReparentRequested(childId, newParentId);
        }
    }
};

EditorHierarchyPanel::EditorHierarchyPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Hierarchy"), this);
    m_tree = new HierarchyTreeWidget(this);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    layout->addWidget(header);
    layout->addWidget(m_tree, 1);
    setLayout(layout);

    setupContextMenu();

    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &EditorHierarchyPanel::showContextMenu);

    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
        if (!current)
        {
            return;
        }

        const QVariant idData = current->data(0, Qt::UserRole);
        if (!idData.isValid())
        {
            return;
        }

        const auto id = static_cast<Aetherion::Core::EntityId>(idData.toULongLong());
        if (m_selection)
        {
            m_updatingSelection = true;
            m_selection->SelectEntityById(id);
            m_updatingSelection = false;
        }

        emit entitySelected(id);
    });

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item)
        {
            return;
        }

        const QVariant idData = item->data(0, Qt::UserRole);
        if (!idData.isValid())
        {
            return;
        }

        const auto id = static_cast<Aetherion::Core::EntityId>(idData.toULongLong());
        emit entityActivated(id);
    });

    connect(m_tree, &HierarchyTreeWidget::ReparentRequested, this, [this](qulonglong childId, qulonglong newParentId) {
        emit entityReparentRequested(static_cast<Aetherion::Core::EntityId>(childId),
                                     static_cast<Aetherion::Core::EntityId>(newParentId));
    });
}

void EditorHierarchyPanel::BindScene(std::shared_ptr<Scene::Scene> scene)
{
    m_scene = std::move(scene);
    m_itemLookup.clear();

    if (!m_tree)
    {
        return;
    }

    m_tree->clear();
    if (!m_scene)
    {
        return;
    }

    const QString sceneName = QString::fromStdString(m_scene->GetName().empty() ? std::string("Scene") : m_scene->GetName());
    auto* root = new QTreeWidgetItem(m_tree, QStringList{sceneName});
    root->setIcon(0, QIcon(":/aetherion/editor_icon.png"));
    root->setExpanded(true);

    // First pass: create items and lookup without parenting.
    for (const auto& entity : m_scene->GetEntities())
    {
        if (!entity)
        {
            continue;
        }

        const QString name = QString::fromStdString(entity->GetName().empty() ? std::string("Entity") : entity->GetName());
        auto* item = new QTreeWidgetItem(QStringList{name});
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(static_cast<qulonglong>(entity->GetId())));

        // Set icon based on component type
        if (entity->GetComponent<Scene::CameraComponent>())
        {
            item->setIcon(0, QIcon(":/aetherion/icons/camera.svg"));
        }
        else if (entity->GetComponent<Scene::LightComponent>())
        {
            item->setIcon(0, QIcon(":/aetherion/icons/light.svg"));
        }
        else if (entity->GetComponent<Scene::MeshRendererComponent>())
        {
            item->setIcon(0, QIcon(":/aetherion/icons/mesh.svg"));
        }
        else
        {
            item->setIcon(0, QIcon(":/aetherion/icons/entity.svg"));
        }

        m_itemLookup.insert(static_cast<qulonglong>(entity->GetId()), item);
    }

    // Second pass: attach to parents when available.
    QTreeWidgetItem* firstEntityItem = nullptr;
    for (const auto& entity : m_scene->GetEntities())
    {
        if (!entity)
        {
            continue;
        }

        auto transform = entity->GetComponent<Scene::TransformComponent>();
        const Core::EntityId parentId = transform ? transform->GetParentId() : 0;

        QTreeWidgetItem* parentItem = nullptr;
        if (parentId != 0)
        {
            auto it = m_itemLookup.constFind(static_cast<qulonglong>(parentId));
            parentItem = (it != m_itemLookup.constEnd()) ? it.value() : nullptr;
        }

        auto itSelf = m_itemLookup.constFind(static_cast<qulonglong>(entity->GetId()));
        if (itSelf == m_itemLookup.constEnd())
        {
            continue;
        }

        QTreeWidgetItem* item = itSelf.value();
        if (parentItem)
        {
            parentItem->addChild(item);
        }
        else
        {
            root->addChild(item);
        }

        if (!firstEntityItem)
        {
            firstEntityItem = item;
        }
    }

    m_tree->expandAll();
    if (firstEntityItem)
    {
        m_tree->setCurrentItem(firstEntityItem);
        if (m_selection)
        {
            m_selection->SelectEntityById(firstEntityItem->data(0, Qt::UserRole).toULongLong());
        }
    }
}

void EditorHierarchyPanel::SetSelectionModel(EditorSelection* selection)
{
    if (m_selection == selection)
    {
        return;
    }

    if (m_selection)
    {
        QObject::disconnect(m_selection, nullptr, this, nullptr);
    }

    m_selection = selection;
    if (!m_selection)
    {
        return;
    }

    connect(m_selection, &EditorSelection::SelectionChanged, this, [this](Aetherion::Core::EntityId id) {
        SetSelectedEntity(id);
    });
    connect(m_selection, &EditorSelection::SelectionCleared, this, [this]() {
        m_updatingSelection = true;
        if (m_tree)
        {
            m_tree->setCurrentItem(nullptr);
        }
        m_updatingSelection = false;
    });
}

void EditorHierarchyPanel::SetSelectedEntity(Aetherion::Core::EntityId id)
{
    if (!m_tree || m_updatingSelection)
    {
        return;
    }

    auto it = m_itemLookup.constFind(static_cast<qulonglong>(id));
    if (it == m_itemLookup.constEnd())
    {
        return;
    }

    QTreeWidgetItem* item = it.value();
    if (!item)
    {
        return;
    }

    m_updatingSelection = true;
    m_tree->setCurrentItem(item);
    m_updatingSelection = false;
}

Aetherion::Core::EntityId EditorHierarchyPanel::GetSelectedEntityId() const
{
    if (!m_tree)
    {
        return 0;
    }
    
    QTreeWidgetItem* current = m_tree->currentItem();
    if (!current)
    {
        return 0;
    }
    
    const QVariant idData = current->data(0, Qt::UserRole);
    if (!idData.isValid())
    {
        return 0;
    }
    
    return static_cast<Aetherion::Core::EntityId>(idData.toULongLong());
}

void EditorHierarchyPanel::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    auto* addMenu = m_contextMenu->addMenu(tr("Add"));

    auto* addEmptyAction = addMenu->addAction(tr("Empty"));
    addEmptyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    addEmptyAction->setProperty("requiresSelection", false);
    connect(addEmptyAction, &QAction::triggered, this, [this]() {
        Core::EntityId id = GetSelectedEntityId();
        if (id != 0)
        {
            emit createEmptyEntityRequested(id);
        }
        else
        {
            emit createEmptyEntityAtRootRequested();
        }
    });

    auto* newObjectMenu = addMenu->addMenu(tr("New Object"));
    struct MeshEntry
    {
        const char* label;
        const char* assetId;
    };
    const std::array<MeshEntry, 9> meshEntries = {{
        {"Cube", "meshes/cube.obj"},
        {"Textured Cube", "meshes/cube.gltf"},
        {"Pyramid", "meshes/pyramid.obj"},
        {"Plane", "meshes/plane.obj"},
        {"Cone", "meshes/cone.obj"},
        {"Cylinder", "meshes/cylinder.obj"},
        {"Tri Prism", "meshes/tri_prism.obj"},
        {"Wedge", "meshes/wedge.obj"},
        {"Octahedron", "meshes/octahedron.obj"},
    }};
    for (const auto& entry : meshEntries)
    {
        auto* action = newObjectMenu->addAction(tr(entry.label));
        action->setProperty("requiresSelection", false);
        connect(action, &QAction::triggered, this, [this, entry]() {
            const Core::EntityId parentId = GetSelectedEntityId();
            emit createMeshEntityRequested(parentId,
                                           QString::fromLatin1(entry.assetId),
                                           tr(entry.label));
        });
    }

    auto* addLightAction = addMenu->addAction(tr("Directional Light"));
    addLightAction->setProperty("requiresSelection", false);
    connect(addLightAction, &QAction::triggered, this, [this]() {
        Core::EntityId id = GetSelectedEntityId();
        emit createLightEntityRequested(id);
    });

    auto* addCameraAction = addMenu->addAction(tr("Camera"));
    addCameraAction->setProperty("requiresSelection", false);
    connect(addCameraAction, &QAction::triggered, this, [this]() {
        Core::EntityId id = GetSelectedEntityId();
        emit createCameraEntityRequested(id);
    });

    m_contextMenu->addSeparator();

    auto* duplicateAction = m_contextMenu->addAction(tr("Duplicate"));
    duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    duplicateAction->setProperty("requiresSelection", true);
    connect(duplicateAction, &QAction::triggered, this, [this]() {
        Core::EntityId id = GetSelectedEntityId();
        if (id != 0)
        {
            emit entityDuplicateRequested(id);
        }
    });

    auto* renameAction = m_contextMenu->addAction(tr("Rename"));
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameAction->setProperty("requiresSelection", true);
    connect(renameAction, &QAction::triggered, this, [this]() {
        Core::EntityId id = GetSelectedEntityId();
        if (id != 0)
        {
            emit entityRenameRequested(id);
        }
    });
    
    m_contextMenu->addSeparator();

    auto* expandAllAction = m_contextMenu->addAction(tr("Expand All"));
    expandAllAction->setProperty("requiresSelection", false);
    connect(expandAllAction, &QAction::triggered, m_tree, &QTreeWidget::expandAll);

    auto* collapseAllAction = m_contextMenu->addAction(tr("Collapse All"));
    collapseAllAction->setProperty("requiresSelection", false);
    connect(collapseAllAction, &QAction::triggered, m_tree, &QTreeWidget::collapseAll);

    m_contextMenu->addSeparator();

    auto* deleteAction = m_contextMenu->addAction(tr("Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setProperty("requiresSelection", true);
    connect(deleteAction, &QAction::triggered, this, [this]() {
        Core::EntityId id = GetSelectedEntityId();
        if (id != 0)
        {
            emit entityDeleteRequested(id);
        }
    });
}

void EditorHierarchyPanel::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    Core::EntityId entityId = 0;
    
    if (item)
    {
        const QVariant idData = item->data(0, Qt::UserRole);
        if (idData.isValid())
        {
            entityId = static_cast<Core::EntityId>(idData.toULongLong());
        }
    }
    
    const bool hasSelection = entityId != 0;

    auto updateActionState = [hasSelection](QAction* action, const auto& self) -> void {
        if (!action)
        {
            return;
        }
        if (auto* menu = action->menu())
        {
            action->setEnabled(true);
            for (auto* subAction : menu->actions())
            {
                self(subAction, self);
            }
            return;
        }

        if (action->isSeparator())
        {
            return;
        }

        const bool requiresSelection =
            action->property("requiresSelection").toBool();
        action->setEnabled(!requiresSelection || hasSelection);
    };

    for (QAction* action : m_contextMenu->actions())
    {
        updateActionState(action, updateActionState);
    }

    m_contextMenu->exec(m_tree->mapToGlobal(pos));
}

void EditorHierarchyPanel::keyPressEvent(QKeyEvent* event)
{
    Core::EntityId id = GetSelectedEntityId();
    
    if (event->key() == Qt::Key_Delete && id != 0)
    {
        emit entityDeleteRequested(id);
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_F2 && id != 0)
    {
        emit entityRenameRequested(id);
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_D && (event->modifiers() & Qt::ControlModifier) && id != 0)
    {
        emit entityDuplicateRequested(id);
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_N && (event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::ShiftModifier))
    {
        emit createEmptyEntityAtRootRequested();
        event->accept();
        return;
    }
    
    QWidget::keyPressEvent(event);
}
} // namespace Aetherion::Editor

#include "EditorHierarchyPanel.moc"
