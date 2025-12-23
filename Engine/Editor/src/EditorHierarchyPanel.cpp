#include "Aetherion/Editor/EditorHierarchyPanel.h"

#include <QAbstractItemView>
#include <QDropEvent>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Aetherion/Editor/EditorSelection.h"
#include "Aetherion/Scene/Entity.h"
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

    layout->addWidget(header);
    layout->addWidget(m_tree, 1);
    setLayout(layout);

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
} // namespace Aetherion::Editor

#include "EditorHierarchyPanel.moc"
