#pragma once

#include <functional>

#include <QDialog>
#include <QKeySequence>
#include <QString>
#include <QVector>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace Aetherion::Editor
{
struct CommandPaletteEntry
{
    QString name;
    QString description;
    QString category;
    QKeySequence shortcut;
    std::function<void()> action;
};

class EditorCommandPalette : public QDialog
{
    Q_OBJECT

public:
    explicit EditorCommandPalette(QWidget* parent = nullptr);

    void SetCommands(const QVector<CommandPaletteEntry>& entries);
    void AddCommand(const CommandPaletteEntry& entry);
    void RegisterAction(QAction* action,
                        const QString& category,
                        const QString& description = QString());
    void ShowPalette(const QString& query = QString());

signals:
    void CommandExecuted(const QString& name);

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void OnFilterChanged();
    void OnActivateItem(QListWidgetItem* item);

private:
    void RefreshList();
    void ExecuteEntry(int index);
    QVector<int> BuildMatchList(const QString& query) const;
    int ScoreEntry(const CommandPaletteEntry& entry, const QString& query) const;
    void UpdateRecentCommands(const QString& name);
    QStringList LoadRecentCommands() const;
    QString NormalizeText(const QString& text) const;

    QLineEdit* m_filter = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_hintLabel = nullptr;
    QVector<CommandPaletteEntry> m_entries;
    QVector<int> m_filteredIndices;
};
} // namespace Aetherion::Editor
