#include "Aetherion/Editor/AICopilotPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

namespace Aetherion::Editor {

AICopilotPanel::AICopilotPanel(QWidget* parent) : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);

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

void AICopilotPanel::AppendMessage(const QString& sender, const QString& message) {
    QString formatted;
    if (sender == "User") {
        formatted = QString("<div style='color: #4da6ff;'><b>You:</b> %1</div>").arg(message);
    } else if (sender == "System") {
        formatted = QString("<div style='color: #888888;'><i>%1</i></div>").arg(message);
    } else {
        formatted = QString("<div style='color: #4dff88;'><b>Copilot:</b> %1</div>").arg(message);
    }
    m_chatHistory->append(formatted);
}

void AICopilotPanel::SetProcessing(bool processing) {
    m_isProcessing = processing;
    m_inputField->setEnabled(!processing);
    m_submitButton->setEnabled(!processing);
    if (processing) {
        m_submitButton->setText(tr("..."));
    } else {
        m_submitButton->setText(tr("Send"));
        m_inputField->setFocus();
    }
}

void AICopilotPanel::OnSubmit() {
    const QString text = m_inputField->text().trimmed();
    if (text.isEmpty()) return;

    AppendMessage("User", text);
    emit PromptSubmitted(text);
    m_inputField->clear();
}

} // namespace Aetherion::Editor
