#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;

namespace Aetherion::Editor
{
enum class ConsoleSeverity
{
    Info,
    Warning,
    Error,
};

class EditorConsole : public QWidget
{
    Q_OBJECT

public:
    explicit EditorConsole(QWidget* parent = nullptr);
    ~EditorConsole() override = default;

    void AppendMessage(const QString& message, ConsoleSeverity severity);

private:
    QTextEdit* m_output = nullptr;
};
} // namespace Aetherion::Editor
