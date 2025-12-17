#include "Aetherion/Editor/EditorConsole.h"

#include <QTextEdit>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorConsole::EditorConsole(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* consoleOutput = new QTextEdit(this);
    consoleOutput->setReadOnly(true);
    consoleOutput->setPlaceholderText(tr("Console output will appear here..."));

    layout->addWidget(consoleOutput);
    setLayout(layout);

    // TODO: Connect to logging system with severity filtering and search.
}
} // namespace Aetherion::Editor
