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

signals:
    void AssetSelected(const QString& assetId);
    void AssetSelectionCleared();
    void AssetActivated();
    void RescanRequested();
    void AssetDroppedOnScene(const QString& assetId);
    void AssetDeleteRequested(const QString& assetId);
    void AssetRenameRequested(const QString& assetId);
    void AssetShowInExplorerRequested(const QString& assetId);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onFilterTextChanged(const QString& text);
    void showContextMenu(const QPoint& pos);

private:
    void updateVisibleItems();
    void setupContextMenu();

    QListWidget* m_list = nullptr;
    QToolButton* m_rescanButton = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QMenu* m_contextMenu = nullptr;
    std::vector<Item> m_allItems;
    QString m_filterText;
};
} // namespace Aetherion::Editor
