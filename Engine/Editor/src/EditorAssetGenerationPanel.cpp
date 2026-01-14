#include "Aetherion/Editor/EditorAssetGenerationPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <atomic>

#include "Aetherion/Assets/AssetGenerator.h"
#include "Aetherion/Assets/AssetRegistry.h"
#include "Aetherion/Assets/LLMClient.h"

namespace {
    std::atomic<bool> s_isProcessing{false};
    
    // Convert editor LLM provider to assets LLM provider
    Aetherion::Assets::LLMProvider ConvertProvider(
        Aetherion::Editor::LLMProviderType editorProvider) {
        switch (editorProvider) {
            case Aetherion::Editor::LLMProviderType::OpenAI:
                return Aetherion::Assets::LLMProvider::OpenAI;
            case Aetherion::Editor::LLMProviderType::Anthropic:
                return Aetherion::Assets::LLMProvider::Anthropic;
            case Aetherion::Editor::LLMProviderType::StabilityAI:
                return Aetherion::Assets::LLMProvider::StabilityAI;
            case Aetherion::Editor::LLMProviderType::LocalOllama:
                return Aetherion::Assets::LLMProvider::LocalOllama;
            case Aetherion::Editor::LLMProviderType::Custom:
                return Aetherion::Assets::LLMProvider::Custom;
            default:
                return Aetherion::Assets::LLMProvider::OpenAI;
        }
    }
}

namespace Aetherion::Editor {

EditorAssetGenerationPanel::EditorAssetGenerationPanel(QWidget *parent)
    : QDockWidget(tr("Asset Generation"), parent) {
    setObjectName("AssetGenerationPanel");
    setAllowedAreas(Qt::AllDockWidgetAreas);
    
    m_generationQueue = std::make_shared<Assets::GenerationQueue>();
    
    setupUI();
    
    // Setup progress callback
    m_generationQueue->SetProgressCallback(
        [this](const std::string &requestId, float progress,
               const std::string &message) {
            QMetaObject::invokeMethod(
                this, [this, requestId, progress, message]() {
                    onProgressUpdate(QString::fromStdString(requestId),
                                   progress, QString::fromStdString(message));
                },
                Qt::QueuedConnection);
        });
    
    // Setup timer for processing queue
    m_processTimer = new QTimer(this);
    connect(m_processTimer, &QTimer::timeout, this,
            &EditorAssetGenerationPanel::processQueue);
    m_processTimer->start(100);  // Process every 100ms
}

EditorAssetGenerationPanel::~EditorAssetGenerationPanel() = default;

void EditorAssetGenerationPanel::SetAssetRegistry(
    std::shared_ptr<Assets::AssetRegistry> registry) {
    m_assetRegistry = std::move(registry);
    
    if (m_assetRegistry && m_generationQueue) {
        // Set output directory to assets/_generated
        auto rootPath = m_assetRegistry->GetRootPath();
        if (!rootPath.empty()) {
            m_generationQueue->SetOutputDirectory(rootPath / "_generated");
        }
    }
}

void EditorAssetGenerationPanel::ConfigureLLMGenerator(const LLMSettings& settings) {
    if (settings.provider == LLMProviderType::None) {
        // Use default procedural generators - unregister any LLM generator
        m_generationQueue->UnregisterGenerator("LLMAssetGenerator");
        
        // Ensure we have at least the procedural generators
        if (!m_generationQueue->GetGeneratorForType("texture")) {
            m_generationQueue->RegisterGenerator(
                std::make_shared<Assets::ProceduralTextureGenerator>());
            m_generationQueue->RegisterGenerator(
                std::make_shared<Assets::StubAssetGenerator>());
        }
        return;
    }
    
    // Create LLM configuration
    Assets::LLMConfig config;
    config.provider = ConvertProvider(settings.provider);
    config.apiKey = settings.apiKey;
    config.endpoint = settings.endpoint;
    config.model = settings.model;
    config.imageModel = settings.imageModel;
    config.timeoutMs = settings.timeoutMs;
    config.enableLogging = settings.enableLogging;
    
    // Create and register LLM generator
    auto llmGenerator = Assets::CreateLLMAssetGenerator(config);
    if (llmGenerator) {
        // Unregister old LLM generator if exists
        m_generationQueue->UnregisterGenerator("LLMAssetGenerator");
        m_generationQueue->RegisterGenerator(llmGenerator);
    }
    
    // Keep procedural generator as fallback
    if (!m_generationQueue->GetGenerator("ProceduralTextureGenerator")) {
        m_generationQueue->RegisterGenerator(
            std::make_shared<Assets::ProceduralTextureGenerator>());
    }
}

void EditorAssetGenerationPanel::setupUI() {
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);
    m_mainLayout->setSpacing(8);
    
    // =========================================================================
    // Prompt Section
    // =========================================================================
    auto *promptGroup = new QGroupBox(tr("Generation Prompt"), m_centralWidget);
    auto *promptLayout = new QVBoxLayout(promptGroup);
    
    m_promptEdit = new QTextEdit(promptGroup);
    m_promptEdit->setPlaceholderText(
        tr("Describe the asset to generate...\n"
           "e.g. red brick texture, blue gradient, checkerboard"));
    m_promptEdit->setMinimumHeight(60);
    m_promptEdit->setMaximumHeight(80);
    promptLayout->addWidget(m_promptEdit);
    
    auto *promptOptionsLayout = new QHBoxLayout();
    
    auto *typeLabel = new QLabel(tr("Type:"), promptGroup);
    m_assetTypeCombo = new QComboBox(promptGroup);
    m_assetTypeCombo->addItem(tr("Texture"), "texture");
    m_assetTypeCombo->addItem(tr("Mesh"), "mesh");
    m_assetTypeCombo->addItem(tr("Audio"), "audio");
    m_assetTypeCombo->addItem(tr("Script"), "script");
    connect(m_assetTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &EditorAssetGenerationPanel::onAssetTypeChanged);
    
    auto *nameLabel = new QLabel(tr("Name:"), promptGroup);
    m_nameEdit = new QLineEdit(promptGroup);
    m_nameEdit->setPlaceholderText(tr("(auto-generated)"));
    
    promptOptionsLayout->addWidget(typeLabel);
    promptOptionsLayout->addWidget(m_assetTypeCombo);
    promptOptionsLayout->addSpacing(16);
    promptOptionsLayout->addWidget(nameLabel);
    promptOptionsLayout->addWidget(m_nameEdit, 1);
    
    promptLayout->addLayout(promptOptionsLayout);
    m_mainLayout->addWidget(promptGroup);
    
    // =========================================================================
    // Parameters Section
    // =========================================================================
    auto *paramsGroup = new QGroupBox(tr("Parameters"), m_centralWidget);
    auto *paramsLayout = new QFormLayout(paramsGroup);
    
    auto *sizeLayout = new QHBoxLayout();
    m_widthSpin = new QSpinBox(paramsGroup);
    m_widthSpin->setRange(32, 4096);
    m_widthSpin->setValue(256);
    m_widthSpin->setSuffix(" px");
    
    auto *xLabel = new QLabel("×", paramsGroup);
    
    m_heightSpin = new QSpinBox(paramsGroup);
    m_heightSpin->setRange(32, 4096);
    m_heightSpin->setValue(256);
    m_heightSpin->setSuffix(" px");
    
    sizeLayout->addWidget(m_widthSpin);
    sizeLayout->addWidget(xLabel);
    sizeLayout->addWidget(m_heightSpin);
    sizeLayout->addStretch();
    
    paramsLayout->addRow(tr("Size:"), sizeLayout);
    
    m_formatCombo = new QComboBox(paramsGroup);
    m_formatCombo->addItem("BMP", "bmp");
    m_formatCombo->addItem("PPM", "ppm");
    paramsLayout->addRow(tr("Format:"), m_formatCombo);
    
    m_mainLayout->addWidget(paramsGroup);
    
    // =========================================================================
    // Control Buttons
    // =========================================================================
    auto *buttonLayout = new QHBoxLayout();
    
    m_generateBtn = new QPushButton(tr("Generate"), m_centralWidget);
    m_generateBtn->setMinimumHeight(32);
    m_generateBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; "
        "padding: 6px 12px; border: none; border-radius: 3px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
        "QPushButton:disabled { background-color: #666; color: #999; }");
    connect(m_generateBtn, &QPushButton::clicked, this,
            &EditorAssetGenerationPanel::startGeneration);
    
    m_cancelBtn = new QPushButton(tr("Cancel"), m_centralWidget);
    m_cancelBtn->setMinimumHeight(32);
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setStyleSheet(
        "QPushButton { padding: 6px 12px; border-radius: 3px; }"
        "QPushButton:disabled { color: #666; }");
    connect(m_cancelBtn, &QPushButton::clicked, this,
            &EditorAssetGenerationPanel::cancelSelected);
    
    m_retryBtn = new QPushButton(tr("Retry"), m_centralWidget);
    m_retryBtn->setMinimumHeight(32);
    m_retryBtn->setEnabled(false);
    m_retryBtn->setStyleSheet(
        "QPushButton { padding: 6px 12px; border-radius: 3px; }"
        "QPushButton:disabled { color: #666; }");
    connect(m_retryBtn, &QPushButton::clicked, this,
            &EditorAssetGenerationPanel::retrySelected);
    
    buttonLayout->addWidget(m_generateBtn, 1);
    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->addWidget(m_retryBtn);
    
    m_mainLayout->addLayout(buttonLayout);
    
    // =========================================================================
    // Progress Section
    // =========================================================================
    auto *progressGroup = new QGroupBox(tr("Current Generation"), m_centralWidget);
    auto *progressLayout = new QVBoxLayout(progressGroup);
    
    m_progressBar = new QProgressBar(progressGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    progressLayout->addWidget(m_progressBar);
    
    m_statusLabel = new QLabel(tr("Ready"), progressGroup);
    m_statusLabel->setStyleSheet("color: #888;");
    progressLayout->addWidget(m_statusLabel);
    
    m_mainLayout->addWidget(progressGroup);
    
    // =========================================================================
    // History Section
    // =========================================================================
    auto *historyGroup = new QGroupBox(tr("Generation History"), m_centralWidget);
    auto *historyLayout = new QVBoxLayout(historyGroup);
    
    m_historyList = new QListWidget(historyGroup);
    m_historyList->setAlternatingRowColors(true);
    m_historyList->setMinimumHeight(80);
    m_historyList->setMaximumHeight(120);
    connect(m_historyList, &QListWidget::itemClicked, this,
            &EditorAssetGenerationPanel::onHistoryItemSelected);
    historyLayout->addWidget(m_historyList);
    
    m_clearHistoryBtn = new QPushButton(tr("Clear"), historyGroup);
    m_clearHistoryBtn->setMaximumWidth(80);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this,
            &EditorAssetGenerationPanel::clearHistory);
    auto *historyBtnLayout = new QHBoxLayout();
    historyBtnLayout->addStretch();
    historyBtnLayout->addWidget(m_clearHistoryBtn);
    historyLayout->addLayout(historyBtnLayout);
    
    m_mainLayout->addWidget(historyGroup);
    
    // =========================================================================
    // Details Section
    // =========================================================================
    auto *detailsGroup = new QGroupBox(tr("Details"), m_centralWidget);
    auto *detailsLayout = new QVBoxLayout(detailsGroup);
    
    m_detailsLabel = new QLabel(tr("Select a generation from history to see details."),
                               detailsGroup);
    m_detailsLabel->setWordWrap(true);
    m_detailsLabel->setStyleSheet("color: #888;");
    detailsLayout->addWidget(m_detailsLabel);
    
    m_mainLayout->addWidget(detailsGroup);
    
    // Stretch at bottom
    m_mainLayout->addStretch();
    
    setWidget(m_centralWidget);
}

void EditorAssetGenerationPanel::startGeneration() {
    QString prompt = m_promptEdit->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, tr("Empty Prompt"),
                            tr("Please enter a description of the asset to generate."));
        return;
    }
    
    Assets::GenerationRequest request;
    request.prompt = prompt.toStdString();
    request.assetType = getAssetTypeString(m_assetTypeCombo->currentIndex());
    request.targetId = m_nameEdit->text().toStdString();
    request.width = m_widthSpin->value();
    request.height = m_heightSpin->value();
    request.format = m_formatCombo->currentData().toString().toStdString();
    
    auto callback = [this](const Assets::GenerationResult &result) {
        QMetaObject::invokeMethod(
            this, [this, result]() {
                onGenerationComplete(
                    QString::fromStdString(result.assetId),
                    result.success,
                    QString::fromStdString(result.message));
            },
            Qt::QueuedConnection);
    };
    
    std::string requestId = m_generationQueue->QueueRequest(
        std::move(request), callback);
    
    m_currentRequestId = requestId;
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Queued: %1").arg(QString::fromStdString(requestId)));
    m_statusLabel->setStyleSheet("color: #2196F3;");
    
    // Add to history
    auto *item = new QListWidgetItem(
        QString("⏳ %1 (%2)")
            .arg(prompt.left(40))
            .arg(getAssetTypeName(m_assetTypeCombo->currentIndex())));
    item->setData(Qt::UserRole, QString::fromStdString(requestId));
    m_historyList->insertItem(0, item);
    m_requestItems[requestId] = item;
    
    // Register with asset registry if available
    if (m_assetRegistry) {
        Assets::AssetRegistry::AssetType type = Assets::AssetRegistry::AssetType::Other;
        if (request.assetType == "texture") {
            type = Assets::AssetRegistry::AssetType::Texture;
        } else if (request.assetType == "mesh") {
            type = Assets::AssetRegistry::AssetType::Mesh;
        } else if (request.assetType == "audio") {
            type = Assets::AssetRegistry::AssetType::Audio;
        } else if (request.assetType == "script") {
            type = Assets::AssetRegistry::AssetType::Script;
        }
        const std::string assetId =
            m_assetRegistry->RequestGenerativeAsset(prompt.toStdString(), type,
                                                    request.targetId);
        if (!requestId.empty()) {
            m_requestAssetIds[requestId] = assetId;
        }
        m_assetRegistry->UpdateGenerativeAssetStatus(
            assetId,
            Assets::AssetRegistry::GenerativeAssetStatus::Pending,
            "Queued for generation");
    }
    
    updateButtonStates();
}

void EditorAssetGenerationPanel::cancelSelected() {
    if (m_historyList->currentItem()) {
        QString requestId = m_historyList->currentItem()->data(Qt::UserRole).toString();
        if (m_generationQueue->CancelRequest(requestId.toStdString())) {
            m_historyList->currentItem()->setText(
                "❌ " + m_historyList->currentItem()->text().mid(2));
            m_statusLabel->setText(tr("Cancelled"));
            m_statusLabel->setStyleSheet("color: #FF9800;");
            if (m_assetRegistry) {
                auto it = m_requestAssetIds.find(requestId.toStdString());
                if (it != m_requestAssetIds.end()) {
                    m_assetRegistry->UpdateGenerativeAssetStatus(
                        it->second,
                        Assets::AssetRegistry::GenerativeAssetStatus::Failed,
                        "Cancelled");
                    emit requestAssetBrowserRefresh();
                }
            }
        }
    }
    updateButtonStates();
}

void EditorAssetGenerationPanel::retrySelected() {
    if (m_historyList->currentItem()) {
        QString requestId = m_historyList->currentItem()->data(Qt::UserRole).toString();
        if (m_generationQueue->RetryRequest(requestId.toStdString())) {
            m_historyList->currentItem()->setText(
                "🔄 " + m_historyList->currentItem()->text().mid(2));
            m_statusLabel->setText(tr("Retrying..."));
            m_statusLabel->setStyleSheet("color: #2196F3;");
            if (m_assetRegistry) {
                auto it = m_requestAssetIds.find(requestId.toStdString());
                if (it != m_requestAssetIds.end()) {
                    m_assetRegistry->UpdateGenerativeAssetStatus(
                        it->second,
                        Assets::AssetRegistry::GenerativeAssetStatus::Pending,
                        "Retry queued");
                    emit requestAssetBrowserRefresh();
                }
            }
        }
    }
    updateButtonStates();
}

void EditorAssetGenerationPanel::clearHistory() {
    m_generationQueue->ClearHistory();
    m_historyList->clear();
    m_requestItems.clear();
    m_requestAssetIds.clear();
    m_detailsLabel->setText(tr("Select a generation from history to see details."));
    updateButtonStates();
}

void EditorAssetGenerationPanel::processQueue() {
    // Avoid blocking UI - only start processing if not already running
    if (s_isProcessing.exchange(true)) {
        return;  // Already processing in background
    }
    
    // Run generation in background thread
    auto queue = m_generationQueue;
    (void)QtConcurrent::run([queue]() {
        queue->ProcessNext();
        s_isProcessing.store(false);
    });
}

void EditorAssetGenerationPanel::onAssetTypeChanged(int index) {
    // Enable/disable size parameters based on asset type
    bool isTexture = (index == 0);
    m_widthSpin->setEnabled(isTexture);
    m_heightSpin->setEnabled(isTexture);
    m_formatCombo->setEnabled(isTexture);
}

void EditorAssetGenerationPanel::onHistoryItemSelected(QListWidgetItem *item) {
    if (!item) return;

    QString requestId = item->data(Qt::UserRole).toString();
    if (requestId.isEmpty()) {
        m_detailsLabel->setText(tr("No request data found for this history entry."));
        updateButtonStates();
        return;
    }

    if (!m_generationQueue) {
        m_detailsLabel->setText(tr("Generation system not available."));
        updateButtonStates();
        return;
    }

    auto state = m_generationQueue->GetRequestState(requestId.toStdString());

    if (state) {
        QString details;
        details += tr("<b>Request ID:</b> %1<br>").arg(requestId);
        details += tr("<b>Status:</b> %1<br>")
            .arg(QString::fromStdString(state->statusMessage));
        details += tr("<b>Progress:</b> %1%<br>")
            .arg(static_cast<int>(state->progress * 100));

        if (state->result) {
            QString filename = tr("<unknown>");
            try {
                const auto u8name = state->result->outputPath.filename().u8string();
                filename = QString::fromUtf8(reinterpret_cast<const char*>(u8name.data()),
                                            static_cast<int>(u8name.size()));
            } catch (...) {
                filename = tr("<unavailable>");
            }
            details += tr("<b>Output:</b> %1<br>")
                .arg(filename);
            details += tr("<b>Generation Time:</b> %1 ms<br>")
                .arg(state->result->generationTimeMs);
        }

        m_detailsLabel->setText(details);
    } else {
        m_detailsLabel->setText(tr("Request not found (history may have been cleared)."));
    }

    updateButtonStates();
}

void EditorAssetGenerationPanel::onProgressUpdate(const QString &requestId,     
                                                  float progress,
                                                  const QString &message) {     
    if (requestId.toStdString() == m_currentRequestId) {
        m_progressBar->setValue(static_cast<int>(progress * 100));
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet("color: #2196F3;");
    }

    // Update history item
    auto it = m_requestItems.find(requestId.toStdString());
    if (it != m_requestItems.end()) {
        QString text = it->second->text();
        if (text.startsWith("⏳")) {
            it->second->setText("🔄" + text.mid(1));
        }
    }

    if (m_assetRegistry && progress < 1.0f) {
        auto assetIt = m_requestAssetIds.find(requestId.toStdString());
        if (assetIt != m_requestAssetIds.end()) {
            const std::string statusMessage =
                message.isEmpty() ? "Generating..." : message.toStdString();
            m_assetRegistry->UpdateGenerativeAssetStatus(
                assetIt->second,
                Assets::AssetRegistry::GenerativeAssetStatus::Generating,
                statusMessage);
            emit requestAssetBrowserRefresh();
        }
    }
}

void EditorAssetGenerationPanel::onGenerationComplete(const QString &requestId,
                                                      bool success,
                                                      const QString &message) {
    auto it = m_requestItems.find(requestId.toStdString());
    if (it != m_requestItems.end()) {
        QString text = it->second->text();
        QString prefix = success ? "✅" : "❌";
        if (text.length() > 1) {
            it->second->setText(prefix + text.mid(1));
        }
    }
    
    if (requestId.toStdString() == m_currentRequestId) {
        m_progressBar->setValue(success ? 100 : 0);
        m_statusLabel->setText(message);
        m_statusLabel->setStyleSheet(success ? "color: #4CAF50;" : "color: #F44336;");
    }
    
    if (success) {
        emit assetGenerated(requestId, message);
        emit requestAssetBrowserRefresh();

        // Update asset registry
        if (m_assetRegistry) {
            auto state = m_generationQueue->GetRequestState(requestId.toStdString());
            if (state && state->result) {
                const std::string fallbackAssetId = state->result->assetId;
                auto assetIt = m_requestAssetIds.find(requestId.toStdString());
                const std::string assetId = (assetIt != m_requestAssetIds.end())
                                                ? assetIt->second
                                                : fallbackAssetId;
                m_assetRegistry->UpdateGenerativeAssetStatus(
                    assetId,
                    Assets::AssetRegistry::GenerativeAssetStatus::Ready,
                    message.toStdString(),
                    state->result->outputPath);
                m_assetRegistry->FinalizeGenerativeAsset(
                    assetId, state->result->outputPath);
            }
        }
    } else {
        emit generationFailed(requestId, message);
        if (m_assetRegistry) {
            auto assetIt = m_requestAssetIds.find(requestId.toStdString());
            if (assetIt != m_requestAssetIds.end()) {
                m_assetRegistry->UpdateGenerativeAssetStatus(
                    assetIt->second,
                    Assets::AssetRegistry::GenerativeAssetStatus::Failed,
                    message.toStdString());
            }
        }
    }

    updateButtonStates();
    updateSelectedDetails();

    m_requestAssetIds.erase(requestId.toStdString());
}

void EditorAssetGenerationPanel::updateHistoryList() {
    // History is updated incrementally, no need to rebuild
}

void EditorAssetGenerationPanel::updateSelectedDetails() {
    if (m_historyList->currentItem()) {
        onHistoryItemSelected(m_historyList->currentItem());
    }
}

void EditorAssetGenerationPanel::updateButtonStates() {
    bool hasSelection = (m_historyList->currentItem() != nullptr);
    bool canCancel = false;
    bool canRetry = false;

    if (hasSelection) {
        QString requestId = m_historyList->currentItem()->data(Qt::UserRole).toString();
        if (!requestId.isEmpty() && m_generationQueue) {
            auto state = m_generationQueue->GetRequestState(requestId.toStdString());
            if (state) {
                canCancel = (state->status == Assets::GenerationStatus::Pending);
                canRetry = (state->status == Assets::GenerationStatus::Failed);
            }
        }
    }
    
    m_cancelBtn->setEnabled(canCancel);
    m_retryBtn->setEnabled(canRetry);
    m_clearHistoryBtn->setEnabled(m_historyList->count() > 0);
}

QString EditorAssetGenerationPanel::getAssetTypeName(int index) const {
    switch (index) {
        case 0: return tr("Texture");
        case 1: return tr("Mesh");
        case 2: return tr("Audio");
        case 3: return tr("Script");
        default: return tr("Unknown");
    }
}

std::string EditorAssetGenerationPanel::getAssetTypeString(int index) const {
    switch (index) {
        case 0: return "texture";
        case 1: return "mesh";
        case 2: return "audio";
        case 3: return "script";
        default: return "other";
    }
}

} // namespace Aetherion::Editor
