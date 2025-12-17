#include "Aetherion/Editor/EditorAssetBrowser.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorAssetBrowser::EditorAssetBrowser(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Asset Browser"), this);
    auto* list = new QListWidget(this);
    list->addItems({tr("Textures/"), tr("Audio/"), tr("Scripts/"), tr("Prefabs/")});

    layout->addWidget(header);
    layout->addWidget(list, 1);
    setLayout(layout);

    // TODO: Pull data from asset registry and support previews.
}
} // namespace Aetherion::Editor
