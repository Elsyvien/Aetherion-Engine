#include "Aetherion/Editor/EditorSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorSettingsDialog::EditorSettingsDialog(const EditorSettings& current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);
    setMinimumWidth(500);

    auto* rootLayout = new QVBoxLayout(this);
    
    m_tabWidget = new QTabWidget(this);
    
    // Rendering Tab
    auto* renderingTab = new QWidget();
    setupRenderingTab(renderingTab, current);
    m_tabWidget->addTab(renderingTab, tr("Rendering"));
    
    // AI/LLM Tab
    auto* aiTab = new QWidget();
    setupAITab(aiTab, current);
    m_tabWidget->addTab(aiTab, tr("AI Generation"));
    
    rootLayout->addWidget(m_tabWidget);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);

    setLayout(rootLayout);
}

void EditorSettingsDialog::setupRenderingTab(QWidget* tab, const EditorSettings& current)
{
    auto* layout = new QVBoxLayout(tab);
    auto* form = new QFormLayout();

    m_validation = new QCheckBox(tab);
    m_validation->setChecked(current.validationEnabled);
    form->addRow(tr("Enable Vulkan Validation"), m_validation);

    m_verboseLogging = new QCheckBox(tab);
    m_verboseLogging->setChecked(current.verboseLogging);
    form->addRow(tr("Verbose Rendering Logs"), m_verboseLogging);

    m_targetFps = new QSpinBox(tab);
    m_targetFps->setRange(1, 240);
    m_targetFps->setValue(current.targetFps);
    m_targetFps->setSuffix(tr(" fps"));
    form->addRow(tr("Target Frame Rate"), m_targetFps);

    m_headlessSleep = new QSpinBox(tab);
    m_headlessSleep->setRange(0, 1000);
    m_headlessSleep->setValue(current.headlessSleepMs);
    m_headlessSleep->setSuffix(tr(" ms"));
    form->addRow(tr("Sleep When Headless/Minimized"), m_headlessSleep);

    layout->addLayout(form);
    layout->addStretch();
}

void EditorSettingsDialog::setupAITab(QWidget* tab, const EditorSettings& current)
{
    auto* layout = new QVBoxLayout(tab);
    
    // Provider Selection Group
    auto* providerGroup = new QGroupBox(tr("AI Provider"), tab);
    auto* providerLayout = new QFormLayout(providerGroup);
    
    m_llmProvider = new QComboBox(tab);
    m_llmProvider->addItem(tr("Disabled (Procedural Only)"), static_cast<int>(LLMProviderType::None));
    m_llmProvider->addItem(tr("OpenAI (GPT-4, DALL-E 3)"), static_cast<int>(LLMProviderType::OpenAI));
    m_llmProvider->addItem(tr("Anthropic (Claude)"), static_cast<int>(LLMProviderType::Anthropic));
    m_llmProvider->addItem(tr("Stability AI (Stable Diffusion)"), static_cast<int>(LLMProviderType::StabilityAI));
    m_llmProvider->addItem(tr("Local Ollama"), static_cast<int>(LLMProviderType::LocalOllama));
    m_llmProvider->addItem(tr("Custom Endpoint"), static_cast<int>(LLMProviderType::Custom));
    
    int providerIndex = m_llmProvider->findData(static_cast<int>(current.llm.provider));
    if (providerIndex >= 0) {
        m_llmProvider->setCurrentIndex(providerIndex);
    }
    connect(m_llmProvider, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EditorSettingsDialog::onProviderChanged);
    providerLayout->addRow(tr("Provider:"), m_llmProvider);
    
    layout->addWidget(providerGroup);
    
    // API Configuration Group
    auto* apiGroup = new QGroupBox(tr("API Configuration"), tab);
    auto* apiLayout = new QFormLayout(apiGroup);
    
    m_apiKey = new QLineEdit(tab);
    m_apiKey->setEchoMode(QLineEdit::Password);
    m_apiKey->setPlaceholderText(tr("Enter your API key..."));
    m_apiKey->setText(QString::fromStdString(current.llm.apiKey));
    apiLayout->addRow(tr("API Key:"), m_apiKey);
    
    m_endpoint = new QLineEdit(tab);
    m_endpoint->setPlaceholderText(tr("Auto-detected from provider"));
    m_endpoint->setText(QString::fromStdString(current.llm.endpoint));
    apiLayout->addRow(tr("Endpoint:"), m_endpoint);
    
    m_model = new QLineEdit(tab);
    m_model->setPlaceholderText(tr("Default model for provider"));
    m_model->setText(QString::fromStdString(current.llm.model));
    apiLayout->addRow(tr("Text Model:"), m_model);
    
    m_imageModel = new QLineEdit(tab);
    m_imageModel->setPlaceholderText(tr("Default image model"));
    m_imageModel->setText(QString::fromStdString(current.llm.imageModel));
    apiLayout->addRow(tr("Image Model:"), m_imageModel);
    
    m_timeout = new QSpinBox(tab);
    m_timeout->setRange(5000, 300000);
    m_timeout->setSingleStep(5000);
    m_timeout->setValue(current.llm.timeoutMs);
    m_timeout->setSuffix(tr(" ms"));
    apiLayout->addRow(tr("Timeout:"), m_timeout);
    
    m_enableLogging = new QCheckBox(tr("Log API requests"), tab);
    m_enableLogging->setChecked(current.llm.enableLogging);
    apiLayout->addRow(tr("Debug:"), m_enableLogging);
    
    // Test connection button
    auto* testRow = new QHBoxLayout();
    auto* testBtn = new QPushButton(tr("Test Connection"), tab);
    connect(testBtn, &QPushButton::clicked, this, &EditorSettingsDialog::onTestConnection);
    testRow->addWidget(testBtn);
    testRow->addStretch();
    apiLayout->addRow("", testRow);
    
    layout->addWidget(apiGroup);
    
    // Info label
    auto* infoLabel = new QLabel(tab);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: #888; font-size: 11px;");
    infoLabel->setText(tr(
        "Configure an AI provider to enable real AI-powered asset generation. "
        "Without configuration, the engine uses procedural generation (patterns, noise, etc.).\n\n"
        "Supported features by provider:\n"
        "• OpenAI: Textures (DALL-E), Scripts (GPT-4), Meshes (GPT-4)\n"
        "• Anthropic: Scripts (Claude), Meshes (Claude)\n"
        "• Stability AI: Textures only (Stable Diffusion)\n"
        "• Ollama: Scripts, Meshes (local, no API key needed)"
    ));
    layout->addWidget(infoLabel);
    
    layout->addStretch();
    
    // Update fields based on current provider
    updateAIFieldsFromProvider(current.llm.provider);
}

void EditorSettingsDialog::onProviderChanged(int index)
{
    auto provider = static_cast<LLMProviderType>(m_llmProvider->itemData(index).toInt());
    updateAIFieldsFromProvider(provider);
}

void EditorSettingsDialog::updateAIFieldsFromProvider(LLMProviderType provider)
{
    bool enabled = provider != LLMProviderType::None;
    bool needsApiKey = provider != LLMProviderType::None && provider != LLMProviderType::LocalOllama;
    bool needsEndpoint = provider == LLMProviderType::Custom || provider == LLMProviderType::LocalOllama;
    
    m_apiKey->setEnabled(needsApiKey);
    m_endpoint->setEnabled(enabled);
    m_model->setEnabled(enabled);
    m_imageModel->setEnabled(enabled);
    m_timeout->setEnabled(enabled);
    m_enableLogging->setEnabled(enabled);
    
    // Set placeholder hints based on provider
    switch (provider) {
        case LLMProviderType::OpenAI:
            m_endpoint->setPlaceholderText("https://api.openai.com/v1");
            m_model->setPlaceholderText("gpt-4o");
            m_imageModel->setPlaceholderText("dall-e-3");
            break;
        case LLMProviderType::Anthropic:
            m_endpoint->setPlaceholderText("https://api.anthropic.com/v1");
            m_model->setPlaceholderText("claude-sonnet-4-20250514");
            m_imageModel->setPlaceholderText("(Not supported - uses OpenAI)");
            break;
        case LLMProviderType::StabilityAI:
            m_endpoint->setPlaceholderText("https://api.stability.ai/v1");
            m_model->setPlaceholderText("(Text not supported)");
            m_imageModel->setPlaceholderText("stable-diffusion-xl-1024-v1-0");
            break;
        case LLMProviderType::LocalOllama:
            m_endpoint->setPlaceholderText("http://localhost:11434/v1");
            m_model->setPlaceholderText("llama3.1");
            m_imageModel->setPlaceholderText("(Not supported locally)");
            break;
        case LLMProviderType::Custom:
            m_endpoint->setPlaceholderText("https://your-api-endpoint.com/v1");
            m_model->setPlaceholderText("your-model-name");
            m_imageModel->setPlaceholderText("your-image-model");
            break;
        default:
            m_endpoint->setPlaceholderText("");
            m_model->setPlaceholderText("");
            m_imageModel->setPlaceholderText("");
            break;
    }
}

void EditorSettingsDialog::onTestConnection()
{
    auto settings = GetSettings();
    
    if (settings.llm.provider == LLMProviderType::None) {
        QMessageBox::information(this, tr("Test Connection"), 
            tr("AI generation is disabled. Select a provider first."));
        return;
    }
    
    if (settings.llm.apiKey.empty() && settings.llm.provider != LLMProviderType::LocalOllama) {
        QMessageBox::warning(this, tr("Test Connection"), 
            tr("Please enter an API key."));
        return;
    }
    
    // For now, just show a message - actual test would need to use LLMClient
    QMessageBox::information(this, tr("Test Connection"), 
        tr("Connection test will be performed when you click OK and try to generate an asset."));
}

EditorSettings EditorSettingsDialog::GetSettings() const
{
    EditorSettings settings{};
    
    // Rendering settings
    settings.validationEnabled = m_validation && m_validation->isChecked();
    settings.verboseLogging = m_verboseLogging && m_verboseLogging->isChecked();
    settings.targetFps = m_targetFps ? m_targetFps->value() : 60;
    settings.headlessSleepMs = m_headlessSleep ? m_headlessSleep->value() : 50;
    
    // LLM settings
    if (m_llmProvider) {
        settings.llm.provider = static_cast<LLMProviderType>(
            m_llmProvider->currentData().toInt());
    }
    if (m_apiKey) {
        settings.llm.apiKey = m_apiKey->text().toStdString();
    }
    if (m_endpoint) {
        settings.llm.endpoint = m_endpoint->text().toStdString();
    }
    if (m_model) {
        settings.llm.model = m_model->text().toStdString();
    }
    if (m_imageModel) {
        settings.llm.imageModel = m_imageModel->text().toStdString();
    }
    if (m_timeout) {
        settings.llm.timeoutMs = m_timeout->value();
    }
    if (m_enableLogging) {
        settings.llm.enableLogging = m_enableLogging->isChecked();
    }
    
    settings.Clamp();
    return settings;
}
} // namespace Aetherion::Editor
