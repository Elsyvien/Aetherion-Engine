#include "Aetherion/Editor/EditorConsole.h"

#include <QColor>
#include <QMetaObject>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDateTime>
#include <QGuiApplication>
#include <QClipboard>
#include <QScrollBar>

namespace Aetherion::Editor
{
EditorConsole::EditorConsole(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // --- Toolbar ---
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    // Search
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search..."));
    connect(m_searchBox, &QLineEdit::textChanged, this, &EditorConsole::OnFilterChanged);
    toolbarLayout->addWidget(m_searchBox, 1); // Stretch factor 1 to take available space

    // Severity Filters
    m_toggleInfo = new QCheckBox("Info", this);
    m_toggleInfo->setChecked(true);
    connect(m_toggleInfo, &QCheckBox::toggled, this, &EditorConsole::OnFilterChanged);
    toolbarLayout->addWidget(m_toggleInfo);

    m_toggleWarning = new QCheckBox("Warn", this);
    m_toggleWarning->setChecked(true);
    connect(m_toggleWarning, &QCheckBox::toggled, this, &EditorConsole::OnFilterChanged);
    toolbarLayout->addWidget(m_toggleWarning);

    m_toggleError = new QCheckBox("Error", this);
    m_toggleError->setChecked(true);
    connect(m_toggleError, &QCheckBox::toggled, this, &EditorConsole::OnFilterChanged);
    toolbarLayout->addWidget(m_toggleError);

    // Auto Scroll
    m_autoScroll = new QCheckBox("Auto-scroll", this);
    m_autoScroll->setChecked(true);
    toolbarLayout->addWidget(m_autoScroll);

    // Buttons
    auto* clearBtn = new QPushButton("Clear", this);
    connect(clearBtn, &QPushButton::clicked, this, &EditorConsole::OnClearClicked);
    toolbarLayout->addWidget(clearBtn);

    auto* copyBtn = new QPushButton("Copy", this);
    connect(copyBtn, &QPushButton::clicked, this, &EditorConsole::OnCopyClicked);
    toolbarLayout->addWidget(copyBtn);

    mainLayout->addLayout(toolbarLayout);

    // --- Output Area ---
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setPlaceholderText(tr("Console output will appear here..."));
    
    // Optional: Monospace font for logs
    QFont font("Consolas"); // Or generic Monospace
    font.setStyleHint(QFont::Monospace);
    m_output->setFont(font);

    mainLayout->addWidget(m_output);
    setLayout(mainLayout);
}

void EditorConsole::AppendMessage(const QString& message, ConsoleSeverity severity)
{
    // Ensure thread safety for UI updates
    QMetaObject::invokeMethod(this, [this, message, severity]() {
        ConsoleMessage msg;
        msg.timestamp = QDateTime::currentDateTime();
        msg.text = message;
        msg.severity = severity;

        m_messages.append(msg);

        // Check if we should display it based on current filters
        bool showSeverity = false;
        switch (severity) {
            case ConsoleSeverity::Info: showSeverity = m_toggleInfo->isChecked(); break;
            case ConsoleSeverity::Warning: showSeverity = m_toggleWarning->isChecked(); break;
            case ConsoleSeverity::Error: showSeverity = m_toggleError->isChecked(); break;
        }

        if (!showSeverity) return;

        QString filterText = m_searchBox->text();
        if (!filterText.isEmpty() && !msg.text.contains(filterText, Qt::CaseInsensitive)) {
            return;
        }

        AddMessageToView(msg);
    }, Qt::QueuedConnection);
}

void EditorConsole::AddMessageToView(const ConsoleMessage& msg)
{
    if (!m_output) return;

    QColor color;
    QString prefix;

    switch (msg.severity) {
        case ConsoleSeverity::Error:
            color = QColor(230, 80, 80);
            prefix = "[Error]";
            break;
        case ConsoleSeverity::Warning:
            color = QColor(230, 180, 70);
            prefix = "[Warn] "; // Extra space for alignment roughly
            break;
        case ConsoleSeverity::Info:
        default:
            color = QColor(220, 220, 220);
            prefix = "[Info] ";
            break;
    }

    QString timeStr = msg.timestamp.toString("HH:mm:ss.zzz");
    QString formattedMsg = QString("[%1] %2 %3").arg(timeStr, prefix, msg.text);

    QScrollBar* sb = m_output->verticalScrollBar();
    int oldScrollValue = sb->value();
    bool wasAtBottom = (oldScrollValue == sb->maximum());

    m_output->moveCursor(QTextCursor::End);
    m_output->setTextColor(color);
    m_output->append(formattedMsg);
    m_output->moveCursor(QTextCursor::End);
    m_output->setTextColor(Qt::white); // Reset to default

    if (m_autoScroll->isChecked()) {
        sb->setValue(sb->maximum());
    } else {
        // Maintain position if not auto-scrolling
        sb->setValue(oldScrollValue);
    }
}

void EditorConsole::RefreshConsole()
{
    m_output->clear();
    
    QString filterText = m_searchBox->text();
    bool checkInfo = m_toggleInfo->isChecked();
    bool checkWarn = m_toggleWarning->isChecked();
    bool checkError = m_toggleError->isChecked();

    for (const auto& msg : m_messages) {
        bool showSeverity = false;
        switch (msg.severity) {
            case ConsoleSeverity::Info: showSeverity = checkInfo; break;
            case ConsoleSeverity::Warning: showSeverity = checkWarn; break;
            case ConsoleSeverity::Error: showSeverity = checkError; break;
        }

        if (!showSeverity) continue;

        if (!filterText.isEmpty() && !msg.text.contains(filterText, Qt::CaseInsensitive)) {
            continue;
        }

        AddMessageToView(msg);
    }
}

void EditorConsole::OnFilterChanged()
{
    RefreshConsole();
}

void EditorConsole::OnClearClicked()
{
    m_messages.clear();
    m_output->clear();
}

void EditorConsole::OnCopyClicked()
{
    // Copy the plain text content of the visible console
    if (m_output) {
        QGuiApplication::clipboard()->setText(m_output->toPlainText());
    }
}

} // namespace Aetherion::Editor