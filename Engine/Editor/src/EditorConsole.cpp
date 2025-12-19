#include "Aetherion/Editor/EditorConsole.h"

#include <QColor>
#include <QMetaObject>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorConsole::EditorConsole(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(tr("Console output will appear here..."));

    layout->addWidget(m_output);
    setLayout(layout);

    // TODO: Connect to logging system with severity filtering and search.
}

void EditorConsole::AppendMessage(const QString& message, ConsoleSeverity severity)
{
    if (!m_output)
    {
        return;
    }

    const QColor color = (severity == ConsoleSeverity::Error)
                             ? QColor(230, 80, 80)
                             : (severity == ConsoleSeverity::Warning ? QColor(230, 180, 70) : QColor(220, 220, 220));

    const QString prefix = (severity == ConsoleSeverity::Error)
                               ? QStringLiteral("[Error] ")
                               : (severity == ConsoleSeverity::Warning ? QStringLiteral("[Warn] ")
                                                                       : QStringLiteral("[Info] "));

    QMetaObject::invokeMethod(
        m_output,
        [this, text = prefix + message, color]() {
            m_output->moveCursor(QTextCursor::End);
            m_output->setTextColor(color);
            m_output->append(text);
            m_output->moveCursor(QTextCursor::End);
            m_output->setTextColor(Qt::white);
        },
        Qt::QueuedConnection);
}
} // namespace Aetherion::Editor
