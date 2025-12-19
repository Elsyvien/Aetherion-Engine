#pragma once

#include <QWidget>

class QListWidget;

namespace Aetherion::Editor
{
class EditorAssetBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit EditorAssetBrowser(QWidget* parent = nullptr);
    ~EditorAssetBrowser() override = default;

signals:
    void AssetSelected(const QString& assetId);
    void AssetSelectionCleared();

    // TODO: Hook into asset registry and implement drag-and-drop.

private:
    QListWidget* m_list = nullptr;
};
} // namespace Aetherion::Editor
