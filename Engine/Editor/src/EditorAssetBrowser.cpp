#include "Aetherion/Editor/EditorAssetBrowser.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDrag>
#include <QFont>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMimeData>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <iostream>

namespace Aetherion::Editor
{
EditorAssetBrowser::EditorAssetBrowser(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* headerRow = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    auto* header = new QLabel(tr("Asset Browser"), headerRow);
    
    m_backButton = new QToolButton(headerRow);
    m_backButton->setText(tr("<"));
    m_backButton->setToolTip(tr("Back"));
    m_backButton->setEnabled(false);
    
    m_forwardButton = new QToolButton(headerRow);
    m_forwardButton->setText(tr(">"));
    m_forwardButton->setToolTip(tr("Forward"));
    m_forwardButton->setEnabled(false);

    m_rescanButton = new QToolButton(headerRow);
    m_rescanButton->setText(tr("Rescan"));
    m_rescanButton->setToolTip(tr("Rescan assets"));

    headerLayout->addWidget(header);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_backButton);
    headerLayout->addWidget(m_forwardButton);
    headerLayout->addWidget(m_rescanButton);

    // Search/filter field
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter assets..."));
    m_filterEdit->setClearButtonEnabled(true);

    m_list = new QListWidget(this);
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setIconSize(QSize(24, 24)); // Larger icons

    layout->addWidget(headerRow);
    layout->addWidget(m_filterEdit);
    layout->addWidget(m_list, 1);
    setLayout(layout);

    setupContextMenu();

    connect(m_filterEdit, &QLineEdit::textChanged, this, &EditorAssetBrowser::onFilterTextChanged);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &EditorAssetBrowser::showContextMenu);
    
    connect(m_backButton, &QToolButton::clicked, this, &EditorAssetBrowser::NavigateBack);
    connect(m_forwardButton, &QToolButton::clicked, this, &EditorAssetBrowser::NavigateForward);
    
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem* previous) {
        const std::string cur = current ? current->text().toStdString() : std::string("<null>");
        const std::string prev = previous ? previous->text().toStdString() : std::string("<null>");
        // std::cerr << "[EditorAssetBrowser] currentItemChanged: prev='" << prev << "' cur='" << cur << "'" << std::endl;

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

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item)
        {
            QString id = item->data(Qt::UserRole).toString();
            if (id.trimmed().isEmpty())
            {
                id = item->text();
            }
            
            if (id.endsWith('/')) {
                // Navigate into folder
                 if (!m_currentPath.isEmpty()) {
                    m_backStack.push_back(m_currentPath);
                }
                m_currentPath = id;
                m_forwardStack.clear();
                updateNavigationButtons();
                // Logic to actually change view would go here, 
                // but currently we just filter visible items or assume flat list for now.
                // For a proper folder structure, we would filter m_allItems based on m_currentPath.
            }
            else if (!id.trimmed().isEmpty())
            {
                emit AssetDroppedOnScene(id);
            }
        }
        emit AssetActivated();
    });

    if (m_rescanButton)
    {
        connect(m_rescanButton, &QToolButton::clicked, this, &EditorAssetBrowser::RescanRequested);
    }
}

void EditorAssetBrowser::updateNavigationButtons()
{
    if (m_backButton) m_backButton->setEnabled(!m_backStack.empty());
    if (m_forwardButton) m_forwardButton->setEnabled(!m_forwardStack.empty());
}

void EditorAssetBrowser::NavigateBack()
{
    if (m_backStack.empty()) return;
    m_forwardStack.push_back(m_currentPath);
    m_currentPath = m_backStack.back();
    m_backStack.pop_back();
    updateNavigationButtons();
    // emit NavigateToPathRequested(m_currentPath); // If we had folder logic
}

void EditorAssetBrowser::NavigateForward()
{
    if (m_forwardStack.empty()) return;
    m_backStack.push_back(m_currentPath);
    m_currentPath = m_forwardStack.back();
    m_forwardStack.pop_back();
    updateNavigationButtons();
    // emit NavigateToPathRequested(m_currentPath); // If we had folder logic
}

void EditorAssetBrowser::onFilterTextChanged(const QString& text)
{
    m_filterText = text.trimmed().toLower();
    updateVisibleItems();
}

void EditorAssetBrowser::FocusFilter()
{
    if (!m_filterEdit)
    {
        return;
    }

    m_filterEdit->setFocus(Qt::ShortcutFocusReason);
    m_filterEdit->selectAll();
}

void EditorAssetBrowser::updateVisibleItems()
{
    if (!m_list)
    {
        return;
    }

    const bool signalsBlocked = m_list->blockSignals(true);
    m_list->clear();

    auto addHeaderItem = [this](const Item& headerItem) {
        auto* listItem = new QListWidgetItem(headerItem.label, m_list);
        listItem->setData(Qt::UserRole, headerItem.id);
        QFont font = listItem->font();
        font.setBold(true);
        listItem->setFont(font);
        listItem->setFlags(listItem->flags() & ~Qt::ItemIsDragEnabled);
        listItem->setIcon(QIcon(":/aetherion/icons/folder.svg"));
    };

    auto addPlaceholderItem = [this](const QString& text) {
        auto* listItem = new QListWidgetItem(text, m_list);
        QFont font = listItem->font();
        font.setItalic(true);
        listItem->setFont(font);
        listItem->setFlags(listItem->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled));
    };

    Item pendingHeader;
    bool hasPendingHeader = false;
    bool headerAddedForCategory = false;

    for (const auto& item : m_allItems)
    {
        if (item.isHeader)
        {
            pendingHeader = item;
            hasPendingHeader = true;
            headerAddedForCategory = false;

            if (m_filterText.isEmpty())
            {
                addHeaderItem(item);
                headerAddedForCategory = true;
            }
            continue;
        }

        // Check if item matches filter
        bool matchesFilter = true;
        if (!m_filterText.isEmpty())
        {
            const QString labelLower = item.label.toLower();
            const QString idLower = item.id.toLower();
            const QString pathLower = item.assetPath.toLower();
            matchesFilter = labelLower.contains(m_filterText) ||
                idLower.contains(m_filterText) ||
                pathLower.contains(m_filterText);
        }

        if (!matchesFilter)
        {
            continue;
        }

        if (!m_filterText.isEmpty() && hasPendingHeader && !headerAddedForCategory)
        {
            addHeaderItem(pendingHeader);
            headerAddedForCategory = true;
        }

        auto* listItem = new QListWidgetItem(item.label, m_list);
        listItem->setData(Qt::UserRole, item.id);
        listItem->setData(Qt::UserRole + 1, item.assetPath);
        if (!item.id.isEmpty())
        {
            QString tooltip = item.id;
            if (!item.assetPath.isEmpty())
            {
                tooltip += "\n" + item.assetPath;
            }
            listItem->setToolTip(tooltip);
        }

        if (!item.iconPath.isEmpty())
        {
            listItem->setIcon(QIcon(item.iconPath));
        }
        else
        {
            listItem->setIcon(QIcon(":/aetherion/icons/file.svg"));
        }
        
        // Set drag data for the item
        listItem->setFlags(listItem->flags() | Qt::ItemIsDragEnabled);
    }

    if (m_list->count() == 0)
    {
        addPlaceholderItem(m_filterText.isEmpty() ? tr("No assets available.") : tr("No assets match filter."));
    }

    m_list->blockSignals(signalsBlocked);
    emit AssetSelectionCleared();
}

void EditorAssetBrowser::SetItems(const std::vector<Item>& items)
{
    m_allItems = items;
    updateVisibleItems();
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

QString EditorAssetBrowser::GetSelectedAssetId() const
{
    if (!m_list)
    {
        return QString();
    }
    
    QListWidgetItem* current = m_list->currentItem();
    if (!current)
    {
        return QString();
    }
    
    QString id = current->data(Qt::UserRole).toString();
    if (id.trimmed().isEmpty())
    {
        id = current->text();
    }
    
    // Don't return header IDs
    if (id.endsWith('/'))
    {
        return QString();
    }
    
    return id;
}

void EditorAssetBrowser::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    auto* addToSceneAction = m_contextMenu->addAction(tr("Add to Scene"));
    addToSceneAction->setShortcut(QKeySequence(Qt::Key_Return));
    connect(addToSceneAction, &QAction::triggered, this, [this]() {
        QString id = GetSelectedAssetId();
        if (!id.isEmpty())
        {
            emit AssetDroppedOnScene(id);
        }
    });

    m_contextMenu->addSeparator();

    auto* copyIdAction = m_contextMenu->addAction(tr("Copy Asset ID"));
    connect(copyIdAction, &QAction::triggered, this, [this]() {
        const QString id = GetSelectedAssetId();
        if (!id.isEmpty())
        {
            if (auto* clipboard = QApplication::clipboard())
            {
                clipboard->setText(id);
            }
        }
    });

    auto* copyPathAction = m_contextMenu->addAction(tr("Copy Asset Path"));
    connect(copyPathAction, &QAction::triggered, this, [this]() {
        const QString id = GetSelectedAssetId();
        if (id.isEmpty() || !m_list)
        {
            return;
        }

        QListWidgetItem* current = m_list->currentItem();
        if (!current)
        {
            return;
        }

        const QString assetPath = current->data(Qt::UserRole + 1).toString().trimmed();
        if (!assetPath.isEmpty())
        {
            if (auto* clipboard = QApplication::clipboard())
            {
                clipboard->setText(assetPath);
            }
            return;
        }

        const QString label = current->text().trimmed();
        if (label.isEmpty() || label.endsWith('/'))
        {
            return;
        }

        if (auto* clipboard = QApplication::clipboard())
        {
            clipboard->setText(label);
        }
    });

    m_contextMenu->addSeparator();

    auto* showInExplorerAction = m_contextMenu->addAction(tr("Show in Explorer"));
    connect(showInExplorerAction, &QAction::triggered, this, [this]() {
        QString id = GetSelectedAssetId();
        if (!id.isEmpty())
        {
            emit AssetShowInExplorerRequested(id);
        }
    });
    
    m_contextMenu->addSeparator();
    
    auto* renameAction = m_contextMenu->addAction(tr("Rename"));
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    connect(renameAction, &QAction::triggered, this, [this]() {
        QString id = GetSelectedAssetId();
        if (!id.isEmpty())
        {
            emit AssetRenameRequested(id);
        }
    });
    
    auto* deleteAction = m_contextMenu->addAction(tr("Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, [this]() {
        QString id = GetSelectedAssetId();
        if (!id.isEmpty())
        {
            emit AssetDeleteRequested(id);
        }
    });
    
    m_contextMenu->addSeparator();
    
    auto* refreshAction = m_contextMenu->addAction(tr("Refresh"));
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &EditorAssetBrowser::RescanRequested);
}

void EditorAssetBrowser::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_list->itemAt(pos);
    QString selectedId = item ? item->data(Qt::UserRole).toString() : QString();
    
    // Enable/disable actions based on selection
    const bool hasSelection = !selectedId.isEmpty() && !selectedId.endsWith('/');
    
    for (QAction* action : m_contextMenu->actions())
    {
        if (action->text() == tr("Refresh"))
        {
            action->setEnabled(true);
        }
        else if (!action->isSeparator())
        {
            action->setEnabled(hasSelection);
        }
    }
    
    m_contextMenu->exec(m_list->mapToGlobal(pos));
}

void EditorAssetBrowser::keyPressEvent(QKeyEvent* event)
{
    QString id = GetSelectedAssetId();

    if (event->matches(QKeySequence::Copy) && !id.isEmpty())
    {
        if (auto* clipboard = QApplication::clipboard())
        {
            clipboard->setText(id);
        }
        event->accept();
        return;
    }

    if ((event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) ==
            (Qt::ControlModifier | Qt::ShiftModifier) &&
        event->key() == Qt::Key_C)
    {
        QString assetPath;
        if (m_list)
        {
            if (auto* current = m_list->currentItem())
            {
                assetPath = current->data(Qt::UserRole + 1).toString().trimmed();
            }
        }
        if (!assetPath.isEmpty())
        {
            if (auto* clipboard = QApplication::clipboard())
            {
                clipboard->setText(assetPath);
            }
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Delete && !id.isEmpty())
    {
        emit AssetDeleteRequested(id);
        event->accept();
        return;
    }
    else if (event->key() == Qt::Key_F2 && !id.isEmpty())
    {
        emit AssetRenameRequested(id);
        event->accept();
        return;
    }
    else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !id.isEmpty())
    {
        emit AssetDroppedOnScene(id);
        event->accept();
        return;
    }
    
    QWidget::keyPressEvent(event);
}
} // namespace Aetherion::Editor
