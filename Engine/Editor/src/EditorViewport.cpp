#include "Aetherion/Editor/EditorViewport.h"

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>

namespace Aetherion::Editor
{
EditorViewport::EditorViewport(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Native “Renderfläche” (hier rendert Vulkan rein)
    m_surface = new QWidget(this);

    // Wichtig: echtes natives Window-Handle erzwingen
    m_surface->setAttribute(Qt::WA_NativeWindow, true);
    m_surface->setAttribute(Qt::WA_DontCreateNativeAncestors, true);

    // Optional: Qt soll hier nicht “malen”
    m_surface->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_surface->setAttribute(Qt::WA_NoSystemBackground, true);

    m_surface->setStyleSheet("background-color: #1e1e1e;"); // nur als Fallback sichtbar
    layout->addWidget(m_surface, 1);

    setLayout(layout);
}

void EditorViewport::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);

    // Erst emittieren, wenn das native Handle sicher existiert
    if (!m_emittedReady && m_surface && m_surface->winId() != 0)
    {
        m_emittedReady = true;
        const auto s = m_surface->size();
        emit surfaceReady(m_surface->winId(), s.width(), s.height());
    }
}

void EditorViewport::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);

    if (m_surface)
    {
        const auto s = m_surface->size();
        emit surfaceResized(s.width(), s.height());
    }
}
} // namespace Aetherion::Editor
