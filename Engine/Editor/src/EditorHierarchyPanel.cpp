#include "Aetherion/Editor/EditorHierarchyPanel.h"

#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/Scene.h"

namespace Aetherion::Editor
{
EditorHierarchyPanel::EditorHierarchyPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Hierarchy"), this);
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);

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
        emit entitySelected(id);
    });
}

void EditorHierarchyPanel::BindScene(std::shared_ptr<Scene::Scene> scene)
{
    m_scene = std::move(scene);

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

    QTreeWidgetItem* firstEntityItem = nullptr;
    for (const auto& entity : m_scene->GetEntities())
    {
        if (!entity)
        {
            continue;
        }

        const QString name = QString::fromStdString(entity->GetName().empty() ? std::string("Entity") : entity->GetName());
        auto* item = new QTreeWidgetItem(root, QStringList{name});
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(static_cast<qulonglong>(entity->GetId())));
        if (!firstEntityItem)
        {
            firstEntityItem = item;
        }
    }

    m_tree->expandAll();
    if (firstEntityItem)
    {
        m_tree->setCurrentItem(firstEntityItem);
    }
}
} // namespace Aetherion::Editor
