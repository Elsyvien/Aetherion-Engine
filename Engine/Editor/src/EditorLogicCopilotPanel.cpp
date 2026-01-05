#include "Aetherion/Editor/EditorLogicCopilotPanel.h"
#include "Aetherion/Scripting/LogicCopilot.h"
#include "Aetherion/Scene/Scene.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTabWidget>
#include <QScrollArea>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>

namespace Aetherion::Editor
{

EditorLogicCopilotPanel::EditorLogicCopilotPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("LogicCopilotPanel");
    SetupUI();
    SetupConnections();
    setMinimumWidth(300);
}

EditorLogicCopilotPanel::~EditorLogicCopilotPanel() = default;

void EditorLogicCopilotPanel::SetScene(Scene::Scene* scene)
{
    m_scene = scene;
}

void EditorLogicCopilotPanel::SetLogicCopilot(Scripting::LogicCopilot* copilot)
{
    m_copilot = copilot;
    m_generateBtn->setEnabled(copilot != nullptr);
}

void EditorLogicCopilotPanel::SetupUI()
{
    // Main container with scroll
    auto* containerLayout = new QVBoxLayout(this);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    containerLayout->addWidget(scrollArea);

    auto* scrollWidget = new QWidget();
    scrollArea->setWidget(scrollWidget);
    
    auto* layout = new QVBoxLayout(scrollWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // ===== INPUT SECTION =====
    // Type selection - single row
    auto* typeLayout = new QHBoxLayout();
    typeLayout->setSpacing(4);
    m_systemTypeCombo = new QComboBox();
    m_systemTypeCombo->addItem(tr("System"), "System");
    m_systemTypeCombo->addItem(tr("Component"), "Component");
    m_systemTypeCombo->addItem(tr("Behavior"), "Behavior");
    m_systemTypeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    typeLayout->addWidget(m_systemTypeCombo);
    
    m_templateCombo = new QComboBox();
    m_templateCombo->addItem(tr("(No Template)"));
    m_templateCombo->addItem(tr("Movement System"));
    m_templateCombo->addItem(tr("Health Component"));
    m_templateCombo->addItem(tr("AI Patrol"));
    m_templateCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    typeLayout->addWidget(m_templateCombo);
    layout->addLayout(typeLayout);

    // Class name input
    m_classNameInput = new QLineEdit();
    m_classNameInput->setPlaceholderText(tr("Class name (optional, auto-generated)"));
    m_classNameInput->setMaximumHeight(28);
    layout->addWidget(m_classNameInput);

    // Description input - compact
    m_promptInput = new QTextEdit();
    m_promptInput->setPlaceholderText(tr("Describe what you want to generate..."));
    m_promptInput->setMaximumHeight(65);
    m_promptInput->setMinimumHeight(65);
    layout->addWidget(m_promptInput);

    // Action buttons - compact row
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(4);
    m_generateBtn = new QPushButton(tr("Generate"));
    m_generateBtn->setEnabled(false);
    m_generateBtn->setMaximumHeight(28);
    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setMaximumHeight(28);
    actionLayout->addWidget(m_generateBtn);
    actionLayout->addWidget(m_clearBtn);
    actionLayout->addStretch();
    layout->addLayout(actionLayout);

    // Thin separator
    auto* sep1 = new QFrame();
    sep1->setFrameShape(QFrame::HLine);
    sep1->setMaximumHeight(1);
    layout->addWidget(sep1);

    // ===== STATUS SECTION =====
    m_progressBar = new QProgressBar();
    m_progressBar->setMaximumHeight(14);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);

    m_statusLabel = new QLabel();
    m_statusLabel->setMaximumHeight(20);
    m_statusLabel->setVisible(false);
    layout->addWidget(m_statusLabel);

    // ===== OUTPUT SECTION =====
    m_outputTabs = new QTabWidget();
    m_outputTabs->setMinimumHeight(200);
    
    // Header Tab - ultra compact
    auto* headerWidget = new QWidget();
    auto* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    headerLayout->setSpacing(2);
    
    m_headerOutput = new QPlainTextEdit();
    m_headerOutput->setReadOnly(true);
    m_headerOutput->setFont(QFont("Consolas", 8));
    m_headerOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
    headerLayout->addWidget(m_headerOutput);
    
    m_copyHeaderBtn = new QPushButton(tr("Copy"));
    m_copyHeaderBtn->setMaximumHeight(22);
    headerLayout->addWidget(m_copyHeaderBtn);
    
    m_outputTabs->addTab(headerWidget, tr("Header"));

    // Source Tab - ultra compact
    auto* sourceWidget = new QWidget();
    auto* sourceLayout = new QVBoxLayout(sourceWidget);
    sourceLayout->setContentsMargins(4, 4, 4, 4);
    sourceLayout->setSpacing(2);
    
    m_sourceOutput = new QPlainTextEdit();
    m_sourceOutput->setReadOnly(true);
    m_sourceOutput->setFont(QFont("Consolas", 8));
    m_sourceOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
    sourceLayout->addWidget(m_sourceOutput);
    
    m_copySourceBtn = new QPushButton(tr("Copy"));
    m_copySourceBtn->setMaximumHeight(22);
    sourceLayout->addWidget(m_copySourceBtn);
    
    m_outputTabs->addTab(sourceWidget, tr("Source"));

    layout->addWidget(m_outputTabs, 1);

    // Save/Add buttons - compact
    auto* saveLayout = new QHBoxLayout();
    saveLayout->setSpacing(4);
    m_saveBtn = new QPushButton(tr("Save"));
    m_saveBtn->setEnabled(false);
    m_saveBtn->setMaximumHeight(26);
    m_addToProjectBtn = new QPushButton(tr("Add to Project"));
    m_addToProjectBtn->setEnabled(false);
    m_addToProjectBtn->setMaximumHeight(26);
    saveLayout->addWidget(m_saveBtn);
    saveLayout->addWidget(m_addToProjectBtn);
    saveLayout->addStretch();
    layout->addLayout(saveLayout);

    // Separator
    auto* sep2 = new QFrame();
    sep2->setFrameShape(QFrame::HLine);
    sep2->setMaximumHeight(1);
    layout->addWidget(sep2);

    // History - very compact, hidden initially
    m_historyList = new QListWidget();
    m_historyList->setMaximumHeight(50);
    m_historyList->setVisible(false);
    layout->addWidget(m_historyList);

    layout->addStretch();
}

void EditorLogicCopilotPanel::SetupConnections()
{
    connect(m_generateBtn, &QPushButton::clicked, this, &EditorLogicCopilotPanel::OnGenerateClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &EditorLogicCopilotPanel::OnClearClicked);
    connect(m_copyHeaderBtn, &QPushButton::clicked, this, &EditorLogicCopilotPanel::OnCopyHeaderClicked);
    connect(m_copySourceBtn, &QPushButton::clicked, this, &EditorLogicCopilotPanel::OnCopySourceClicked);
    connect(m_saveBtn, &QPushButton::clicked, this, &EditorLogicCopilotPanel::OnSaveClicked);
    connect(m_addToProjectBtn, &QPushButton::clicked, this, &EditorLogicCopilotPanel::OnAddToProjectClicked);
    connect(m_historyList, &QListWidget::itemClicked, this, &EditorLogicCopilotPanel::OnHistoryItemSelected);
    connect(m_systemTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditorLogicCopilotPanel::OnSystemTypeChanged);
    connect(m_templateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditorLogicCopilotPanel::OnTemplateSelected);
}

void EditorLogicCopilotPanel::OnGenerateClicked()
{
    if (!m_copilot)
    {
        QMessageBox::warning(this, tr("Error"), tr("Logic Copilot not initialized.\nPlease configure API key in settings."));
        return;
    }

    QString prompt = m_promptInput->toPlainText().trimmed();
    if (prompt.isEmpty())
    {
        QMessageBox::warning(this, tr("Input Required"), tr("Please enter a description for the code to generate."));
        return;
    }

    UpdateUIState(true);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(tr("Generating code..."));
    m_statusLabel->setVisible(true);

    Scripting::CodeGenerationRequest request;
    request.prompt = prompt.toStdString();
    request.systemType = m_systemTypeCombo->currentData().toString().toStdString();
    request.targetName = m_classNameInput->text().trimmed().toStdString();
    request.autoCompile = true;
    request.addToProject = false; // Let user decide

    auto progressCb = [this](const std::string& id, float progress, const std::string& status) {
        QMetaObject::invokeMethod(this, [this, id, progress, status]() {
            UpdateProgress(QString::fromStdString(id), progress, QString::fromStdString(status));
        }, Qt::QueuedConnection);
    };

    auto completionCb = [this, prompt](const Scripting::CodeGenerationResult& result) {
        QMetaObject::invokeMethod(this, [this, result, prompt]() {
            UpdateOutputDisplay(result);
            UpdateUIState(false);
            
            if (result.status == Scripting::CodeGenerationStatus::Ready)
            {
                AddToHistory(prompt, QString::fromStdString(result.code.className));
                m_lastGeneratedClassName = QString::fromStdString(result.code.className);
                m_saveBtn->setEnabled(true);
                m_addToProjectBtn->setEnabled(true);
            }
        }, Qt::QueuedConnection);
    };

    m_currentRequestId = QString::fromStdString(
        m_copilot->GenerateCode(request, progressCb, completionCb)
    );
}

void EditorLogicCopilotPanel::OnClearClicked()
{
    m_promptInput->clear();
    m_classNameInput->clear();
    m_headerOutput->clear();
    m_sourceOutput->clear();
    m_statusLabel->setText("");
    m_statusLabel->setVisible(false);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_saveBtn->setEnabled(false);
    m_addToProjectBtn->setEnabled(false);
}

void EditorLogicCopilotPanel::OnCopyHeaderClicked()
{
    QApplication::clipboard()->setText(m_headerOutput->toPlainText());
    m_statusLabel->setText(tr("Header copied to clipboard."));
    m_statusLabel->setVisible(true);
}

void EditorLogicCopilotPanel::OnCopySourceClicked()
{
    QApplication::clipboard()->setText(m_sourceOutput->toPlainText());
    m_statusLabel->setText(tr("Source copied to clipboard."));
    m_statusLabel->setVisible(true);
}

void EditorLogicCopilotPanel::OnSaveClicked()
{
    if (m_lastGeneratedClassName.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), tr("No generated code to save."));
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose Save Location"));
    if (dir.isEmpty()) return;

    QString headerPath = dir + "/" + m_lastGeneratedClassName + ".h";
    QString sourcePath = dir + "/" + m_lastGeneratedClassName + ".cpp";

    QFile headerFile(headerPath);
    if (headerFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&headerFile);
        stream << m_headerOutput->toPlainText();
        headerFile.close();
    }

    QFile sourceFile(sourcePath);
    if (sourceFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&sourceFile);
        stream << m_sourceOutput->toPlainText();
        sourceFile.close();
    }

    m_statusLabel->setText(tr("Saved: ") + m_lastGeneratedClassName + ".h/.cpp");
    emit CodeGenerated(m_lastGeneratedClassName, headerPath, sourcePath);
}

void EditorLogicCopilotPanel::OnAddToProjectClicked()
{
    if (!m_copilot || m_lastGeneratedClassName.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"), tr("No generated code to add."));
        return;
    }

    // Find the last result and add to project
    auto* result = m_copilot->GetResult(m_currentRequestId.toStdString());
    if (result && result->status == Scripting::CodeGenerationStatus::Ready)
    {
        if (m_copilot->AddToProject(result->code))
        {
            m_statusLabel->setText(tr("Added to project: ") + m_lastGeneratedClassName);
            emit AddToProject(m_lastGeneratedClassName);
            QMessageBox::information(this, tr("Success"), 
                tr("Code has been added to the project.\n\n"
                   "You need to reconfigure CMake and rebuild to use the code."));
        }
        else
        {
            QMessageBox::warning(this, tr("Error"), tr("Could not add code to project."));
        }
    }
}

void EditorLogicCopilotPanel::OnHistoryItemSelected(QListWidgetItem* item)
{
    int idx = m_historyList->row(item);
    if (idx >= 0 && idx < static_cast<int>(m_history.size()))
    {
        const auto& entry = m_history[idx];
        m_promptInput->setText(entry.prompt);
        m_headerOutput->setPlainText(entry.headerCode);
        m_sourceOutput->setPlainText(entry.sourceCode);
        m_lastGeneratedClassName = entry.className;
        m_saveBtn->setEnabled(!entry.headerCode.isEmpty());
        m_addToProjectBtn->setEnabled(!entry.headerCode.isEmpty());
    }
}

void EditorLogicCopilotPanel::OnSystemTypeChanged(int index)
{
    (void)index;
    // Update placeholder based on type
    QString type = m_systemTypeCombo->currentData().toString();
    if (type == "System")
    {
        m_promptInput->setPlaceholderText(
            tr("Describe an ECS System...\n\n"
               "Examples:\n"
               "- Create a system that applies velocity to position\n"
               "- Create a particle update system for explosions\n"
               "- Create an AI navigation system")
        );
    }
    else if (type == "Component")
    {
        m_promptInput->setPlaceholderText(
            tr("Describe an ECS Component...\n\n"
               "Examples:\n"
               "- Create a health component with max HP and regeneration\n"
               "- Create an inventory component with slots and weight limit\n"
               "- Create a damage component for damage sources")
        );
    }
    else if (type == "Behavior")
    {
        m_promptInput->setPlaceholderText(
            tr("Describe a state machine behavior...\n\n"
               "Examples:\n"
               "- Create a guard behavior: Patrol, Investigate, Chase\n"
               "- Create an NPC behavior: Idle, Wander, Interact, Flee\n"
               "- Create a boss behavior with different attack phases")
        );
    }
}

void EditorLogicCopilotPanel::OnTemplateSelected(int index)
{
    if (index > 0)
    {
        ApplyTemplate(index);
    }
}

void EditorLogicCopilotPanel::ApplyTemplate(int templateIndex)
{
    switch (templateIndex)
    {
        case 1: // Movement System
            m_systemTypeCombo->setCurrentIndex(0);
            m_promptInput->setText(
                "Create a movement system that moves all entities with a VelocityComponent.\n"
                "The system should:\n"
                "- Apply velocity to position (position += velocity * deltaTime)\n"
                "- Optionally apply drag/friction\n"
                "- Optionally have a speed limit"
            );
            m_classNameInput->setText("MovementSystem");
            break;
            
        case 2: // Health Component
            m_systemTypeCombo->setCurrentIndex(1);
            m_promptInput->setText(
                "Create a health component with the following features:\n"
                "- Current and maximum health (float)\n"
                "- TakeDamage(float amount) method\n"
                "- Heal(float amount) method\n"
                "- IsDead() check\n"
                "- Optional: OnDeath callback"
            );
            m_classNameInput->setText("HealthComponent");
            break;
            
        case 3: // AI Patrol
            m_systemTypeCombo->setCurrentIndex(2);
            m_promptInput->setText(
                "Create an AI patrol behavior with these states:\n"
                "- Patrolling: Moves between waypoints\n"
                "- Investigating: Goes to a suspicious location\n"
                "- Chasing: Pursues the player\n"
                "- Returning: Returns to patrol route\n"
                "Transitions based on sight range and distance to player"
            );
            m_classNameInput->setText("PatrolBehavior");
            break;
            
        case 4: // Spawner System
            m_systemTypeCombo->setCurrentIndex(0);
            m_promptInput->setText(
                "Create a spawner system that:\n"
                "- Finds entities with SpawnerComponent\n"
                "- Spawns new entities at regular intervals\n"
                "- Respects maximum concurrent spawn count\n"
                "- Optionally uses random position within radius"
            );
            m_classNameInput->setText("SpawnerSystem");
            break;
            
        case 5: // Damage System
            m_systemTypeCombo->setCurrentIndex(0);
            m_promptInput->setText(
                "Create a damage system that:\n"
                "- Processes DamageEvent components\n"
                "- Applies damage to HealthComponents\n"
                "- Calculates critical hits\n"
                "- Considers armor/resistances\n"
                "- Removes damage events after processing"
            );
            m_classNameInput->setText("DamageSystem");
            break;
    }
    
    // Reset template combo
    m_templateCombo->setCurrentIndex(0);
}

void EditorLogicCopilotPanel::UpdateProgress(const QString& requestId, float progress, const QString& status)
{
    (void)requestId;
    m_progressBar->setValue(static_cast<int>(progress * 100));
    m_statusLabel->setText(status);
}

void EditorLogicCopilotPanel::UpdateOutputDisplay(const Scripting::CodeGenerationResult& result)
{
    m_progressBar->setVisible(false);
    
    if (result.status == Scripting::CodeGenerationStatus::Ready)
    {
        m_headerOutput->setPlainText(QString::fromStdString(result.code.headerCode));
        m_sourceOutput->setPlainText(QString::fromStdString(result.code.sourceCode));
        
        QString statusText = QString("✅ Generiert: %1").arg(QString::fromStdString(result.code.className));
        if (result.code.syntaxValid)
        {
            statusText += " (Syntax OK)";
        }
        if (result.code.compilesSuccessfully)
        {
            statusText += " (Kompiliert)";
        }
        m_statusLabel->setText(statusText);
    }
    else if (result.status == Scripting::CodeGenerationStatus::Failed)
    {
        m_statusLabel->setText("❌ Fehler: " + QString::fromStdString(result.statusMessage));
        
        // Show errors
        QString errorText = "// FEHLER BEI DER CODE-GENERIERUNG\n//\n";
        for (const auto& error : result.code.errors)
        {
            errorText += "// " + QString::fromStdString(error) + "\n";
        }
        m_headerOutput->setPlainText(errorText);
        m_sourceOutput->setPlainText(errorText);
    }
}

void EditorLogicCopilotPanel::AddToHistory(const QString& prompt, const QString& className)
{
    HistoryEntry entry;
    entry.prompt = prompt;
    entry.className = className;
    entry.headerCode = m_headerOutput->toPlainText();
    entry.sourceCode = m_sourceOutput->toPlainText();
    m_history.insert(m_history.begin(), entry);

    // Limit history
    if (m_history.size() > 20)
    {
        m_history.pop_back();
    }

    // Update list
    m_historyList->clear();
    for (const auto& e : m_history)
    {
        QString displayText = QString("[%1] %2")
            .arg(e.className)
            .arg(e.prompt.left(50));
        m_historyList->addItem(displayText);
    }
}

void EditorLogicCopilotPanel::UpdateUIState(bool generating)
{
    m_generateBtn->setEnabled(!generating && m_copilot != nullptr);
    m_promptInput->setEnabled(!generating);
    m_classNameInput->setEnabled(!generating);
    m_systemTypeCombo->setEnabled(!generating);
    m_templateCombo->setEnabled(!generating);
}

} // namespace Aetherion::Editor
