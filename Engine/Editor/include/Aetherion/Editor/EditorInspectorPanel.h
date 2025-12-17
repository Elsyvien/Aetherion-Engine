#pragma once

#include <QWidget>

namespace Aetherion::Editor
{
class EditorInspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit EditorInspectorPanel(QWidget* parent = nullptr);
    ~EditorInspectorPanel() override = default;

    // TODO: Reflect selected entity components and allow editing.
};
} // namespace Aetherion::Editor
