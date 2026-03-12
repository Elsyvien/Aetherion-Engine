#pragma once

#include <QObject>
#include <QTabWidget>
#include <QWidget>
#include <QString>
#include <QSettings>
#include <memory>

class QSplitter;
class QVBoxLayout;
class QVariantAnimation;

class QMainWindow;

namespace Aetherion::Editor
{
/**
 * @brief Manages tabbed panel groups for the editor
 * 
 * This class organizes the editor UI into three main tabbed regions:
 * - Left Panel: Asset Browser & Hierarchy
 * - Right Panel: Inspector, Mesh Preview, Camera Preview, AI Copilot
 * - Bottom Panel: Console, Statistics, Animation, Logic Copilot
 */
class TabPanelManager : public QWidget
{
    Q_OBJECT

public:
    explicit TabPanelManager(QWidget* parent = nullptr);
    ~TabPanelManager() override;

    // Create the three main panel groups
    void CreatePanelGroups(QMainWindow* mainWindow);

    // Access the panel groups
    QTabWidget* GetLeftPanel() const { return m_leftPanel; }
    QTabWidget* GetRightPanel() const { return m_rightPanel; }
    QTabWidget* GetBottomPanel() const { return m_bottomPanel; }

    // Add widgets to specific panels
    void AddToLeftPanel(QWidget* widget, const QString& tabName);
    void AddToRightPanel(QWidget* widget, const QString& tabName);
    void AddToBottomPanel(QWidget* widget, const QString& tabName);

    // Set the central content widget (e.g., the 3D viewport)
    void SetCentralWidget(QWidget* widget);

    // Remove widgets from panels
    void RemoveFromLeftPanel(int index);
    void RemoveFromRightPanel(int index);
    void RemoveFromBottomPanel(int index);

    // Set active tabs
    void SetActiveLeftTab(int index);
    void SetActiveRightTab(int index);
    void SetActiveBottomTab(int index);

    // Get active tab indices
    int GetActiveLeftTabIndex() const;
    int GetActiveRightTabIndex() const;
    int GetActiveBottomTabIndex() const;

    // Save/Restore layout
    void SavePanelState(QSettings& settings, const QString& group = "PanelLayout") const;
    void RestorePanelState(QSettings& settings, const QString& group = "PanelLayout");

    // Panel visibility
    void SetBottomPanelVisible(bool visible);
    bool IsBottomPanelVisible() const;
    void ApplyDefaultPanelState();

private:
    QSplitter* m_verticalSplitter = nullptr;
    QSplitter* m_horizontalSplitter = nullptr;
    QWidget* m_centerContainer = nullptr;
    QTabWidget* m_leftPanel = nullptr;      // Asset Browser & Hierarchy        
    QTabWidget* m_rightPanel = nullptr;     // Inspector & Previews & Copilot   
    QTabWidget* m_bottomPanel = nullptr;    // Console & Statistics
    QVariantAnimation* m_bottomPanelAnimation = nullptr;

    void SetupPanelAnimations();
    void AnimateTabChange(QTabWidget* tabWidget, int index);
    void SetupPanelStyle();
};

} // namespace Aetherion::Editor
