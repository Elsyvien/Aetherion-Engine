#pragma once

#include <QWidget>

namespace Aetherion::Editor
{
class EditorConsole : public QWidget
{
    Q_OBJECT

public:
    explicit EditorConsole(QWidget* parent = nullptr);
    ~EditorConsole() override = default;

    // TODO: Connect to logging framework and filtering controls.
};
} // namespace Aetherion::Editor
