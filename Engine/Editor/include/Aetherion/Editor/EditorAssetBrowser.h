#pragma once

#include <QWidget>
#include <QString>
#include <vector>

class QListWidget;

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

signals:
    void AssetSelected(const QString& assetId);
    void AssetSelectionCleared();
    void AssetActivated();

    // TODO: Implement drag-and-drop and asset previews.

private:
    QListWidget* m_list = nullptr;
};
} // namespace Aetherion::Editor
