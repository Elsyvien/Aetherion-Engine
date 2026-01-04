#include "Aetherion/Editor/EditorLogicCopilotPanel.h"
#include "Aetherion/Scripting/LogicCopilot.h"
#include "Aetherion/Scene/Scene.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QTabWidget>
#include <QSplitter>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>

namespace Aetherion::Editor
{

EditorLogicCopilotPanel::EditorLogicCopilotPanel(QWidget* parent)
    : QDockWidget("Logic Copilot", parent)
{
    SetupUI();
    SetupConnections();
    setMinimumWidth(450);
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
    auto* mainWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Header
    auto* headerLabel = new QLabel("🤖 <b>Logic Copilot</b> - Generate C++ ECS code from natural language", this);
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);

    // Splitter for input/output
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // ========== INPUT SECTION ==========
    auto* inputGroup = new QGroupBox("Input", this);
    auto* inputLayout = new QVBoxLayout(inputGroup);

    // System type and template row
    auto* typeRow = new QHBoxLayout();
    
    m_systemTypeCombo = new QComboBox(this);
    m_systemTypeCombo->addItem("🔧 System", "System");
    m_systemTypeCombo->addItem("📦 Component", "Component");
    m_systemTypeCombo->addItem("🧠 Behavior", "Behavior");
    typeRow->addWidget(new QLabel("Type:", this));
    typeRow->addWidget(m_systemTypeCombo);
    
    m_templateCombo = new QComboBox(this);
    m_templateCombo->addItem("(Kein Template)");
    m_templateCombo->addItem("Bewegungssystem");
    m_templateCombo->addItem("Gesundheitskomponente");
    m_templateCombo->addItem("KI-Patrouille");
    m_templateCombo->addItem("Spawner-System");
    m_templateCombo->addItem("Schadens-System");
    typeRow->addWidget(new QLabel("Template:", this));
    typeRow->addWidget(m_templateCombo);
    typeRow->addStretch();
    
    inputLayout->addLayout(typeRow);

    // Class name
    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel("Klassenname:", this));
    m_classNameInput = new QLineEdit(this);
    m_classNameInput->setPlaceholderText("(automatisch generiert wenn leer)");
    nameRow->addWidget(m_classNameInput);
    inputLayout->addLayout(nameRow);

    // Prompt input
    m_promptInput = new QTextEdit(this);
    m_promptInput->setPlaceholderText(
        "Beschreibe die gewünschte Spiellogik in natürlicher Sprache...\n\n"
        "Beispiele:\n"
        "• Erstelle ein System, das alle Entities mit einer TransformComponent in Richtung des Spielers bewegt\n"
        "• Erstelle eine Gesundheitskomponente mit Schaden-nehmen und Heilen-Funktionen\n"
        "• Erstelle ein KI-Verhalten, das zwischen Patrouillieren, Jagen und Fliehen wechselt"
    );
    m_promptInput->setMinimumHeight(100);
    inputLayout->addWidget(m_promptInput);

    // Buttons
    auto* btnRow = new QHBoxLayout();
    m_generateBtn = new QPushButton("✨ Generieren", this);
    m_generateBtn->setEnabled(false);
    m_generateBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px 16px; }");
    m_clearBtn = new QPushButton("🗑️ Löschen", this);
    btnRow->addWidget(m_generateBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addStretch();
    inputLayout->addLayout(btnRow);

    splitter->addWidget(inputGroup);

    // ========== OUTPUT SECTION ==========
    auto* outputGroup = new QGroupBox("Generierter Code", this);
    auto* outputLayout = new QVBoxLayout(outputGroup);

    // Progress
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    outputLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Bereit.", this);
    outputLayout->addWidget(m_statusLabel);

    // Tab widget for header/source
    m_outputTabs = new QTabWidget(this);
    
    // Header tab
    auto* headerWidget = new QWidget(this);
    auto* headerLayout = new QVBoxLayout(headerWidget);
    m_headerOutput = new QPlainTextEdit(this);
    m_headerOutput->setReadOnly(true);
    m_headerOutput->setFont(QFont("Consolas", 10));
    m_headerOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
    headerLayout->addWidget(m_headerOutput);
    m_copyHeaderBtn = new QPushButton("📋 Header kopieren", this);
    headerLayout->addWidget(m_copyHeaderBtn);
    m_outputTabs->addTab(headerWidget, "📄 Header (.h)");

    // Source tab
    auto* sourceWidget = new QWidget(this);
    auto* sourceLayout = new QVBoxLayout(sourceWidget);
    m_sourceOutput = new QPlainTextEdit(this);
    m_sourceOutput->setReadOnly(true);
    m_sourceOutput->setFont(QFont("Consolas", 10));
    m_sourceOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
    sourceLayout->addWidget(m_sourceOutput);
    m_copySourceBtn = new QPushButton("📋 Source kopieren", this);
    sourceLayout->addWidget(m_copySourceBtn);
    m_outputTabs->addTab(sourceWidget, "📄 Source (.cpp)");

    outputLayout->addWidget(m_outputTabs);

    // Action buttons
    auto* actionRow = new QHBoxLayout();
    m_saveBtn = new QPushButton("💾 Speichern", this);
    m_saveBtn->setEnabled(false);
    m_addToProjectBtn = new QPushButton("➕ Zum Projekt hinzufügen", this);
    m_addToProjectBtn->setEnabled(false);
    m_addToProjectBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; }");
    actionRow->addWidget(m_saveBtn);
    actionRow->addWidget(m_addToProjectBtn);
    actionRow->addStretch();
    outputLayout->addLayout(actionRow);

    splitter->addWidget(outputGroup);

    // ========== HISTORY SECTION ==========
    auto* historyGroup = new QGroupBox("Verlauf", this);
    auto* historyLayout = new QVBoxLayout(historyGroup);
    m_historyList = new QListWidget(this);
    m_historyList->setMaximumHeight(100);
    historyLayout->addWidget(m_historyList);
    splitter->addWidget(historyGroup);

    // Set splitter sizes
    splitter->setSizes({300, 400, 100});

    mainLayout->addWidget(splitter);
    setWidget(mainWidget);
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
        QMessageBox::warning(this, "Fehler", "Logic Copilot nicht initialisiert.\nBitte API-Schlüssel in den Einstellungen konfigurieren.");
        return;
    }

    QString prompt = m_promptInput->toPlainText().trimmed();
    if (prompt.isEmpty())
    {
        QMessageBox::warning(this, "Eingabe erforderlich", "Bitte gib eine Beschreibung für den zu generierenden Code ein.");
        return;
    }

    UpdateUIState(true);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText("Generiere Code...");

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
    m_statusLabel->setText("Bereit.");
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_saveBtn->setEnabled(false);
    m_addToProjectBtn->setEnabled(false);
}

void EditorLogicCopilotPanel::OnCopyHeaderClicked()
{
    QApplication::clipboard()->setText(m_headerOutput->toPlainText());
    m_statusLabel->setText("Header in Zwischenablage kopiert.");
}

void EditorLogicCopilotPanel::OnCopySourceClicked()
{
    QApplication::clipboard()->setText(m_sourceOutput->toPlainText());
    m_statusLabel->setText("Source in Zwischenablage kopiert.");
}

void EditorLogicCopilotPanel::OnSaveClicked()
{
    if (m_lastGeneratedClassName.isEmpty())
    {
        QMessageBox::warning(this, "Fehler", "Kein generierter Code zum Speichern.");
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(this, "Speicherort wählen");
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

    m_statusLabel->setText("Gespeichert: " + m_lastGeneratedClassName + ".h/.cpp");
    emit CodeGenerated(m_lastGeneratedClassName, headerPath, sourcePath);
}

void EditorLogicCopilotPanel::OnAddToProjectClicked()
{
    if (!m_copilot || m_lastGeneratedClassName.isEmpty())
    {
        QMessageBox::warning(this, "Fehler", "Kein generierter Code zum Hinzufügen.");
        return;
    }

    // Find the last result and add to project
    auto* result = m_copilot->GetResult(m_currentRequestId.toStdString());
    if (result && result->status == Scripting::CodeGenerationStatus::Ready)
    {
        if (m_copilot->AddToProject(result->code))
        {
            m_statusLabel->setText("Zum Projekt hinzugefügt: " + m_lastGeneratedClassName);
            emit AddToProject(m_lastGeneratedClassName);
            QMessageBox::information(this, "Erfolg", 
                "Code wurde zum Projekt hinzugefügt.\n\n"
                "Du musst CMake neu konfigurieren und neu builden, um den Code zu verwenden.");
        }
        else
        {
            QMessageBox::warning(this, "Fehler", "Konnte Code nicht zum Projekt hinzufügen.");
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
            "Beschreibe ein ECS-System...\n\n"
            "Beispiele:\n"
            "• Erstelle ein System das alle Entities mit Velocity auf ihre Position anwendet\n"
            "• Erstelle ein Partikel-Update-System für Explosionseffekte\n"
            "• Erstelle ein KI-Navigations-System"
        );
    }
    else if (type == "Component")
    {
        m_promptInput->setPlaceholderText(
            "Beschreibe eine ECS-Komponente...\n\n"
            "Beispiele:\n"
            "• Erstelle eine Gesundheitskomponente mit max HP und Regeneration\n"
            "• Erstelle eine Inventar-Komponente mit Slots und Gewichtslimit\n"
            "• Erstelle eine Damage-Komponente für Schadensquellen"
        );
    }
    else if (type == "Behavior")
    {
        m_promptInput->setPlaceholderText(
            "Beschreibe ein State-Machine-Verhalten...\n\n"
            "Beispiele:\n"
            "• Erstelle ein Wächter-Verhalten: Patrouillieren, Untersuchen, Verfolgen\n"
            "• Erstelle ein NPC-Verhalten: Idle, Wandern, Interagieren, Fliehen\n"
            "• Erstelle ein Boss-Verhalten mit verschiedenen Angriffsphasen"
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
        case 1: // Bewegungssystem
            m_systemTypeCombo->setCurrentIndex(0);
            m_promptInput->setText(
                "Erstelle ein Bewegungssystem, das alle Entities mit einer VelocityComponent bewegt.\n"
                "Das System soll:\n"
                "- Die Velocity auf die Position anwenden (position += velocity * deltaTime)\n"
                "- Optional Drag/Friction anwenden\n"
                "- Optional Geschwindigkeitsbegrenzung haben"
            );
            m_classNameInput->setText("MovementSystem");
            break;
            
        case 2: // Gesundheitskomponente
            m_systemTypeCombo->setCurrentIndex(1);
            m_promptInput->setText(
                "Erstelle eine Gesundheitskomponente mit folgenden Features:\n"
                "- Aktuelle und maximale Gesundheit (float)\n"
                "- TakeDamage(float amount) Methode\n"
                "- Heal(float amount) Methode\n"
                "- IsDead() Check\n"
                "- Optional: OnDeath Callback"
            );
            m_classNameInput->setText("HealthComponent");
            break;
            
        case 3: // KI-Patrouille
            m_systemTypeCombo->setCurrentIndex(2);
            m_promptInput->setText(
                "Erstelle ein KI-Patrouille-Verhalten mit diesen Zuständen:\n"
                "- Patrolling: Bewegt sich zwischen Wegpunkten\n"
                "- Investigating: Geht zu einem verdächtigen Ort\n"
                "- Chasing: Verfolgt den Spieler\n"
                "- Returning: Kehrt zur Patrouille zurück\n"
                "Übergänge basierend auf Sichtweite und Distanz zum Spieler"
            );
            m_classNameInput->setText("PatrolBehavior");
            break;
            
        case 4: // Spawner-System
            m_systemTypeCombo->setCurrentIndex(0);
            m_promptInput->setText(
                "Erstelle ein Spawner-System, das:\n"
                "- Entities mit SpawnerComponent findet\n"
                "- In regelmäßigen Intervallen neue Entities spawnt\n"
                "- Maximalzahl gleichzeitiger Spawns respektiert\n"
                "- Optional zufällige Position im Radius verwendet"
            );
            m_classNameInput->setText("SpawnerSystem");
            break;
            
        case 5: // Schadens-System
            m_systemTypeCombo->setCurrentIndex(0);
            m_promptInput->setText(
                "Erstelle ein Schadens-System, das:\n"
                "- DamageEvent-Komponenten verarbeitet\n"
                "- Schaden auf HealthComponents anwendet\n"
                "- Kritische Treffer berechnet\n"
                "- Rüstung/Resistenzen berücksichtigt\n"
                "- Schadens-Events nach Verarbeitung entfernt"
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
