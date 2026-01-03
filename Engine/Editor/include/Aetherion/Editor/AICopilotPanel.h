#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QVBoxLayout;
class QLineEdit;
class QTextEdit;
class QPushButton;

namespace Aetherion::Editor {

class AICopilotPanel : public QWidget {
    Q_OBJECT
public:
    explicit AICopilotPanel(QWidget* parent = nullptr);

signals:
    void PromptSubmitted(const QString& prompt);

public slots:
    void AppendMessage(const QString& sender, const QString& message);
    void SetProcessing(bool processing);

private:
    void OnSubmit();

    QVBoxLayout* m_layout = nullptr;
    QTextEdit* m_chatHistory = nullptr;
    QLineEdit* m_inputField = nullptr;
    QPushButton* m_submitButton = nullptr;
    bool m_isProcessing = false;
};

} // namespace Aetherion::Editor
