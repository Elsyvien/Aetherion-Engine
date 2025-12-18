#include "Aetherion/Editor/EditorInspectorPanel.h"

#include <QLabel>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QVBoxLayout>

#include "Aetherion/Scene/Entity.h"
#include "Aetherion/Scene/TransformComponent.h"

namespace Aetherion::Editor
{
EditorInspectorPanel::EditorInspectorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* header = new QLabel(tr("Inspector"), this);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_content = new QWidget(m_scrollArea);
    m_contentLayout = new QVBoxLayout(m_content);
    m_content->setLayout(m_contentLayout);
    m_scrollArea->setWidget(m_content);

    layout->addWidget(header);
    layout->addWidget(m_scrollArea, 1);
    setLayout(layout);

    RebuildUi();
}

void EditorInspectorPanel::SetSelectedEntity(std::shared_ptr<Scene::Entity> entity)
{
    m_entity = std::move(entity);
    RebuildUi();
}

void EditorInspectorPanel::RebuildUi()
{
    if (!m_contentLayout)
    {
        return;
    }

    m_buildingUi = true;

    while (auto* item = m_contentLayout->takeAt(0))
    {
        if (auto* w = item->widget())
        {
            w->deleteLater();
        }
        delete item;
    }

    m_posX = nullptr;
    m_posY = nullptr;
    m_rotZ = nullptr;
    m_scaleX = nullptr;
    m_scaleY = nullptr;

    if (!m_entity)
    {
        auto* placeholder = new QLabel(tr("Select an entity to view details"), m_content);
        placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_contentLayout->addWidget(placeholder);
        m_contentLayout->addStretch(1);
        m_buildingUi = false;
        return;
    }

    auto* title = new QLabel(QString::fromStdString(m_entity->GetName().empty() ? std::string("Entity") : m_entity->GetName()), m_content);
    title->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_contentLayout->addWidget(title);

    auto transform = m_entity->GetComponent<Scene::TransformComponent>();
    if (!transform)
    {
        auto* noTransform = new QLabel(tr("No Transform component on selected entity."), m_content);
        noTransform->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        m_contentLayout->addWidget(noTransform);
        m_contentLayout->addStretch(1);
        m_buildingUi = false;
        return;
    }

    auto* formHost = new QWidget(m_content);
    auto* form = new QFormLayout(formHost);
    form->setLabelAlignment(Qt::AlignLeft);

    auto makeSpin = [formHost](double min, double max, double step) {
        auto* s = new QDoubleSpinBox(formHost);
        s->setRange(min, max);
        s->setSingleStep(step);
        s->setDecimals(3);
        return s;
    };

    m_posX = makeSpin(-10.0, 10.0, 0.01);
    m_posY = makeSpin(-10.0, 10.0, 0.01);
    m_rotZ = makeSpin(-180.0, 180.0, 1.0);
    m_scaleX = makeSpin(0.001, 10.0, 0.01);
    m_scaleY = makeSpin(0.001, 10.0, 0.01);

    m_posX->setValue(transform->GetPositionX());
    m_posY->setValue(transform->GetPositionY());
    m_rotZ->setValue(transform->GetRotationZDegrees());
    m_scaleX->setValue(transform->GetScaleX());
    m_scaleY->setValue(transform->GetScaleY());

    form->addRow(tr("Position X"), m_posX);
    form->addRow(tr("Position Y"), m_posY);
    form->addRow(tr("Rotation Z (deg)"), m_rotZ);
    form->addRow(tr("Scale X"), m_scaleX);
    form->addRow(tr("Scale Y"), m_scaleY);

    auto applyAndEmit = [this, transform]() {
        if (m_buildingUi || !m_entity)
        {
            return;
        }

        transform->SetPosition(static_cast<float>(m_posX->value()), static_cast<float>(m_posY->value()));
        transform->SetRotationZDegrees(static_cast<float>(m_rotZ->value()));
        transform->SetScale(static_cast<float>(m_scaleX->value()), static_cast<float>(m_scaleY->value()));

        emit transformChanged(m_entity->GetId(),
                              static_cast<float>(m_posX->value()),
                              static_cast<float>(m_posY->value()),
                              static_cast<float>(m_rotZ->value()),
                              static_cast<float>(m_scaleX->value()),
                              static_cast<float>(m_scaleY->value()));
    };

    connect(m_posX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
    connect(m_posY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
    connect(m_rotZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
    connect(m_scaleX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });
    connect(m_scaleY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [applyAndEmit](double) { applyAndEmit(); });

    formHost->setLayout(form);
    m_contentLayout->addWidget(formHost);
    m_contentLayout->addStretch(1);

    m_buildingUi = false;

    // Push initial values out to listeners (renderer).
    emit transformChanged(m_entity->GetId(),
                          static_cast<float>(m_posX->value()),
                          static_cast<float>(m_posY->value()),
                          static_cast<float>(m_rotZ->value()),
                          static_cast<float>(m_scaleX->value()),
                          static_cast<float>(m_scaleY->value()));
}
} // namespace Aetherion::Editor
