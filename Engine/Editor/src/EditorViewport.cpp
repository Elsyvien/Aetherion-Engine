#include "Aetherion/Editor/EditorViewport.h"

#include <QLabel>
#include <QVBoxLayout>

namespace Aetherion::Editor
{
EditorViewport::EditorViewport(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* placeholder = new QLabel(tr("Scene Viewport"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("background-color: #1e1e1e; color: #ffffff;");

    layout->addWidget(placeholder, 1);
    setLayout(layout);

    // TODO: Embed rendering surface and camera controls.
}
} // namespace Aetherion::Editor
