#pragma once

#include <QWidget>
#include <QString>
#include <QList>
#include <QDateTime>

class QTextEdit;
class QLineEdit;
class QCheckBox;
class QPushButton;

namespace Aetherion::Editor
{
enum class ConsoleSeverity
{
    Info,
    Warning,
    Error,
};

struct ConsoleMessage
{
    QDateTime timestamp;
    QString text;
    ConsoleSeverity severity;
};

/**
 * @brief Widget for displaying log messages.
 * 
 * Features:
 * - Search filtering (case-insensitive).
 * - Severity filtering (Info, Warning, Error).
 * - Auto-scroll toggle.
 * - Clear and Copy functionality.
 * 
 * Usage:
 * - Use AppendMessage() to add logs.
 * - Controls are available in the top toolbar.
 */
class EditorConsole : public QWidget
{
    Q_OBJECT

public:
    explicit EditorConsole(QWidget* parent = nullptr);
    ~EditorConsole() override = default;

    void AppendMessage(const QString& message, ConsoleSeverity severity);

private slots:
    void OnFilterChanged();
    void OnClearClicked();
    void OnCopyClicked();

private:
    void RefreshConsole();
    void AddMessageToView(const ConsoleMessage& msg);

    QTextEdit* m_output = nullptr;
    QLineEdit* m_searchBox = nullptr;
    
    QCheckBox* m_toggleInfo = nullptr;
    QCheckBox* m_toggleWarning = nullptr;
    QCheckBox* m_toggleError = nullptr;
    QCheckBox* m_autoScroll = nullptr;
    
    QList<ConsoleMessage> m_messages;
};
} // namespace Aetherion::Editor
