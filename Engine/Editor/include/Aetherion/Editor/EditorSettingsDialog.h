#pragma once

#include <QDialog>

#include "Aetherion/Editor/EditorSettings.h"

class QCheckBox;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QTabWidget;

namespace Aetherion::Editor
{
class EditorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditorSettingsDialog(const EditorSettings& current, QWidget* parent = nullptr);
    ~EditorSettingsDialog() override = default;

    [[nodiscard]] EditorSettings GetSettings() const;

private slots:
    void onProviderChanged(int index);
    void onTestConnection();

private:
    void setupRenderingTab(QWidget* tab, const EditorSettings& current);
    void setupAITab(QWidget* tab, const EditorSettings& current);
    void updateAIFieldsFromProvider(LLMProviderType provider);

    QTabWidget* m_tabWidget = nullptr;
    
    // Rendering tab
    QCheckBox* m_validation = nullptr;
    QCheckBox* m_verboseLogging = nullptr;
    QSpinBox* m_targetFps = nullptr;
    QSpinBox* m_headlessSleep = nullptr;
    
    // AI/LLM tab
    QComboBox* m_llmProvider = nullptr;
    QLineEdit* m_apiKey = nullptr;
    QLineEdit* m_endpoint = nullptr;
    QLineEdit* m_model = nullptr;
    QLineEdit* m_imageModel = nullptr;
    QSpinBox* m_timeout = nullptr;
    QCheckBox* m_enableLogging = nullptr;
};
} // namespace Aetherion::Editor
