#include "Aetherion/Editor/TabPanelManager.h"

#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QMainWindow>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSettings>
#include <QSplitter>
#include <QStyleFactory>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>
#include <algorithm>

namespace Aetherion::Editor
{
namespace
{
bool HasNativeChild(QWidget* widget)
{
    if (!widget) {
        return false;
    }

    if (widget->testAttribute(Qt::WA_NativeWindow) ||
        widget->testAttribute(Qt::WA_PaintOnScreen)) {
        return true;
    }

    const auto children =
        widget->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (const auto* child : children) {
        if (!child) {
            continue;
        }
        if (child->testAttribute(Qt::WA_NativeWindow) ||
            child->testAttribute(Qt::WA_PaintOnScreen)) {
            return true;
        }
    }

    return false;
}
} // namespace


TabPanelManager::TabPanelManager(QWidget* parent)
    : QWidget(parent)
{
    // Main layout for the panel groups
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Vertical splitter separates main area and bottom panel
    m_verticalSplitter = new QSplitter(Qt::Vertical, this);
    m_verticalSplitter->setStyleSheet(
        "QSplitter::handle { background: #2a3140; height: 4px; }");
    m_verticalSplitter->setHandleWidth(4);

    // Horizontal splitter for left / center / right
    m_horizontalSplitter = new QSplitter(Qt::Horizontal, m_verticalSplitter);
    m_horizontalSplitter->setStyleSheet(
        "QSplitter::handle { background: #2a3140; width: 4px; }");
    m_horizontalSplitter->setHandleWidth(4);

    // Left panel (Assets & Hierarchy)
    m_leftPanel = new QTabWidget(this);
    m_leftPanel->setTabPosition(QTabWidget::North);
    m_leftPanel->setDocumentMode(true);
    m_leftPanel->setMinimumWidth(200);
    m_leftPanel->setMaximumWidth(600);

    // Center container (viewport goes here)
    m_centerContainer = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(m_centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    // Right panel (Inspector & Previews & Copilot)
    m_rightPanel = new QTabWidget(this);
    m_rightPanel->setTabPosition(QTabWidget::North);
    m_rightPanel->setDocumentMode(true);
    m_rightPanel->setMinimumWidth(250);
    m_rightPanel->setMaximumWidth(800);

    // Assemble horizontal splitter
    m_horizontalSplitter->addWidget(m_leftPanel);
    m_horizontalSplitter->addWidget(m_centerContainer);
    m_horizontalSplitter->addWidget(m_rightPanel);
    m_horizontalSplitter->setStretchFactor(0, 1);
    m_horizontalSplitter->setStretchFactor(1, 3);
    m_horizontalSplitter->setStretchFactor(2, 1);
    m_horizontalSplitter->setCollapsible(0, true);
    m_horizontalSplitter->setCollapsible(1, false);
    m_horizontalSplitter->setCollapsible(2, true);

    // Bottom panel (Console & Statistics & Animation & Logic Copilot)
    m_bottomPanel = new QTabWidget(this);
    m_bottomPanel->setTabPosition(QTabWidget::North);
    m_bottomPanel->setDocumentMode(true);
    m_bottomPanel->setMinimumHeight(150);
    m_bottomPanel->setMaximumHeight(500);

    // Assemble vertical splitter
    m_verticalSplitter->addWidget(m_horizontalSplitter);
    m_verticalSplitter->addWidget(m_bottomPanel);
    m_verticalSplitter->setStretchFactor(0, 3);
    m_verticalSplitter->setStretchFactor(1, 1);
    m_verticalSplitter->setCollapsible(1, true);

    mainLayout->addWidget(m_verticalSplitter);

    SetupPanelStyle();
    SetupPanelAnimations();

    setLayout(mainLayout);
}

TabPanelManager::~TabPanelManager() = default;

void TabPanelManager::CreatePanelGroups(QMainWindow* mainWindow)
{
    if (!mainWindow) {
        return;
    }

    // Configure tab widgets styling
    for (auto* tabWidget : { m_leftPanel, m_rightPanel, m_bottomPanel }) {
        if (tabWidget) {
            tabWidget->setStyleSheet(
                "QTabWidget::pane { border-top: 1px solid #242c3a; background: #131722; } "
                "QTabBar::tab { "
                "    background-color: #1b2230; "
                "    color: #b6beca; "
                "    padding: 8px 16px; "
                "    border: 1px solid #2a3140; "
                "    border-bottom: none; "
                "    border-top-left-radius: 6px; "
                "    border-top-right-radius: 6px; "
                "    margin-right: 4px; "
                "    min-width: 88px; "
                "} "
                "QTabBar::tab:selected { "
                "    background-color: #ff6b3d; "
                "    color: #141824; "
                "    border-color: #ff8b66; "
                "} "
                "QTabBar::tab:hover:!selected { "
                "    background-color: #2a3344; "
                "    color: #e6e3dc; "
                "}"
            );
        }
    }
}

void TabPanelManager::SetupPanelAnimations()
{
    for (auto* tabWidget : { m_leftPanel, m_rightPanel, m_bottomPanel }) {
        if (!tabWidget) {
            continue;
        }

        connect(tabWidget, &QTabWidget::currentChanged, this,
                [this, tabWidget](int index) {
                    AnimateTabChange(tabWidget, index);
                });
    }
}

void TabPanelManager::AnimateTabChange(QTabWidget* tabWidget, int index)
{
    if (!tabWidget || index < 0) {
        return;
    }

    auto* page = tabWidget->widget(index);
    if (!page || !page->isVisible()) {
        return;
    }

    const QRect endRect = page->geometry();
    if (!endRect.isValid()) {
        return;
    }

    const int offset = 8;
    const QRect startRect = endRect.translated(0, offset);
    page->setGeometry(startRect);

    auto* slide = new QPropertyAnimation(page, "geometry", page);
    slide->setDuration(220);
    slide->setStartValue(startRect);
    slide->setEndValue(endRect);
    slide->setEasingCurve(QEasingCurve::OutCubic);

    if (HasNativeChild(page)) {
        connect(slide, &QPropertyAnimation::finished, page, [page, endRect] {
            page->setGeometry(endRect);
        });
        slide->start(QAbstractAnimation::DeleteWhenStopped);
        return;
    }

    auto* opacityEffect =
        qobject_cast<QGraphicsOpacityEffect*>(page->graphicsEffect());
    if (page->graphicsEffect() && !opacityEffect) {
        connect(slide, &QPropertyAnimation::finished, page, [page, endRect] {
            page->setGeometry(endRect);
        });
        slide->start(QAbstractAnimation::DeleteWhenStopped);
        return;
    }

    if (!opacityEffect) {
        opacityEffect = new QGraphicsOpacityEffect(page);
        page->setGraphicsEffect(opacityEffect);
    }

    opacityEffect->setOpacity(0.0);

    auto* fade = new QPropertyAnimation(opacityEffect, "opacity", page);
    fade->setDuration(180);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);

    auto* group = new QParallelAnimationGroup(page);
    group->addAnimation(fade);
    group->addAnimation(slide);
    connect(group, &QParallelAnimationGroup::finished, page,
            [page, opacityEffect, endRect] {
                page->setGeometry(endRect);
                opacityEffect->setOpacity(1.0);
            });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void TabPanelManager::AddToLeftPanel(QWidget* widget, const QString& tabName)   
{
    if (m_leftPanel && widget) {
        m_leftPanel->addTab(widget, tabName);
    }
}

void TabPanelManager::AddToRightPanel(QWidget* widget, const QString& tabName)
{
    if (m_rightPanel && widget) {
        m_rightPanel->addTab(widget, tabName);
    }
}

void TabPanelManager::AddToBottomPanel(QWidget* widget, const QString& tabName)
{
    if (m_bottomPanel && widget) {
        m_bottomPanel->addTab(widget, tabName);
    }
}

void TabPanelManager::SetCentralWidget(QWidget* widget)
{
    if (!m_centerContainer) {
        return;
    }

    auto* layout = qobject_cast<QVBoxLayout*>(m_centerContainer->layout());
    if (!layout) {
        layout = new QVBoxLayout(m_centerContainer);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }

    // Remove any existing central widget
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (auto* oldWidget = item->widget()) {
            oldWidget->setParent(nullptr);
        }
        delete item;
    }

    if (widget) {
        widget->setParent(m_centerContainer);
        layout->addWidget(widget);
    }
}

void TabPanelManager::RemoveFromLeftPanel(int index)
{
    if (m_leftPanel && index >= 0 && index < m_leftPanel->count()) {
        m_leftPanel->removeTab(index);
    }
}

void TabPanelManager::RemoveFromRightPanel(int index)
{
    if (m_rightPanel && index >= 0 && index < m_rightPanel->count()) {
        m_rightPanel->removeTab(index);
    }
}

void TabPanelManager::RemoveFromBottomPanel(int index)
{
    if (m_bottomPanel && index >= 0 && index < m_bottomPanel->count()) {
        m_bottomPanel->removeTab(index);
    }
}

void TabPanelManager::SetActiveLeftTab(int index)
{
    if (m_leftPanel) {
        m_leftPanel->setCurrentIndex(index);
    }
}

void TabPanelManager::SetActiveRightTab(int index)
{
    if (m_rightPanel) {
        m_rightPanel->setCurrentIndex(index);
    }
}

void TabPanelManager::SetActiveBottomTab(int index)
{
    if (m_bottomPanel) {
        m_bottomPanel->setCurrentIndex(index);
    }
}

int TabPanelManager::GetActiveLeftTabIndex() const
{
    return m_leftPanel ? m_leftPanel->currentIndex() : -1;
}

int TabPanelManager::GetActiveRightTabIndex() const
{
    return m_rightPanel ? m_rightPanel->currentIndex() : -1;
}

int TabPanelManager::GetActiveBottomTabIndex() const
{
    return m_bottomPanel ? m_bottomPanel->currentIndex() : -1;
}

void TabPanelManager::SetBottomPanelVisible(bool visible)
{
    if (!m_bottomPanel || !m_verticalSplitter) {
        if (m_bottomPanel) {
            m_bottomPanel->setVisible(visible);
        }
        return;
    }

    QList<int> startSizes = m_verticalSplitter->sizes();
    if (startSizes.size() < 2) {
        m_bottomPanel->setVisible(visible);
        return;
    }

    const int total = startSizes[0] + startSizes[1];
    QList<int> endSizes = startSizes;

    if (!visible) {
        endSizes[0] = total;
        endSizes[1] = 0;
    } else {
        if (startSizes[1] == 0) {
            const int bottom = std::max(1, total / 4);
            endSizes[1] = bottom;
            endSizes[0] = std::max(1, total - bottom);
        }
    }

    if (startSizes == endSizes) {
        m_bottomPanel->setVisible(visible);
        m_verticalSplitter->setSizes(endSizes);
        return;
    }

    if (m_bottomPanelAnimation) {
        m_bottomPanelAnimation->stop();
        m_bottomPanelAnimation->deleteLater();
        m_bottomPanelAnimation = nullptr;
    }

    if (visible) {
        m_bottomPanel->setVisible(true);
    }

    auto* animation = new QVariantAnimation(this);
    m_bottomPanelAnimation = animation;
    animation->setDuration(220);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this,
            [this, startSizes, endSizes](const QVariant& value) {
                const double t = value.toDouble();
                QList<int> sizes;
                sizes.reserve(startSizes.size());
                for (int i = 0; i < startSizes.size(); ++i) {
                    const int start = startSizes[i];
                    const int end = endSizes[i];
                    sizes.append(static_cast<int>(start + (end - start) * t));
                }
                m_verticalSplitter->setSizes(sizes);
            });

    connect(animation, &QVariantAnimation::finished, this,
            [this, visible, endSizes] {
                if (m_verticalSplitter) {
                    m_verticalSplitter->setSizes(endSizes);
                }
                if (m_bottomPanel) {
                    m_bottomPanel->setVisible(visible);
                }
                if (m_bottomPanelAnimation) {
                    m_bottomPanelAnimation->deleteLater();
                    m_bottomPanelAnimation = nullptr;
                }
            });

    animation->start();
}

bool TabPanelManager::IsBottomPanelVisible() const
{
    return m_bottomPanel && m_bottomPanel->isVisible();
}

void TabPanelManager::SavePanelState(QSettings& settings, const QString& group) const
{
    settings.beginGroup(group);
    
    if (m_verticalSplitter) {
        settings.setValue("VerticalSplitterState", m_verticalSplitter->saveState());
    }
    if (m_horizontalSplitter) {
        settings.setValue("HorizontalSplitterState", m_horizontalSplitter->saveState());
    }

    if (m_leftPanel) {
        settings.setValue("LeftPanelCurrentIndex", m_leftPanel->currentIndex());
    }
    if (m_rightPanel) {
        settings.setValue("RightPanelCurrentIndex", m_rightPanel->currentIndex());
    }
    if (m_bottomPanel) {
        settings.setValue("BottomPanelCurrentIndex", m_bottomPanel->currentIndex());
        settings.setValue("BottomPanelVisible", m_bottomPanel->isVisible());
    }
    
    settings.endGroup();
}

void TabPanelManager::RestorePanelState(QSettings& settings, const QString& group)
{
    settings.beginGroup(group);
    
    if (m_verticalSplitter) {
        const QByteArray data = settings.value("VerticalSplitterState").toByteArray();
        if (!data.isEmpty()) {
            m_verticalSplitter->restoreState(data);
        }
    }

    if (m_horizontalSplitter) {
        const QByteArray data = settings.value("HorizontalSplitterState").toByteArray();
        if (!data.isEmpty()) {
            m_horizontalSplitter->restoreState(data);
        }
    }

    if (m_leftPanel) {
        int index = settings.value("LeftPanelCurrentIndex", 0).toInt();
        if (index >= 0 && index < m_leftPanel->count()) {
            m_leftPanel->setCurrentIndex(index);
        }
    }
    if (m_rightPanel) {
        int index = settings.value("RightPanelCurrentIndex", 0).toInt();
        if (index >= 0 && index < m_rightPanel->count()) {
            m_rightPanel->setCurrentIndex(index);
        }
    }
    if (m_bottomPanel) {
        int index = settings.value("BottomPanelCurrentIndex", 0).toInt();
        if (index >= 0 && index < m_bottomPanel->count()) {
            m_bottomPanel->setCurrentIndex(index);
        }
        bool visible = settings.value("BottomPanelVisible", true).toBool();
        m_bottomPanel->setVisible(visible);
    }
    
    settings.endGroup();
}

void TabPanelManager::SetupPanelStyle()
{
    // Apply a clean, modern stylesheet to all panels
    for (auto* tabWidget : { m_leftPanel, m_rightPanel, m_bottomPanel }) {
        if (tabWidget) {
            tabWidget->setStyleSheet(
                "QTabWidget::pane { "
                "    border-top: 1px solid #242c3a; "
                "    background-color: #131722; "
                "} "
                "QTabBar::tab { "
                "    background-color: #1b2230; "
                "    color: #b6beca; "
                "    padding: 8px 16px; "
                "    border: 1px solid #2a3140; "
                "    border-bottom: none; "
                "    border-top-left-radius: 6px; "
                "    border-top-right-radius: 6px; "
                "    margin-right: 4px; "
                "    min-width: 88px; "
                "} "
                "QTabBar::tab:hover { "
                "    background-color: #2a3344; "
                "    color: #e6e3dc; "
                "} "
                "QTabBar::tab:selected { "
                "    background-color: #ff6b3d; "
                "    color: #141824; "
                "    border-color: #ff8b66; "
                "}"
            );
        }
    }
}

} // namespace Aetherion::Editor
