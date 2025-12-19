#pragma once

#include <QDialog>

#include "Aetherion/Editor/EditorSettings.h"

class QCheckBox;
class QSpinBox;

namespace Aetherion::Editor
{
class EditorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditorSettingsDialog(const EditorSettings& current, QWidget* parent = nullptr);
    ~EditorSettingsDialog() override = default;

    [[nodiscard]] EditorSettings GetSettings() const;

private:
    QCheckBox* m_validation = nullptr;
    QCheckBox* m_verboseLogging = nullptr;
    QSpinBox* m_targetFps = nullptr;
    QSpinBox* m_headlessSleep = nullptr;
};
} // namespace Aetherion::Editor
