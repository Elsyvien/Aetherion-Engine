#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QGroupBox>

#include <memory>
#include <string>

namespace Aetherion::Scene
{
    class Scene;
}

namespace Aetherion::Scripting
{
    class LogicCopilot;
    struct CodeGenerationResult;
}

namespace Aetherion::Editor
{

/// @brief Widget panel for the Logic Copilot - NL-to-C++ code generation
/// Allows users to describe game logic in natural language and generates
/// ECS components, systems, and behaviors.
class EditorLogicCopilotPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorLogicCopilotPanel(QWidget* parent = nullptr);
    ~EditorLogicCopilotPanel() override;

    /// @brief Set the current scene for context
    void SetScene(Scene::Scene* scene);

    /// @brief Set the Logic Copilot instance
    void SetLogicCopilot(Scripting::LogicCopilot* copilot);

signals:
    /// @brief Emitted when code is generated successfully
    void CodeGenerated(const QString& className, const QString& headerPath, const QString& sourcePath);

    /// @brief Emitted when generated code should be added to the project
    void AddToProject(const QString& className);

private slots:
    void OnGenerateClicked();
    void OnClearClicked();
    void OnCopyHeaderClicked();
    void OnCopySourceClicked();
    void OnSaveClicked();
    void OnAddToProjectClicked();
    void OnCompileAndLoadClicked();
    void OnReloadModuleClicked();
    void OnHistoryItemSelected(QListWidgetItem* item);
    void OnSystemTypeChanged(int index);
    void OnTemplateSelected(int index);
    void UpdateProgress(const QString& requestId, float progress, const QString& status);

private:
    void SetupUI();
    void SetupConnections();
    void UpdateOutputDisplay(const Scripting::CodeGenerationResult& result);
    void AddToHistory(const QString& prompt, const QString& className);
    void ApplyTemplate(int templateIndex);
    void UpdateUIState(bool generating);

    // UI Elements - Input
    QComboBox* m_systemTypeCombo{nullptr};
    QComboBox* m_templateCombo{nullptr};
    QTextEdit* m_promptInput{nullptr};
    QLineEdit* m_classNameInput{nullptr};
    QPushButton* m_generateBtn{nullptr};
    QPushButton* m_clearBtn{nullptr};

    // UI Elements - Output
    QTabWidget* m_outputTabs{nullptr};
    QPlainTextEdit* m_headerOutput{nullptr};
    QPlainTextEdit* m_sourceOutput{nullptr};
    QPushButton* m_copyHeaderBtn{nullptr};
    QPushButton* m_copySourceBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_addToProjectBtn{nullptr};
    QPushButton* m_compileAndLoadBtn{nullptr};
    QPushButton* m_reloadModuleBtn{nullptr};

    // UI Elements - Status
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_statusLabel{nullptr};
    QListWidget* m_historyList{nullptr};

    // State
    Scene::Scene* m_scene{nullptr};
    Scripting::LogicCopilot* m_copilot{nullptr};
    QString m_currentRequestId;
    QString m_lastGeneratedClassName;
    QString m_lastLoadedModuleId;

    struct HistoryEntry
    {
        QString prompt;
        QString className;
        QString headerCode;
        QString sourceCode;
    };
    std::vector<HistoryEntry> m_history;
};

} // namespace Aetherion::Editor
