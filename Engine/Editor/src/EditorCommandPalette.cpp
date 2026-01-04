#include "Aetherion/Editor/EditorCommandPalette.h"

#include <algorithm>

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSet>
#include <QSettings>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorCommandPalette::EditorCommandPalette(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Command Palette"));
    setModal(false);
    setWindowFlag(Qt::Tool, true);
    setMinimumWidth(520);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(tr("Type a command..."));
    m_filter->installEventFilter(this);
    connect(m_filter, &QLineEdit::textChanged, this,
            &EditorCommandPalette::OnFilterChanged);
    layout->addWidget(m_filter);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_list->installEventFilter(this);
    connect(m_list, &QListWidget::itemActivated, this,
            &EditorCommandPalette::OnActivateItem);
    connect(m_list, &QListWidget::itemDoubleClicked, this,
            &EditorCommandPalette::OnActivateItem);
    layout->addWidget(m_list, 1);

    m_hintLabel = new QLabel(tr("Enter to run · Esc to close"), this);
    m_hintLabel->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(m_hintLabel);

    setLayout(layout);
}

void EditorCommandPalette::SetCommands(
    const QVector<CommandPaletteEntry>& entries)
{
    m_entries = entries;
    RefreshList();
}

void EditorCommandPalette::AddCommand(const CommandPaletteEntry& entry)
{
    m_entries.push_back(entry);
}

void EditorCommandPalette::RegisterAction(QAction* action,
                                          const QString& category,
                                          const QString& description)
{
    if (!action)
    {
        return;
    }

    QString name = action->text();
    name.remove('&');

    CommandPaletteEntry entry{};
    entry.name = name;
    entry.category = category;
    entry.shortcut = action->shortcut();
    if (!description.isEmpty())
    {
        entry.description = description;
    }
    else if (!action->statusTip().isEmpty())
    {
        entry.description = action->statusTip();
    }
    else if (!action->toolTip().isEmpty())
    {
        entry.description = action->toolTip();
    }
    if (action->isCheckable())
    {
        entry.description = entry.description.isEmpty()
                                ? tr("Toggle this setting")
                                : tr("%1 (toggle)").arg(entry.description);
    }

    entry.action = [action]() { action->trigger(); };
    AddCommand(entry);
}

void EditorCommandPalette::ShowPalette(const QString& query)
{
    if (!query.isNull())
    {
        m_filter->setText(query);
    }

    show();
    raise();
    activateWindow();
    m_filter->setFocus();
    m_filter->selectAll();
    RefreshList();
}

void EditorCommandPalette::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    RefreshList();
}

bool EditorCommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape)
        {
            close();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter)
        {
            auto* item = m_list->currentItem();
            if (item)
            {
                OnActivateItem(item);
                return true;
            }
        }
        if (keyEvent->key() == Qt::Key_Down)
        {
            if (m_list->count() > 0)
            {
                const int next = std::min(m_list->currentRow() + 1,
                                          m_list->count() - 1);
                m_list->setCurrentRow(next);
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up)
        {
            if (m_list->count() > 0)
            {
                const int next = std::max(m_list->currentRow() - 1, 0);
                m_list->setCurrentRow(next);
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void EditorCommandPalette::OnFilterChanged()
{
    RefreshList();
}

void EditorCommandPalette::OnActivateItem(QListWidgetItem* item)
{
    if (!item)
    {
        return;
    }
    const QVariant data = item->data(Qt::UserRole);
    if (!data.isValid())
    {
        return;
    }
    const int index = data.toInt();
    ExecuteEntry(index);
}

void EditorCommandPalette::RefreshList()
{
    const QString query = m_filter->text().trimmed();
    m_filteredIndices = BuildMatchList(query);

    m_list->clear();
    if (m_filteredIndices.isEmpty())
    {
        auto* emptyItem = new QListWidgetItem(tr("No matching commands"));
        emptyItem->setFlags(Qt::NoItemFlags);
        m_list->addItem(emptyItem);
        return;
    }

    for (int index : m_filteredIndices)
    {
        if (index < 0 || index >= m_entries.size())
        {
            continue;
        }
        const auto& entry = m_entries[index];
        QString label = entry.name;
        if (!entry.category.isEmpty())
        {
            label = tr("%1 — %2").arg(label, entry.category);
        }
        if (!entry.shortcut.isEmpty())
        {
            label = tr("%1 [%2]")
                        .arg(label,
                             entry.shortcut.toString(QKeySequence::NativeText));
        }

        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, index);
        if (!entry.description.isEmpty())
        {
            item->setToolTip(entry.description);
        }
        m_list->addItem(item);
    }

    if (m_list->count() > 0)
    {
        m_list->setCurrentRow(0);
    }
}

void EditorCommandPalette::ExecuteEntry(int index)
{
    if (index < 0 || index >= m_entries.size())
    {
        return;
    }

    const auto entry = m_entries[index];
    UpdateRecentCommands(entry.name);
    close();
    if (entry.action)
    {
        entry.action();
    }
    emit CommandExecuted(entry.name);
}

QVector<int> EditorCommandPalette::BuildMatchList(const QString& query) const
{
    QVector<int> result;
    if (m_entries.isEmpty())
    {
        return result;
    }

    const QString normalizedQuery = NormalizeText(query);
    if (normalizedQuery.isEmpty())
    {
        const QStringList recent = LoadRecentCommands();
        QSet<QString> used;
        for (const QString& name : recent)
        {
            for (int i = 0; i < m_entries.size(); ++i)
            {
                if (m_entries[i].name == name && !used.contains(name))
                {
                    result.push_back(i);
                    used.insert(name);
                    break;
                }
            }
        }

        QVector<int> remaining;
        remaining.reserve(m_entries.size());
        for (int i = 0; i < m_entries.size(); ++i)
        {
            if (!used.contains(m_entries[i].name))
            {
                remaining.push_back(i);
            }
        }
        std::sort(remaining.begin(), remaining.end(),
                  [this](int a, int b)
                  {
                      const auto& ea = m_entries[a];
                      const auto& eb = m_entries[b];
                      if (ea.category == eb.category)
                      {
                          return ea.name < eb.name;
                      }
                      return ea.category < eb.category;
                  });

        result += remaining;
        return result;
    }

    struct ScoredIndex
    {
        int index;
        int score;
    };

    QVector<ScoredIndex> scored;
    scored.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i)
    {
        const int score = ScoreEntry(m_entries[i], normalizedQuery);
        if (score >= 0)
        {
            scored.push_back({i, score});
        }
    }

    std::sort(scored.begin(), scored.end(),
              [this](const ScoredIndex& a, const ScoredIndex& b)
              {
                  if (a.score != b.score)
                  {
                      return a.score > b.score;
                  }
                  return m_entries[a.index].name < m_entries[b.index].name;
              });

    for (const auto& item : scored)
    {
        result.push_back(item.index);
    }
    return result;
}

int EditorCommandPalette::ScoreEntry(const CommandPaletteEntry& entry,
                                     const QString& query) const
{
    if (query.isEmpty())
    {
        return 0;
    }

    const QString name = NormalizeText(entry.name);
    const QString desc = NormalizeText(entry.description);
    const QString category = NormalizeText(entry.category);
    const QStringList tokens = query.split(' ', Qt::SkipEmptyParts);

    int score = 0;
    for (const QString& token : tokens)
    {
        bool matched = false;
        if (name.contains(token))
        {
            score += 50;
            matched = true;
        }
        if (!desc.isEmpty() && desc.contains(token))
        {
            score += 20;
            matched = true;
        }
        if (!category.isEmpty() && category.contains(token))
        {
            score += 10;
            matched = true;
        }
        if (!matched)
        {
            return -1;
        }
    }

    if (name.contains(query))
    {
        score += 80;
    }

    score -= name.size() / 4;
    return score;
}

void EditorCommandPalette::UpdateRecentCommands(const QString& name)
{
    QSettings settings("Aetherion", "Editor");
    QStringList recent = settings.value("commandPalette/recentCommands")
                             .toStringList();
    recent.removeAll(name);
    recent.prepend(name);
    while (recent.size() > 12)
    {
        recent.removeLast();
    }
    settings.setValue("commandPalette/recentCommands", recent);
}

QStringList EditorCommandPalette::LoadRecentCommands() const
{
    QSettings settings("Aetherion", "Editor");
    return settings.value("commandPalette/recentCommands").toStringList();
}

QString EditorCommandPalette::NormalizeText(const QString& text) const
{
    QString normalized = text;
    normalized.remove('&');
    return normalized.toLower();
}
} // namespace Aetherion::Editor
