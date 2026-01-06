#include "Aetherion/Editor/AICopilotPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QTextDocumentFragment>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

namespace Aetherion::Editor {

AICopilotPanel::AICopilotPanel(QWidget* parent) : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);

    // Status indicator
    m_thinkingLabel = new QLabel(this);
    m_thinkingLabel->setText("Ready");
    m_thinkingLabel->setStyleSheet(
        "QLabel {"
        "  background-color: #2d3f2d;"
        "  color: #90EE90;"
        "  font-size: 11px;"
        "  padding: 4px 8px;"
        "  border-radius: 2px;"
        "  margin: 2px 0px;"
        "}"
    );
    m_thinkingLabel->setAlignment(Qt::AlignLeft);
    m_thinkingLabel->hide();
    m_layout->addWidget(m_thinkingLabel);

    // Timer for animated dots
    m_thinkingTimer = new QTimer(this);
    connect(m_thinkingTimer, &QTimer::timeout, this, &AICopilotPanel::UpdateThinkingAnimation);

    // Chat History
    m_chatHistory = new QTextEdit(this);
    m_chatHistory->setReadOnly(true);
    m_layout->addWidget(m_chatHistory);

    // Input Area
    auto* inputLayout = new QHBoxLayout();
    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText(tr("Ask Copilot to create entities, move objects..."));
    connect(m_inputField, &QLineEdit::returnPressed, this, &AICopilotPanel::OnSubmit);
    
    m_submitButton = new QPushButton(tr("Send"), this);
    connect(m_submitButton, &QPushButton::clicked, this, &AICopilotPanel::OnSubmit);

    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_submitButton);
    m_layout->addLayout(inputLayout);

    AppendMessage("System", "Aetherion AI Copilot initialized. How can I help?");
}

void AICopilotPanel::HighlightEntity(uint64_t entityId) {
    emit EntityHighlightRequested(entityId, 1.5f);
}

void AICopilotPanel::AppendMessage(const QString& sender, const QString& message) {
    auto renderMarkdown = [](const QString& text) {
        return QTextDocumentFragment::fromMarkdown(text).toHtml();
    };

    QString bodyHtml = renderMarkdown(Qt::convertFromPlainText(message));
    
    if (sender == "User") {
        // User message: light background, left-aligned
        const QString formatted = QString(
            "<div style='margin: 4px 0px; padding: 8px 12px; "
            "background-color: #1e3a5f; border-left: 3px solid #4da6ff; "
            "color: #e0e0e0; border-radius: 3px;'>"
            "<b style='color: #4da6ff;'>You:</b> %1"
            "</div>"
        ).arg(bodyHtml);
        m_chatHistory->append(formatted);
    } else if (sender == "System") {
        // System message: italic gray
        const QString formatted = QString(
            "<div style='margin: 4px 0px; padding: 6px 12px; "
            "background-color: #2a2a2a; color: #999999; "
            "border-radius: 2px; font-style: italic;'>"
            "%1"
            "</div>"
        ).arg(renderMarkdown(message));
        m_chatHistory->append(formatted);
    } else {
        // AI message: different background and color
        const QString formatted = QString(
            "<div style='margin: 4px 0px; padding: 8px 12px; "
            "background-color: #1f3a1f; border-left: 3px solid #4dff88; "
            "color: #e0e0e0; border-radius: 3px;'>"
            "<b style='color: #4dff88;'>Copilot:</b> %1"
            "</div>"
        ).arg(bodyHtml);
        m_chatHistory->append(formatted);
    }
}

void AICopilotPanel::SetProcessing(bool processing) {
    m_isProcessing = processing;
    m_inputField->setEnabled(!processing);
    m_submitButton->setEnabled(!processing);
    
    if (processing) {
        m_submitButton->setText(tr("..."));
        m_thinkingDots = 0;
        m_thinkingLabel->setText("Analyzing.");
        m_thinkingLabel->show();
        m_thinkingTimer->start(500); // Update every 500ms
        emit ProcessingStarted();
    } else {
        m_submitButton->setText(tr("Send"));
        m_thinkingLabel->hide();
        m_thinkingTimer->stop();
        m_inputField->setFocus();
        emit ProcessingStopped();
    }
}

void AICopilotPanel::UpdateThinkingAnimation() {
    m_thinkingDots = (m_thinkingDots + 1) % 3;
    QString dots = QString(".").repeated(m_thinkingDots);
    
    static const QStringList statuses = {
        "Analyzing",
        "Processing",
        "Executing"
    };
    
    static int statusIndex = 0;
    if (m_thinkingDots == 0) {
        statusIndex = (statusIndex + 1) % statuses.size();
    }
    
    m_thinkingLabel->setText(statuses[statusIndex] + dots);
}

void AICopilotPanel::OnSubmit() {
    const QString text = m_inputField->text().trimmed();
    if (text.isEmpty()) return;

    AppendMessage("User", text);
    emit PromptSubmitted(text);
    m_inputField->clear();
}

} // namespace Aetherion::Editor
