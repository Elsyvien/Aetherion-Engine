#include "Aetherion/Editor/EditorAssetBrowser.h"

#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>

#include <iostream>

namespace Aetherion::Editor
{
EditorAssetBrowser::EditorAssetBrowser(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Asset Browser"), this);
    m_list = new QListWidget(this);
    m_list->addItems({tr("Textures/"), tr("Audio/"), tr("Scripts/"), tr("Prefabs/")});

    layout->addWidget(header);
    layout->addWidget(m_list, 1);
    setLayout(layout);

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem* previous) {
        const std::string cur = current ? current->text().toStdString() : std::string("<null>");
        const std::string prev = previous ? previous->text().toStdString() : std::string("<null>");
        std::cerr << "[EditorAssetBrowser] currentItemChanged: prev='" << prev << "' cur='" << cur << "'" << std::endl;

        if (!current)
        {
            emit AssetSelectionCleared();
            return;
        }

        const QString text = current->text();
        if (text.trimmed().isEmpty())
        {
            emit AssetSelectionCleared();
            return;
        }
        emit AssetSelected(text);
    });

    // TODO: Pull data from asset registry and support previews.
}
} // namespace Aetherion::Editor
