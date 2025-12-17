#pragma once

#include <QWidget>

namespace Aetherion::Editor
{
class EditorViewport : public QWidget
{
    Q_OBJECT

public:
    explicit EditorViewport(QWidget* parent = nullptr);
    ~EditorViewport() override = default;

    // TODO: Connect viewport to rendering backend and input handling.
};
} // namespace Aetherion::Editor
