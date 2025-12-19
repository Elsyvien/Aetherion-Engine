#include "Aetherion/Editor/EditorSettingsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorSettingsDialog::EditorSettingsDialog(const EditorSettings& current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setModal(true);

    auto* rootLayout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_validation = new QCheckBox(this);
    m_validation->setChecked(current.validationEnabled);
    form->addRow(tr("Enable Vulkan Validation"), m_validation);

    m_verboseLogging = new QCheckBox(this);
    m_verboseLogging->setChecked(current.verboseLogging);
    form->addRow(tr("Verbose Rendering Logs"), m_verboseLogging);

    m_targetFps = new QSpinBox(this);
    m_targetFps->setRange(1, 240);
    m_targetFps->setValue(current.targetFps);
    m_targetFps->setSuffix(tr(" fps"));
    form->addRow(tr("Target Frame Rate"), m_targetFps);

    m_headlessSleep = new QSpinBox(this);
    m_headlessSleep->setRange(0, 1000);
    m_headlessSleep->setValue(current.headlessSleepMs);
    m_headlessSleep->setSuffix(tr(" ms"));
    form->addRow(tr("Sleep When Headless/Minimized"), m_headlessSleep);

    rootLayout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);

    setLayout(rootLayout);
}

EditorSettings EditorSettingsDialog::GetSettings() const
{
    EditorSettings settings{};
    settings.validationEnabled = m_validation && m_validation->isChecked();
    settings.verboseLogging = m_verboseLogging && m_verboseLogging->isChecked();
    settings.targetFps = m_targetFps ? m_targetFps->value() : 60;
    settings.headlessSleepMs = m_headlessSleep ? m_headlessSleep->value() : 50;
    settings.Clamp();
    return settings;
}
} // namespace Aetherion::Editor
