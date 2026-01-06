#include "Aetherion/Editor/AICopilotPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QFrame>
#include <QSplitter>
#include <QScrollBar>
#include <QTextDocumentFragment>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QClipboard>
#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QTime>

namespace Aetherion::Editor {

// ============================================================================
// AICopilotPanel Implementation
// ============================================================================

AICopilotPanel::AICopilotPanel(QWidget* parent) : QWidget(parent) {
    SetupUI();
    SetupAnimations();
    SetupStyles();
    
    AppendMessage("System", "Aetherion AI Copilot initialized. How can I help?");
}

AICopilotPanel::~AICopilotPanel() = default;

void AICopilotPanel::SetupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // ========== Activity Status Panel ==========
    m_activityFrame = new QFrame(this);
    m_activityFrame->setObjectName("activityFrame");
    auto* activityLayout = new QVBoxLayout(m_activityFrame);
    activityLayout->setContentsMargins(8, 6, 8, 6);
    activityLayout->setSpacing(4);
    
    // Main activity row
    auto* activityRow = new QHBoxLayout();
    activityRow->setSpacing(8);
    
    m_activityIcon = new QLabel("●", m_activityFrame);
    m_activityIcon->setObjectName("activityIcon");
    m_activityIcon->setFixedWidth(16);
    
    m_activityLabel = new QLabel("Ready", m_activityFrame);
    m_activityLabel->setObjectName("activityLabel");
    
    m_activityDetails = new QLabel("", m_activityFrame);
    m_activityDetails->setObjectName("activityDetails");
    m_activityDetails->setWordWrap(true);
    
    activityRow->addWidget(m_activityIcon);
    activityRow->addWidget(m_activityLabel);
    activityRow->addWidget(m_activityDetails, 1);
    activityLayout->addLayout(activityRow);
    
    // Current tool frame (hidden by default)
    m_currentToolFrame = new QFrame(m_activityFrame);
    m_currentToolFrame->setObjectName("currentToolFrame");
    m_currentToolFrame->hide();
    auto* toolLayout = new QHBoxLayout(m_currentToolFrame);
    toolLayout->setContentsMargins(24, 4, 8, 4);
    toolLayout->setSpacing(8);
    
    auto* toolIcon = new QLabel("⚡", m_currentToolFrame);
    m_toolNameLabel = new QLabel("", m_currentToolFrame);
    m_toolNameLabel->setObjectName("toolNameLabel");
    m_toolParamsLabel = new QLabel("", m_currentToolFrame);
    m_toolParamsLabel->setObjectName("toolParamsLabel");
    m_toolParamsLabel->setWordWrap(true);
    
    toolLayout->addWidget(toolIcon);
    toolLayout->addWidget(m_toolNameLabel);
    toolLayout->addWidget(m_toolParamsLabel, 1);
    activityLayout->addWidget(m_currentToolFrame);
    
    m_mainLayout->addWidget(m_activityFrame);
    
    // ========== Activity Log (collapsible) ==========
    m_activityLogFrame = new QFrame(this);
    m_activityLogFrame->setObjectName("activityLogFrame");
    m_activityLogFrame->setMaximumHeight(80);
    auto* logLayout = new QVBoxLayout(m_activityLogFrame);
    logLayout->setContentsMargins(8, 4, 8, 4);
    logLayout->setSpacing(2);
    
    auto* logHeader = new QLabel("📋 Activity Log", m_activityLogFrame);
    logHeader->setObjectName("logHeader");
    logLayout->addWidget(logHeader);
    
    m_activityLog = new QTextEdit(m_activityLogFrame);
    m_activityLog->setObjectName("activityLog");
    m_activityLog->setReadOnly(true);
    m_activityLog->setMaximumHeight(50);
    logLayout->addWidget(m_activityLog);
    
    m_mainLayout->addWidget(m_activityLogFrame);
    
    // ========== Splitter for Chat + Code Viewer ==========
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName("mainSplitter");
    
    // Chat Frame
    m_chatFrame = new QFrame(m_splitter);
    m_chatFrame->setObjectName("chatFrame");
    auto* chatLayout = new QVBoxLayout(m_chatFrame);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    chatLayout->setSpacing(0);
    
    m_chatHistory = new QTextEdit(m_chatFrame);
    m_chatHistory->setObjectName("chatHistory");
    m_chatHistory->setReadOnly(true);
    chatLayout->addWidget(m_chatHistory);
    
    m_splitter->addWidget(m_chatFrame);
    
    // Code Viewer Frame (hidden by default)
    m_codeViewerFrame = new QFrame(m_splitter);
    m_codeViewerFrame->setObjectName("codeViewerFrame");
    m_codeViewerFrame->hide();
    auto* codeLayout = new QVBoxLayout(m_codeViewerFrame);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(0);
    
    // Code viewer header
    auto* codeHeader = new QFrame(m_codeViewerFrame);
    codeHeader->setObjectName("codeViewerHeader");
    auto* codeHeaderLayout = new QHBoxLayout(codeHeader);
    codeHeaderLayout->setContentsMargins(8, 4, 8, 4);
    
    m_codeViewerTitle = new QLabel("📄 Generated Code", codeHeader);
    m_codeViewerTitle->setObjectName("codeViewerTitle");
    
    auto* copyBtn = new QPushButton("📋 Copy", codeHeader);
    copyBtn->setObjectName("copyCodeBtn");
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_codeViewer->toPlainText());
    });
    
    m_codeViewerClose = new QPushButton("✕", codeHeader);
    m_codeViewerClose->setObjectName("closeCodeBtn");
    m_codeViewerClose->setFixedSize(24, 24);
    connect(m_codeViewerClose, &QPushButton::clicked, this, &AICopilotPanel::ClearCodeViewer);
    
    codeHeaderLayout->addWidget(m_codeViewerTitle);
    codeHeaderLayout->addStretch();
    codeHeaderLayout->addWidget(copyBtn);
    codeHeaderLayout->addWidget(m_codeViewerClose);
    codeLayout->addWidget(codeHeader);
    
    m_codeViewer = new QTextEdit(m_codeViewerFrame);
    m_codeViewer->setObjectName("codeViewer");
    m_codeViewer->setReadOnly(true);
    m_codeViewer->setLineWrapMode(QTextEdit::NoWrap);
    
    // Use monospace font
    QFont monoFont("Consolas", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_codeViewer->setFont(monoFont);
    
    codeLayout->addWidget(m_codeViewer);
    
    m_splitter->addWidget(m_codeViewerFrame);
    m_splitter->setSizes({400, 0});
    
    m_mainLayout->addWidget(m_splitter, 1);
    
    // ========== Input Area ==========
    m_inputFrame = new QFrame(this);
    m_inputFrame->setObjectName("inputFrame");
    auto* inputLayout = new QHBoxLayout(m_inputFrame);
    inputLayout->setContentsMargins(8, 8, 8, 8);
    inputLayout->setSpacing(8);
    
    m_inputField = new QLineEdit(m_inputFrame);
    m_inputField->setObjectName("inputField");
    m_inputField->setPlaceholderText(tr("Ask Copilot to create entities, generate code..."));
    connect(m_inputField, &QLineEdit::returnPressed, this, &AICopilotPanel::OnSubmit);
    
    m_submitButton = new QPushButton(tr("Send ➤"), m_inputFrame);
    m_submitButton->setObjectName("submitButton");
    connect(m_submitButton, &QPushButton::clicked, this, &AICopilotPanel::OnSubmit);
    
    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_submitButton);
    
    m_mainLayout->addWidget(m_inputFrame);
    
    // ========== Timers ==========
    m_thinkingTimer = new QTimer(this);
    connect(m_thinkingTimer, &QTimer::timeout, this, &AICopilotPanel::UpdateThinkingAnimation);
    
    m_activityPulseTimer = new QTimer(this);
    connect(m_activityPulseTimer, &QTimer::timeout, this, &AICopilotPanel::UpdateActivityPulse);
}

void AICopilotPanel::SetupAnimations() {
    m_activityEffect = new QGraphicsOpacityEffect(m_activityIcon);
    m_activityIcon->setGraphicsEffect(m_activityEffect);
    
    m_pulseAnimation = new QPropertyAnimation(m_activityEffect, "opacity", this);
    m_pulseAnimation->setDuration(800);
    m_pulseAnimation->setStartValue(0.3);
    m_pulseAnimation->setEndValue(1.0);
    m_pulseAnimation->setEasingCurve(QEasingCurve::InOutSine);
    m_pulseAnimation->setLoopCount(-1);
}

void AICopilotPanel::SetupStyles() {
    setStyleSheet(R"(
        /* Main container */
        AICopilotPanel {
            background-color: #1a1a1a;
        }
        
        /* Activity Frame */
        #activityFrame {
            background-color: #252525;
            border-bottom: 1px solid #333;
        }
        
        #activityIcon {
            color: #4dff88;
            font-size: 12px;
        }
        
        #activityLabel {
            color: #e0e0e0;
            font-weight: bold;
            font-size: 12px;
        }
        
        #activityDetails {
            color: #888;
            font-size: 11px;
        }
        
        /* Current Tool */
        #currentToolFrame {
            background-color: #1e2d1e;
            border-radius: 4px;
            margin: 2px 0px;
        }
        
        #toolNameLabel {
            color: #4da6ff;
            font-weight: bold;
            font-size: 11px;
        }
        
        #toolParamsLabel {
            color: #999;
            font-size: 10px;
            font-family: Consolas, monospace;
        }
        
        /* Activity Log */
        #activityLogFrame {
            background-color: #1e1e1e;
            border-bottom: 1px solid #333;
        }
        
        #logHeader {
            color: #888;
            font-size: 10px;
        }
        
        #activityLog {
            background-color: #141414;
            color: #666;
            font-size: 10px;
            font-family: Consolas, monospace;
            border: none;
            border-radius: 2px;
        }
        
        /* Chat Area */
        #chatFrame {
            background-color: #1a1a1a;
        }
        
        #chatHistory {
            background-color: #1a1a1a;
            border: none;
            color: #e0e0e0;
            font-size: 13px;
            padding: 8px;
        }
        
        /* Code Viewer */
        #codeViewerFrame {
            background-color: #1e1e1e;
            border-left: 2px solid #4da6ff;
        }
        
        #codeViewerHeader {
            background-color: #252525;
            border-bottom: 1px solid #333;
        }
        
        #codeViewerTitle {
            color: #4da6ff;
            font-weight: bold;
            font-size: 11px;
        }
        
        #codeViewer {
            background-color: #1a1a1a;
            color: #d4d4d4;
            border: none;
            selection-background-color: #264f78;
        }
        
        #copyCodeBtn {
            background-color: #2d4a2d;
            color: #90EE90;
            border: none;
            border-radius: 3px;
            padding: 4px 8px;
            font-size: 10px;
        }
        
        #copyCodeBtn:hover {
            background-color: #3d5a3d;
        }
        
        #closeCodeBtn {
            background-color: transparent;
            color: #888;
            border: none;
            font-size: 14px;
        }
        
        #closeCodeBtn:hover {
            color: #ff6b6b;
        }
        
        /* Input Area */
        #inputFrame {
            background-color: #252525;
            border-top: 1px solid #333;
        }
        
        #inputField {
            background-color: #1a1a1a;
            border: 1px solid #444;
            border-radius: 4px;
            color: #e0e0e0;
            padding: 8px 12px;
            font-size: 13px;
        }
        
        #inputField:focus {
            border-color: #4da6ff;
        }
        
        #inputField:disabled {
            background-color: #252525;
            color: #666;
        }
        
        #submitButton {
            background-color: #2d5a8a;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
            font-size: 12px;
            min-width: 80px;
        }
        
        #submitButton:hover {
            background-color: #3d6a9a;
        }
        
        #submitButton:pressed {
            background-color: #1d4a7a;
        }
        
        #submitButton:disabled {
            background-color: #333;
            color: #666;
        }
        
        /* Splitter */
        #mainSplitter::handle {
            background-color: #333;
            width: 2px;
        }
        
        #mainSplitter::handle:hover {
            background-color: #4da6ff;
        }
        
        /* Scrollbars */
        QScrollBar:vertical {
            background-color: #1a1a1a;
            width: 10px;
            margin: 0px;
        }
        
        QScrollBar::handle:vertical {
            background-color: #444;
            min-height: 20px;
            border-radius: 5px;
            margin: 2px;
        }
        
        QScrollBar::handle:vertical:hover {
            background-color: #555;
        }
        
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}

void AICopilotPanel::setActivityOpacity(float opacity) {
    m_activityOpacity = opacity;
    if (m_activityEffect) {
        m_activityEffect->setOpacity(opacity);
    }
}

void AICopilotPanel::SetActivity(ActivityType type, const QString& details) {
    m_currentActivity = type;
    
    struct ActivityInfo {
        QString icon;
        QString color;
        QString label;
    };
    
    static const QMap<ActivityType, ActivityInfo> activityMap = {
        {ActivityType::Idle,              {"●", "#4dff88", "Ready"}},
        {ActivityType::Thinking,          {"◐", "#ffcc00", "Thinking"}},
        {ActivityType::ExecutingTool,     {"⚡", "#4da6ff", "Executing Tool"}},
        {ActivityType::GeneratingCode,    {"⌨", "#ff88ff", "Generating Code"}},
        {ActivityType::HighlightingEntity,{"👁", "#ff8844", "Highlighting Entity"}},
        {ActivityType::ModifyingScene,    {"🔧", "#88ff88", "Modifying Scene"}},
        {ActivityType::ReadingFile,       {"📖", "#88ccff", "Reading File"}},
        {ActivityType::WritingFile,       {"💾", "#ffaa88", "Writing File"}}
    };
    
    auto info = activityMap.value(type, activityMap[ActivityType::Idle]);
    
    m_activityIcon->setText(info.icon);
    m_activityIcon->setStyleSheet(QString("#activityIcon { color: %1; font-size: 14px; }").arg(info.color));
    m_activityLabel->setText(info.label);
    m_activityDetails->setText(details);
    
    if (type == ActivityType::Idle) {
        m_pulseAnimation->stop();
        m_activityEffect->setOpacity(1.0);
    } else {
        m_pulseAnimation->start();
    }
}

void AICopilotPanel::SetCurrentTool(const QString& toolName, const QString& params) {
    if (toolName.isEmpty()) {
        m_currentToolFrame->hide();
        return;
    }
    
    m_toolNameLabel->setText(toolName);
    m_toolParamsLabel->setText(params.left(100) + (params.length() > 100 ? "..." : ""));
    m_currentToolFrame->show();
    
    // Animate appearance
    auto* effect = new QGraphicsOpacityEffect(m_currentToolFrame);
    m_currentToolFrame->setGraphicsEffect(effect);
    
    auto* anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AICopilotPanel::ShowGeneratedCode(const QString& code, const QString& language) {
    m_codeViewer->setPlainText(code);
    m_codeViewerTitle->setText(QString("📄 Generated Code (%1)").arg(language.toUpper()));
    
    if (m_codeViewerFrame->isHidden()) {
        m_codeViewerFrame->show();
        m_splitter->setSizes({300, 300});
        
        // Animate appearance
        auto* effect = new QGraphicsOpacityEffect(m_codeViewerFrame);
        m_codeViewerFrame->setGraphicsEffect(effect);
        
        auto* anim = new QPropertyAnimation(effect, "opacity", this);
        anim->setDuration(300);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void AICopilotPanel::ClearCodeViewer() {
    // Animate disappearance
    auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_codeViewerFrame->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(m_codeViewerFrame);
        m_codeViewerFrame->setGraphicsEffect(effect);
    }
    
    auto* anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(200);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        m_codeViewerFrame->hide();
        m_codeViewer->clear();
        m_splitter->setSizes({400, 0});
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AICopilotPanel::AddActivityLogEntry(const QString& action, const QString& details) {
    QString timestamp = QTime::currentTime().toString("HH:mm:ss");
    QString entry = QString("<span style='color:#666;'>[%1]</span> "
                           "<span style='color:#4da6ff;'>%2</span> "
                           "<span style='color:#888;'>%3</span><br>")
                    .arg(timestamp, action, details);
    m_activityLog->insertHtml(entry);
    m_activityLog->verticalScrollBar()->setValue(m_activityLog->verticalScrollBar()->maximum());
}

void AICopilotPanel::HighlightEntity(uint64_t entityId) {
    SetActivity(ActivityType::HighlightingEntity, QString("Entity #%1").arg(entityId));
    AddActivityLogEntry("Highlight", QString("Entity ID: %1").arg(entityId));
    emit EntityHighlightRequested(entityId, 1.5f);
    
    // Reset to idle after delay
    QTimer::singleShot(1500, this, [this]() {
        if (m_currentActivity == ActivityType::HighlightingEntity) {
            SetActivity(ActivityType::Idle);
        }
    });
}

void AICopilotPanel::AppendMessage(const QString& sender, const QString& message) {
    QString formatted;
    
    if (sender == "User") {
        formatted = QString(
            "<div style='margin: 6px 0px; padding: 8px; text-align: left; "
            "background-color: #1e2d3d; border-left: 3px solid #4da6ff;'>"
            "<span style='color: #4da6ff; font-weight: bold;'>User:</span> "
            "<span style='color: #e0e0e0;'>%1</span>"
            "</div>"
        ).arg(message.toHtmlEscaped());
    } else if (sender == "System") {
        formatted = QString(
            "<div style='margin: 4px 0px; padding: 6px; text-align: left; "
            "color: #888; font-style: italic; font-size: 11px;'>"
            "%1"
            "</div>"
        ).arg(message.toHtmlEscaped());
    } else {
        // Copilot message
        formatted = QString(
            "<div style='margin: 6px 0px; padding: 8px; text-align: left; "
            "background-color: #1d2d1d; border-left: 3px solid #4dff88;'>"
            "<span style='color: #4dff88; font-weight: bold;'>Copilot:</span> "
            "<span style='color: #e0e0e0;'>%1</span>"
            "</div>"
        ).arg(message.toHtmlEscaped());
    }
    
    m_chatHistory->append(formatted);
    AnimateNewMessage();
    ScrollToBottom();
}

void AICopilotPanel::AnimateNewMessage() {
    // Smooth scroll to bottom with animation
    QTimer::singleShot(50, this, &AICopilotPanel::ScrollToBottom);
}

void AICopilotPanel::ScrollToBottom() {
    auto* scrollBar = m_chatHistory->verticalScrollBar();
    
    auto* anim = new QPropertyAnimation(scrollBar, "value", this);
    anim->setDuration(150);
    anim->setStartValue(scrollBar->value());
    anim->setEndValue(scrollBar->maximum());
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AICopilotPanel::SetProcessing(bool processing) {
    m_isProcessing = processing;
    m_inputField->setEnabled(!processing);
    m_submitButton->setEnabled(!processing);
    
    if (processing) {
        m_submitButton->setText(tr("⏳"));
        m_thinkingDots = 0;
        SetActivity(ActivityType::Thinking, "Processing your request...");
        m_thinkingTimer->start(400);
        emit ProcessingStarted();
    } else {
        m_submitButton->setText(tr("Send ➤"));
        m_thinkingTimer->stop();
        SetActivity(ActivityType::Idle);
        SetCurrentTool("", "");
        m_inputField->setFocus();
        emit ProcessingStopped();
    }
}

void AICopilotPanel::UpdateThinkingAnimation() {
    m_thinkingDots = (m_thinkingDots + 1) % 4;
    QString dots = QString("●").repeated(m_thinkingDots) + QString("○").repeated(3 - m_thinkingDots);
    
    static const QStringList phases = {
        "Analyzing",
        "Processing", 
        "Reasoning",
        "Formulating"
    };
    
    static int phaseIndex = 0;
    if (m_thinkingDots == 0) {
        phaseIndex = (phaseIndex + 1) % phases.size();
    }
    
    m_activityLabel->setText(phases[phaseIndex]);
    m_activityDetails->setText(dots);
}

void AICopilotPanel::UpdateActivityPulse() {
    // Already handled by QPropertyAnimation
}

void AICopilotPanel::OnSubmit() {
    const QString text = m_inputField->text().trimmed();
    if (text.isEmpty()) return;

    AppendMessage("User", text);
    AddActivityLogEntry("User Input", text.left(50) + (text.length() > 50 ? "..." : ""));
    emit PromptSubmitted(text);
    m_inputField->clear();
}

QString AICopilotPanel::FormatCodeBlock(const QString& code, const QString& language) {
    return QString("<pre style='background-color: #1e1e1e; padding: 8px; border-radius: 4px; "
                  "font-family: Consolas, monospace; font-size: 12px; color: #d4d4d4; "
                  "overflow-x: auto;'>%1</pre>").arg(code.toHtmlEscaped());
}

} // namespace Aetherion::Editor
