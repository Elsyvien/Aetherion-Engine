#pragma once

#include <QWidget>

namespace Aetherion::Editor
{
class EditorAssetBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit EditorAssetBrowser(QWidget* parent = nullptr);
    ~EditorAssetBrowser() override = default;

    // TODO: Hook into asset registry and implement drag-and-drop.
};
} // namespace Aetherion::Editor
