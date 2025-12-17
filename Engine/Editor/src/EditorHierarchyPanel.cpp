#include "Aetherion/Editor/EditorHierarchyPanel.h"

#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorHierarchyPanel::EditorHierarchyPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Hierarchy"), this);
    auto* tree = new QTreeWidget(this);
    tree->setHeaderHidden(true);
    tree->setIndentation(14);

    auto* placeholderRoot = new QTreeWidgetItem(tree, QStringList{tr("Scene Root")});
    new QTreeWidgetItem(placeholderRoot, QStringList{tr("Entity A")});
    new QTreeWidgetItem(placeholderRoot, QStringList{tr("Entity B")});
    tree->expandAll();

    layout->addWidget(header);
    layout->addWidget(tree, 1);
    setLayout(layout);

    // TODO: Bind to runtime scene graph and selection state.
}
} // namespace Aetherion::Editor
