#include "Aetherion/Editor/EditorViewport.h"

#include <QVBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWindow>

namespace Aetherion::Editor
{
EditorViewport::EditorViewport(QWidget* parent)
    : QWidget(parent)
{
    // Ensure the viewport widget itself expands.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(100, 100);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Use a simple native QWidget instead of QWindow+createWindowContainer.
    // This avoids positioning issues on macOS where QWindow may render at wrong location.
    m_surface = new QWidget(this);
    m_surface->setAttribute(Qt::WA_NativeWindow, true);
    m_surface->setAttribute(Qt::WA_PaintOnScreen, true);
    m_surface->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_surface->setAttribute(Qt::WA_NoSystemBackground, true);
    m_surface->setAutoFillBackground(false);
    
    // Policy for proper resizing - container must expand to fill layout
    m_surface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_surface->setMinimumSize(100, 100);

    layout->addWidget(m_surface, 1);

    setLayout(layout);

    // Debounce timer: emit resize only after user stops resizing for 50ms.
    m_resizeDebounceTimer = new QTimer(this);
    m_resizeDebounceTimer->setSingleShot(true);
    m_resizeDebounceTimer->setInterval(50);
    connect(m_resizeDebounceTimer, &QTimer::timeout, this, [this] {
        if (!m_surface)
        {
            return;
        }

        const QSize surfaceSize = m_surface->size();
        if (!surfaceSize.isEmpty())
        {
            emit surfaceResized(surfaceSize.width(), surfaceSize.height());
        }
    });
}

EditorViewport::~EditorViewport()
{
    // m_surface is a child widget, automatically deleted.
}

void EditorViewport::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);

    // Emit ready once the native handle exists.
    if (!m_emittedReady && m_surface)
    {
        // Force creation of the native window handle.
        WId handle = m_surface->winId();
        if (handle != 0)
        {
            m_emittedReady = true;
            const QSize surfaceSize = m_surface->size();
            emit surfaceReady(handle, surfaceSize.width(), surfaceSize.height());

            // Schedule a deferred resize in case layout adjusts after show.
            QTimer::singleShot(0, this, [this] {
                if (m_resizeDebounceTimer)
                {
                    m_resizeDebounceTimer->start();
                }
            });
        }
    }
}

void EditorViewport::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);

    // Restart debounce timer; actual resize will happen after user stops.
    if (m_resizeDebounceTimer)
    {
        m_resizeDebounceTimer->start();
    }
}
} // namespace Aetherion::Editor
