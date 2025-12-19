#pragma once

#include <QWidget>

class QTimer;

namespace Aetherion::Editor
{
class EditorViewport : public QWidget
{
    Q_OBJECT

public:
    explicit EditorViewport(QWidget* parent = nullptr);
    ~EditorViewport() override;

    QWidget* surfaceWidget() const { return m_surface; }

signals:
    // WId ist unter Windows i.d.R. ein HWND (kann in Rendering/Win32 zu HWND gecastet werden)
    void surfaceReady(WId nativeHandle, int width, int height);
    void surfaceResized(int width, int height);

protected:
    void showEvent(QShowEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    QWidget* m_surface = nullptr;
    bool m_emittedReady = false;
    QTimer* m_resizeDebounceTimer = nullptr;
};
} // namespace Aetherion::Editor
