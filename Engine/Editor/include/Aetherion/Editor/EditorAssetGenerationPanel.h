#pragma once

#include <QDockWidget>
#include <memory>
#include <string>
#include <unordered_map>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QTimer;
class QVBoxLayout;

namespace Aetherion::Assets {
class AssetRegistry;
class GenerationQueue;
struct GenerationResult;
} // namespace Aetherion::Assets

namespace Aetherion::Editor {

/// @brief Editor panel for generating assets from text prompts
///
/// Provides a UI for:
/// - Entering generation prompts
/// - Selecting asset type (texture, mesh, audio, script)
/// - Configuring generation parameters
/// - Monitoring generation progress
/// - Viewing generation history
/// - Retrying failed generations
class EditorAssetGenerationPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit EditorAssetGenerationPanel(QWidget *parent = nullptr);
    ~EditorAssetGenerationPanel() override;

    /// @brief Set the asset registry for integration
    void SetAssetRegistry(std::shared_ptr<Assets::AssetRegistry> registry);

    /// @brief Get the generation queue
    [[nodiscard]] std::shared_ptr<Assets::GenerationQueue> GetGenerationQueue() const {
        return m_generationQueue;
    }

signals:
    /// @brief Emitted when an asset is successfully generated
    void assetGenerated(const QString &assetId, const QString &outputPath);

    /// @brief Emitted when generation fails
    void generationFailed(const QString &assetId, const QString &errorMessage);

    /// @brief Emitted when assets should be refreshed in the browser
    void requestAssetBrowserRefresh();

public slots:
    /// @brief Start generation with current settings
    void startGeneration();

    /// @brief Cancel selected pending generation
    void cancelSelected();

    /// @brief Retry selected failed generation
    void retrySelected();

    /// @brief Clear generation history
    void clearHistory();

    /// @brief Process pending generations
    void processQueue();

private slots:
    void onAssetTypeChanged(int index);
    void onHistoryItemSelected(QListWidgetItem *item);
    void onProgressUpdate(const QString &requestId, float progress,
                         const QString &message);
    void onGenerationComplete(const QString &requestId, bool success,
                             const QString &message);

private:
    void setupUI();
    void updateHistoryList();
    void updateSelectedDetails();
    void updateButtonStates();
    QString getAssetTypeName(int index) const;
    std::string getAssetTypeString(int index) const;

    // UI Components
    QWidget *m_centralWidget{nullptr};
    QVBoxLayout *m_mainLayout{nullptr};
    
    // Prompt section
    QTextEdit *m_promptEdit{nullptr};
    QComboBox *m_assetTypeCombo{nullptr};
    QLineEdit *m_nameEdit{nullptr};
    
    // Parameters section
    QSpinBox *m_widthSpin{nullptr};
    QSpinBox *m_heightSpin{nullptr};
    QComboBox *m_formatCombo{nullptr};
    
    // Control buttons
    QPushButton *m_generateBtn{nullptr};
    QPushButton *m_cancelBtn{nullptr};
    QPushButton *m_retryBtn{nullptr};
    QPushButton *m_clearHistoryBtn{nullptr};
    
    // Progress section
    QProgressBar *m_progressBar{nullptr};
    QLabel *m_statusLabel{nullptr};
    
    // History list
    QListWidget *m_historyList{nullptr};
    
    // Details section
    QLabel *m_detailsLabel{nullptr};
    
    // Timer for queue processing
    QTimer *m_processTimer{nullptr};
    
    // Generation system
    std::shared_ptr<Assets::GenerationQueue> m_generationQueue;
    std::shared_ptr<Assets::AssetRegistry> m_assetRegistry;
    
    // Request tracking
    std::unordered_map<std::string, QListWidgetItem*> m_requestItems;
    std::string m_currentRequestId;
};

} // namespace Aetherion::Editor
