#pragma once

#include <QWidget>
#include <QString>
#include <memory>
#include <cstdint>

class QVBoxLayout;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QLabel;
class QTimer;
class QPropertyAnimation;

namespace Aetherion::Editor {

class AICopilotPanel : public QWidget {
    Q_OBJECT
public:
    explicit AICopilotPanel(QWidget* parent = nullptr);

signals:
    void PromptSubmitted(const QString& prompt);
    void ProcessingStarted();
    void ProcessingStopped();
    void EntityHighlightRequested(uint64_t entityId, float duration = 1.5f);

public slots:
    void AppendMessage(const QString& sender, const QString& message);
    void SetProcessing(bool processing);
    void HighlightEntity(uint64_t entityId);

private:
    void OnSubmit();
    void UpdateThinkingAnimation();

    QVBoxLayout* m_layout = nullptr;
    QTextEdit* m_chatHistory = nullptr;
    QLineEdit* m_inputField = nullptr;
    QPushButton* m_submitButton = nullptr;
    QLabel* m_thinkingLabel = nullptr;
    QTimer* m_thinkingTimer = nullptr;
    int m_thinkingDots = 0;
    bool m_isProcessing = false;
};

} // namespace Aetherion::Editor
