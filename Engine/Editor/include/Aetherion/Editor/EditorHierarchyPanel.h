#pragma once

#include <QWidget>

namespace Aetherion::Editor
{
class EditorHierarchyPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorHierarchyPanel(QWidget* parent = nullptr);
    ~EditorHierarchyPanel() override = default;

    // TODO: Bind to scene graph and selection system.
};
} // namespace Aetherion::Editor
