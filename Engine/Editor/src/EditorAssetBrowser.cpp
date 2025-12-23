#include "Aetherion/Editor/EditorAssetBrowser.h"

#include <QFont>
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

        QString id = current->data(Qt::UserRole).toString();
        if (id.trimmed().isEmpty())
        {
            id = current->text();
        }
        if (id.trimmed().isEmpty())
        {
            emit AssetSelectionCleared();
            return;
        }
        emit AssetSelected(id);
    });

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        emit AssetActivated();
    });

    // TODO: Implement drag-and-drop and asset previews.
}

void EditorAssetBrowser::SetItems(const std::vector<Item>& items)
{
    if (!m_list)
    {
        return;
    }

    const bool signalsBlocked = m_list->blockSignals(true);
    m_list->clear();

    for (const auto& item : items)
    {
        auto* listItem = new QListWidgetItem(item.label, m_list);
        listItem->setData(Qt::UserRole, item.id);
        if (item.isHeader)
        {
            QFont font = listItem->font();
            font.setBold(true);
            listItem->setFont(font);
        }
    }

    m_list->blockSignals(signalsBlocked);
    emit AssetSelectionCleared();
}

void EditorAssetBrowser::ClearSelection()
{
    if (!m_list)
    {
        return;
    }
    const bool signalsBlocked = m_list->blockSignals(true);
    m_list->clearSelection();
    m_list->blockSignals(signalsBlocked);
    emit AssetSelectionCleared();
}
} // namespace Aetherion::Editor
