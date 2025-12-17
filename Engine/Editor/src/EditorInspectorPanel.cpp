#include "Aetherion/Editor/EditorInspectorPanel.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorInspectorPanel::EditorInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Inspector"), this);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto* content = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(content);

    auto* placeholder = new QLabel(tr("Select an entity to view details"), content);
    placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    contentLayout->addWidget(placeholder);
    contentLayout->addStretch(1);

    content->setLayout(contentLayout);
    scrollArea->setWidget(content);

    layout->addWidget(header);
    layout->addWidget(scrollArea, 1);
    setLayout(layout);

    // TODO: Reflect component properties and support editing with undo/redo.
}
} // namespace Aetherion::Editor
