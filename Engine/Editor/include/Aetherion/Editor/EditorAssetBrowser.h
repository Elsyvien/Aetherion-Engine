#pragma once

#include <QWidget>
#include <QString>
#include <vector>

class QListWidget;
class QToolButton;
class QLineEdit;
class QMenu;

namespace Aetherion::Editor
{
class EditorAssetBrowser : public QWidget
{
    Q_OBJECT

public:
    struct Item
    {
        QString label;
        QString id;
        bool isHeader{false};
    };

    explicit EditorAssetBrowser(QWidget* parent = nullptr);
    ~EditorAssetBrowser() override = default;

    void SetItems(const std::vector<Item>& items);
    void ClearSelection();
    
    QString GetSelectedAssetId() const;
    void FocusFilter();

    // History navigation
    void NavigateBack();
    void NavigateForward();

signals:
    void AssetSelected(QString id);
    void AssetActivated();
    void AssetDroppedOnScene(QString id);
    void AssetShowInExplorerRequested(QString id);
    void AssetRenameRequested(QString id);
    void AssetDeleteRequested(QString id);
    void AssetSelectionCleared();
    
    // New signal for navigation
    void NavigateToPathRequested(QString path);
    void RescanRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void setupContextMenu();
    void showContextMenu(const QPoint& pos);
    void updateVisibleItems();
    void updateNavigationButtons();
    void onFilterTextChanged(const QString& text);

    QListWidget* m_list = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QToolButton* m_rescanButton = nullptr;
    QToolButton* m_backButton = nullptr;
    QToolButton* m_forwardButton = nullptr;
    QMenu* m_contextMenu = nullptr;

    std::vector<Item> m_allItems;
    QString m_filterText;

    // History
    QString m_currentPath;
    std::vector<QString> m_backStack;
    std::vector<QString> m_forwardStack;
};
} // namespace Aetherion::Editor