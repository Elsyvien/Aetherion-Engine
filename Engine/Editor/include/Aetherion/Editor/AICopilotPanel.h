#pragma once

#include <QWidget>
#include <QString>
#include <QPropertyAnimation>
#include <memory>
#include <cstdint>

class QVBoxLayout;
class QHBoxLayout;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QLabel;
class QTimer;
class QFrame;
class QScrollArea;
class QGraphicsOpacityEffect;
class QSplitter;

namespace Aetherion::Editor {

// Forward declaration
class CodeViewerWidget;

/// Enhanced AI Copilot Panel with activity tracking, code viewer, and animations
class AICopilotPanel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(float activityOpacity READ activityOpacity WRITE setActivityOpacity)

public:
    explicit AICopilotPanel(QWidget* parent = nullptr);
    ~AICopilotPanel() override;

    // Activity status types
    enum class ActivityType {
        Idle,
        Thinking,
        ExecutingTool,
        GeneratingCode,
        HighlightingEntity,
        ModifyingScene,
        ReadingFile,
        WritingFile
    };

    float activityOpacity() const { return m_activityOpacity; }
    void setActivityOpacity(float opacity);

signals:
    void PromptSubmitted(const QString& prompt);
    void ProcessingStarted();
    void ProcessingStopped();
    void EntityHighlightRequested(uint64_t entityId, float duration = 1.5f);
    void CodeGenerationRequested(const QString& prompt);

public slots:
    void AppendMessage(const QString& sender, const QString& message);
    void SetProcessing(bool processing);
    void HighlightEntity(uint64_t entityId);
    
    // New activity tracking
    void SetActivity(ActivityType type, const QString& details = "");
    void SetCurrentTool(const QString& toolName, const QString& params = "");
    void ShowGeneratedCode(const QString& code, const QString& language = "cpp");
    void ClearCodeViewer();
    void AddActivityLogEntry(const QString& action, const QString& details);

private slots:
    void OnSubmit();
    void UpdateThinkingAnimation();
    void UpdateActivityPulse();
    void AnimateNewMessage();

private:
    void SetupUI();
    void SetupAnimations();
    void SetupStyles();
    QString FormatCodeBlock(const QString& code, const QString& language);
    void ScrollToBottom();

    // Main layout
    QVBoxLayout* m_mainLayout = nullptr;
    QSplitter* m_splitter = nullptr;
    
    // Activity panel (top)
    QFrame* m_activityFrame = nullptr;
    QLabel* m_activityIcon = nullptr;
    QLabel* m_activityLabel = nullptr;
    QLabel* m_activityDetails = nullptr;
    QFrame* m_currentToolFrame = nullptr;
    QLabel* m_toolNameLabel = nullptr;
    QLabel* m_toolParamsLabel = nullptr;
    
    // Activity log
    QFrame* m_activityLogFrame = nullptr;
    QTextEdit* m_activityLog = nullptr;
    
    // Chat area
    QFrame* m_chatFrame = nullptr;
    QTextEdit* m_chatHistory = nullptr;
    
    // Code viewer
    QFrame* m_codeViewerFrame = nullptr;
    QTextEdit* m_codeViewer = nullptr;
    QLabel* m_codeViewerTitle = nullptr;
    QPushButton* m_codeViewerClose = nullptr;
    
    // Input area
    QFrame* m_inputFrame = nullptr;
    QLineEdit* m_inputField = nullptr;
    QPushButton* m_submitButton = nullptr;
    
    // Timers
    QTimer* m_thinkingTimer = nullptr;
    QTimer* m_activityPulseTimer = nullptr;
    
    // Animations
    QPropertyAnimation* m_fadeAnimation = nullptr;
    QPropertyAnimation* m_pulseAnimation = nullptr;
    QGraphicsOpacityEffect* m_activityEffect = nullptr;
    
    // State
    int m_thinkingDots = 0;
    bool m_isProcessing = false;
    ActivityType m_currentActivity = ActivityType::Idle;
    float m_activityOpacity = 1.0f;
    int m_pulseDirection = 1;
};

/// Inline code viewer widget for displaying generated code
class CodeViewerWidget : public QFrame {
    Q_OBJECT
public:
    explicit CodeViewerWidget(QWidget* parent = nullptr);
    
    void SetCode(const QString& code, const QString& language = "cpp");
    void SetTitle(const QString& title);
    void Clear();

signals:
    void CloseRequested();
    void CopyRequested();

private:
    QTextEdit* m_codeEdit = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_closeButton = nullptr;
};

} // namespace Aetherion::Editor
